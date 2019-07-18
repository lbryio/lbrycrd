
#include <claimtrie.h>
#include <coins.h>
#include <hash.h>
#include <logging.h>
#include "util.h"

#include <algorithm>
#include <memory>

#include <boost/scoped_ptr.hpp>

static const uint256 one = uint256S("0000000000000000000000000000000000000000000000000000000000000001");

std::vector<unsigned char> heightToVch(int n)
{
    std::vector<unsigned char> vchHeight(8, 0);
    vchHeight[4] = n >> 24;
    vchHeight[5] = n >> 16;
    vchHeight[6] = n >> 8;
    vchHeight[7] = n;
    return vchHeight;
}

uint256 getValueHash(const COutPoint& outPoint, int nHeightOfLastTakeover)
{
    CHash256 hasher;
    auto hash = Hash(outPoint.hash.begin(), outPoint.hash.end());
    hasher.Write(hash.begin(), hash.size());

    auto snOut = std::to_string(outPoint.n);
    hash = Hash(snOut.begin(), snOut.end());
    hasher.Write(hash.begin(), hash.size());

    auto vchHash = heightToVch(nHeightOfLastTakeover);
    hash = Hash(vchHash.begin(), vchHash.end());
    hasher.Write(hash.begin(), hash.size());

    uint256 result;
    hasher.Finalize(result.begin());
    return result;
}

bool CClaimTrieData::insertClaim(const CClaimValue& claim)
{
    claims.push_back(claim);
    return true;
}

bool CClaimTrieData::removeClaim(const COutPoint& outPoint, CClaimValue& claim)
{
    for (auto iClaim = claims.begin(); iClaim != claims.end(); ++iClaim) {
        if (iClaim->outPoint == outPoint) {
            std::swap(claim, *iClaim);
            claims.erase(iClaim);
            return true;
        }
    }
    LogPrintf("CClaimTrieData::%s() : asked to remove a claim that doesn't exist\n", __func__);
    LogPrintf("CClaimTrieData::%s() : claims that do exist:\n", __func__);
    for (auto& iClaim : claims)
        LogPrintf("\t%s\n", iClaim.outPoint.ToString());
    return false;
}

bool CClaimTrieData::getBestClaim(CClaimValue& claim) const
{
    if (claims.empty())
        return false;
    claim = claims.front();
    return true;
}

bool CClaimTrieData::haveClaim(const COutPoint& outPoint) const
{
    return std::any_of(claims.begin(), claims.end(),
        [&outPoint](const CClaimValue& claim) { return claim.outPoint == outPoint; });
}

void CClaimTrieData::reorderClaims(const supportEntryType& supports)
{
    for (auto& claim : claims) {
        claim.nEffectiveAmount = claim.nAmount;
        for (const auto& support : supports)
            if (support.supportedClaimId == claim.claimId)
                claim.nEffectiveAmount += support.nAmount;
    }

    std::make_heap(claims.begin(), claims.end());
}

CClaimTrie::CClaimTrie(bool fMemory, bool fWipe, int proportionalDelayFactor)
{
    nProportionalDelayFactor = proportionalDelayFactor;
    nExpirationTime = Params().GetConsensus().nOriginalClaimExpirationTime;
    db.reset(new CDBWrapper(GetDataDir() / "claimtrie", 100 * 1024 * 1024, fMemory, fWipe, false));
}

bool CClaimTrie::SyncToDisk()
{
    if (db)
        return db->Sync();
    return false;
}

template <typename Key, typename Container>
typename Container::value_type* getQueue(CDBWrapper& db, uint8_t dbkey, const Key& key, Container& queue, bool create)
{
    auto itQueue = queue.find(key);
    if (itQueue != queue.end())
        return &(*itQueue);
    typename Container::mapped_type row;
    if (db.Read(std::make_pair(dbkey, key), row) || create) {
        auto ret = queue.insert(std::make_pair(key, row));
        assert(ret.second);
        return &(*ret.first);
    }
    return nullptr;
}

uint256 computeHash(const std::vector<uint8_t>& vec)
{
    return Hash(vec.begin(), vec.end());
}

typename claimQueueType::value_type* CClaimTrieCacheBase::getQueueCacheRow(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), CLAIM_QUEUE_ROW, nHeight, claimQueueCache, createIfNotExists);
}

typename queueNameType::value_type* CClaimTrieCacheBase::getQueueCacheNameRow(const std::string& name, bool createIfNotExists)
{
    return getQueue(*(base->db), CLAIM_QUEUE_NAME_ROW, name, claimQueueNameCache, createIfNotExists);
}

typename expirationQueueType::value_type* CClaimTrieCacheBase::getExpirationQueueCacheRow(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), EXP_QUEUE_ROW, nHeight, expirationQueueCache, createIfNotExists);
}

typename supportQueueType::value_type* CClaimTrieCacheBase::getSupportQueueCacheRow(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), SUPPORT_QUEUE_ROW, nHeight, supportQueueCache, createIfNotExists);
}

typename queueNameType::value_type* CClaimTrieCacheBase::getSupportQueueCacheNameRow(const std::string& name, bool createIfNotExists)
{
    return getQueue(*(base->db), SUPPORT_QUEUE_NAME_ROW, name, supportQueueNameCache, createIfNotExists);
}

typename expirationQueueType::value_type* CClaimTrieCacheBase::getSupportExpirationQueueCacheRow(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), SUPPORT_EXP_QUEUE_ROW, nHeight, supportExpirationQueueCache, createIfNotExists);
}

bool CClaimTrieCacheBase::haveClaim(const std::string& name, const COutPoint& outPoint) const
{
    auto it = find(name);
    return it && it->haveClaim(outPoint);
}

supportEntryType CClaimTrieCacheBase::getSupportsForName(const std::string& name) const
{
    auto sit = cacheSupports.find(name);
    if (sit != cacheSupports.end())
        return sit->second;

    supportEntryType supports;
    if (base->db->Read(std::make_pair(SUPPORT, name), supports)) // don't trust the try/catch in here
        return supports;
    return {};
}

bool CClaimTrieCacheBase::haveSupport(const std::string& name, const COutPoint& outPoint) const
{
    auto supports = getSupportsForName(name);
    return std::any_of(supports.begin(), supports.end(),
        [&outPoint](const CSupportValue& support) { return support.outPoint == outPoint; });
}

bool CClaimTrieCacheBase::haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight)
{
    auto nameRow = getQueueCacheNameRow(name);
    if (!nameRow) {
        return false;
    }
    for (const auto& iNameRow : nameRow->second) {
        if (iNameRow.outPoint != outPoint)
            continue;
        nValidAtHeight = iNameRow.nHeight;
        if (auto cacheRow = getQueueCacheRow(nValidAtHeight)) {
            for (const auto& iRow : cacheRow->second) {
                if (iRow.first == name && iRow.second.outPoint == outPoint) {
                    if (iRow.second.nValidAtHeight != nValidAtHeight)
                        LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, iRow.second.nValidAtHeight, nNextHeight);
                    return true;
                }
            }
        }
        break;
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nNextHeight);
    return false;
}

bool CClaimTrieCacheBase::haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight)
{
    auto nameRow = getSupportQueueCacheNameRow(name);
    if (!nameRow) {
        return false;
    }
    for (const auto& itNameRow : nameRow->second) {
        if (itNameRow.outPoint != outPoint)
            continue;
        nValidAtHeight = itNameRow.nHeight;
        if (auto row = getSupportQueueCacheRow(nValidAtHeight)) {
            for (const auto& iRow : row->second) {
                if (iRow.first == name && iRow.second.outPoint == outPoint) {
                    if (iRow.second.nValidAtHeight != nValidAtHeight)
                        LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, iRow.second.nValidAtHeight, nNextHeight);
                    return true;
                }
            }
        }
        break;
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nNextHeight);
    return false;
}

std::size_t CClaimTrieCacheBase::getTotalNamesInTrie() const
{
    size_t count = 0;
    for (auto it = base->cbegin(); it != base->cend(); ++it) {
        if (!it->claims.empty())
            ++count;
    }
    return count;
}

std::size_t CClaimTrieCacheBase::getTotalClaimsInTrie() const
{
    std::size_t count = 0;
    for (auto it = base->cbegin(); it != base->cend(); ++it) {
        count += it->claims.size();
    }
    return count;
}

CAmount CClaimTrieCacheBase::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    CAmount value_in_subtrie = 0;
    for (auto it = base->cbegin(); it != base->cend(); ++it) {
        for (const auto& claim : it->claims) {
            value_in_subtrie += claim.nAmount;
            if (fControllingOnly)
                break;
        }
    }
    return value_in_subtrie;
}

bool CClaimTrieCacheBase::getInfoForName(const std::string& name, CClaimValue& claim) const
{
    auto it = find(name);
    return it && it->getBestClaim(claim);
}

CClaimsForNameType CClaimTrieCacheBase::getClaimsForName(const std::string& name) const
{
    claimEntryType claims;
    int nLastTakeoverHeight = 0;
    auto supports = getSupportsForName(name);

    if (auto it = find(name)) {
        claims = it->claims;
        nLastTakeoverHeight = it->nHeightOfLastTakeover;
    }
    return {std::move(claims), std::move(supports), nLastTakeoverHeight, name};
}

CAmount CClaimTrieCacheBase::getEffectiveAmountForClaim(const std::string& name, const uint160& claimId, std::vector<CSupportValue>* supports) const
{
    return getEffectiveAmountForClaim(getClaimsForName(name), claimId, supports);
}

CAmount CClaimTrieCacheBase::getEffectiveAmountForClaim(const CClaimsForNameType& claims, const uint160& claimId, std::vector<CSupportValue>* supports) const
{
    CAmount effectiveAmount = 0;
    for (const auto& claim : claims.claims) {
        if (claim.claimId == claimId && claim.nValidAtHeight < nNextHeight) {
            effectiveAmount += claim.nAmount;
            for (const auto& support : claims.supports) {
                if (support.supportedClaimId == claimId && support.nValidAtHeight < nNextHeight) {
                    effectiveAmount += support.nAmount;
                    if (supports) supports->push_back(support);
                }
            }
            break;
        }
    }
    return effectiveAmount;
}

void completeHash(uint256& partialHash, const std::string& key, std::size_t to)
{
    CHash256 hasher;
    for (auto i = key.size(); i > to + 1; --i, hasher.Reset())
        hasher
            .Write((uint8_t*)&key[i - 1], 1)
            .Write(partialHash.begin(), partialHash.size())
            .Finalize(partialHash.begin());
}

bool recursiveCheckConsistency(CClaimTrie::const_iterator& it, int minimumHeight, std::string& failed)
{
    std::vector<uint8_t> vchToHash;

    const auto pos = it.key().size();
    auto children = it.children();
    for (auto& child : children) {
        if (!recursiveCheckConsistency(child, minimumHeight, failed))
            return false;
        auto& key = child.key();
        auto hash = child->hash;
        vchToHash.push_back(key[pos]);
        completeHash(hash, key, pos);
        vchToHash.insert(vchToHash.end(), hash.begin(), hash.end());
    }

    CClaimValue claim;
    if (it->getBestClaim(claim)) {
        if (it->nHeightOfLastTakeover < minimumHeight) {
            LogPrintf("\nInvalid takeover height for %s\n", it.key());
            failed = it.key();
            return false;
        }
        uint256 valueHash = getValueHash(claim.outPoint, it->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    } else if (children.empty()) {
        // we don't allow a situation of no children and no claims; no empty leaf nodes allowed
        LogPrintf("\nInvalid empty node for %s\n", it.key());
        failed = it.key();
        return false;
    }

    auto calculatedHash = computeHash(vchToHash);
    auto matched = calculatedHash == it->hash;
    if (!matched) {
        LogPrintf("\nComputed hash doesn't match stored hash for %s\n", it.key());
        failed = it.key();
    }
    return matched;
}

bool CClaimTrieCacheBase::checkConsistency(int minimumHeight) const
{
    if (base->empty())
        return true;

    auto it = base->cbegin();
    std::string failed;
    auto consistent = recursiveCheckConsistency(it, minimumHeight, failed);
    if (!consistent) {
        LogPrintf("Printing base tree from its parent:\n", failed);
        auto basePath = base->nodes(failed);
        if (basePath.size() > 1) basePath.pop_back();
        dumpToLog(basePath.back(), false);
        auto cachePath = cache.nodes(failed);
        if (!cachePath.empty()) {
            LogPrintf("\nPrinting %s's parent from cache:\n", failed);
            if (cachePath.size() > 1) cachePath.pop_back();
            dumpToLog(cachePath.back(), false);
        }
        if (!nodesToDelete.empty()) {
            std::string joined;
            for (const auto &piece : nodesToDelete) joined += ", " + piece;
            LogPrintf("Nodes to be deleted: %s\n", joined.substr(2));
        }
    }
    return consistent;
}

bool CClaimTrieCacheBase::getClaimById(const uint160& claimId, std::string& name, CClaimValue& claim) const
{
    CClaimIndexElement element;
    if (!base->db->Read(std::make_pair(CLAIM_BY_ID, claimId), element))
        return false;
    if (element.claim.claimId == claimId) {
        name = element.name;
        claim = element.claim;
        return true;
    }
    LogPrintf("%s: ClaimIndex[%s] returned unmatched claimId %s when looking for %s\n", __func__, claimId.GetHex(), element.claim.claimId.GetHex(), name);
    return false;
}

template <typename K, typename T>
void BatchWrite(CDBBatch& batch, uint8_t dbkey, const K& key, const std::vector<T>& value)
{
    if (value.empty()) {
        batch.Erase(std::make_pair(dbkey, key));
    } else {
        batch.Write(std::make_pair(dbkey, key), value);
    }
}

template <typename Container>
void BatchWriteQueue(CDBBatch& batch, uint8_t dbkey, const Container& queue)
{
    for (auto& itQueue : queue)
        BatchWrite(batch, dbkey, itQueue.first, itQueue.second);
}

bool CClaimTrieCacheBase::flush()
{
    CDBBatch batch(*(base->db));

    for (const auto& claim : claimsToDelete) {
        auto it = std::find_if(claimsToAdd.begin(), claimsToAdd.end(),
            [&claim](const CClaimIndexElement& e) {
                return e.claim.claimId == claim.claimId;
            }
        );
        if (it == claimsToAdd.end())
            batch.Erase(std::make_pair(CLAIM_BY_ID, claim.claimId));
    }

    for (const auto& e : claimsToAdd)
        batch.Write(std::make_pair(CLAIM_BY_ID, e.claim.claimId), e);

    getMerkleHash();

    for (const auto& nodeName : nodesToDelete) {
        if (cache.contains(nodeName))
            continue;
        auto nodes = base->nodes(nodeName);
        base->erase(nodeName);
        for (auto& node : nodes)
            if (!node)
                batch.Erase(std::make_pair(TRIE_NODE, node.key()));
    }

    for (auto it = cache.begin(); it != cache.end(); ++it) {
        auto old = base->find(it.key());
        if (!old || old.data() != it.data()) {
            base->copy(it);
            batch.Write(std::make_pair(TRIE_NODE, it.key()), it.data());
        }
    }

    BatchWriteQueue(batch, SUPPORT, cacheSupports);
    BatchWriteQueue(batch, CLAIM_QUEUE_ROW, claimQueueCache);
    BatchWriteQueue(batch, CLAIM_QUEUE_NAME_ROW, claimQueueNameCache);
    BatchWriteQueue(batch, EXP_QUEUE_ROW, expirationQueueCache);
    BatchWriteQueue(batch, SUPPORT_QUEUE_ROW, supportQueueCache);
    BatchWriteQueue(batch, SUPPORT_QUEUE_NAME_ROW, supportQueueNameCache);
    BatchWriteQueue(batch, SUPPORT_EXP_QUEUE_ROW, supportExpirationQueueCache);

    base->nNextHeight = nNextHeight;
    if (!cache.empty())
        LogPrintf("Cache size: %zu from base size: %zu on block %d\n", cache.height(), base->height(), nNextHeight);
    auto ret = base->db->WriteBatch(batch);
    clear();
    return ret;
}

bool CClaimTrieCacheBase::ReadFromDisk(const CBlockIndex* tip)
{
    LogPrintf("Loading the claim trie from disk...\n");

    nNextHeight = tip ? tip->nHeight + 1 : 0;
    base->nNextHeight = nNextHeight;
    base->nExpirationTime = Params().GetConsensus().GetExpirationTime(nNextHeight - 1); // -1 okay

    clear();
    base->clear();
    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());

    std::vector<std::pair<std::string, uint256>> hashesOnEmptyNodes;

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, std::string> key;
        if (!pcursor->GetKey(key) || key.first != TRIE_NODE)
            continue;

        CClaimTrieData data;
        if (pcursor->GetValue(data)) {
            if (data.empty()) {
                // we have a situation where our old trie had many empty nodes
                // we don't want to automatically throw those all into our prefix trie
                hashesOnEmptyNodes.emplace_back(key.second, data.hash);
                continue;
            }

            // nEffectiveAmount isn't serialized but it needs to be initialized (as done in reorderClaims):
            auto supports = getSupportsForName(key.second);
            data.reorderClaims(supports);
            base->insert(key.second, std::move(data));
        } else {
            return error("%s(): error reading claim trie from disk", __func__);
        }
    }

    CDBBatch batch(*(base->db));
    for (auto& kvp: hashesOnEmptyNodes) {
        auto hit = base->find(kvp.first);
        if (hit != base->end())
            hit->hash = kvp.second;
        else {
            // the first time the prefix trie is ran there will be many unused nodes
            // we need to clean those out so that we can go faster next time
            batch.Erase(std::make_pair(TRIE_NODE, kvp.first));
        }
    }

    LogPrintf("Checking claim trie consistency... ");
    if (checkConsistency()) {
        LogPrintf("consistent\n");
        if (tip && tip->hashClaimTrie != getMerkleHash())
            return error("%s(): hashes don't match when reading claimtrie from disk", __func__);
        base->db->WriteBatch(batch);
        return true;
    }
    LogPrintf("inconsistent!\n");
    return false;
}

CClaimTrieCacheBase::CClaimTrieCacheBase(CClaimTrie* base, bool fRequireTakeoverHeights) : base(base), fRequireTakeoverHeights(fRequireTakeoverHeights)
{
    assert(base);
    nNextHeight = base->nNextHeight;
}

void CClaimTrieCacheBase::setExpirationTime(int time)
{
    base->nExpirationTime = time;
}

int CClaimTrieCacheBase::expirationTime()
{
    return base->nExpirationTime;
}

uint256 CClaimTrieCacheBase::recursiveComputeMerkleHash(CClaimTrie::iterator& it)
{
    if (!it->hash.IsNull())
        return it->hash;

    std::vector<uint8_t> vchToHash;
    const auto pos = it.key().size();
    for (auto& child : it.children()) {
        auto& key = child.key();
        vchToHash.push_back(key[pos]);
        auto hash = recursiveComputeMerkleHash(child);
        assert(!hash.IsNull());
        completeHash(hash, key, pos);
        vchToHash.insert(vchToHash.end(), hash.begin(), hash.end());
    }

    CClaimValue claim;
    if (it->getBestClaim(claim)) {
        uint256 valueHash = getValueHash(claim.outPoint, it->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    return it->hash = computeHash(vchToHash);
}

uint256 CClaimTrieCacheBase::getMerkleHash()
{
    auto it = cache.begin();
    if (cache.empty() && nodesToDelete.empty())
        it = base->begin();
    return !it ? one : recursiveComputeMerkleHash(it);
}

CClaimTrie::const_iterator CClaimTrieCacheBase::begin() const
{
    return cache.empty() && nodesToDelete.empty() ? base->cbegin() : cache.begin();
}

CClaimTrie::const_iterator CClaimTrieCacheBase::end() const
{
    return cache.empty() && nodesToDelete.empty() ? base->cend() : cache.end();
}

CClaimTrie::const_iterator CClaimTrieCacheBase::find(const std::string& name) const
{
    if (auto it = cache.find(name))
        return it;
    return base->find(name);
}

bool CClaimTrieCacheBase::empty() const
{
    return base->empty() && cache.empty();
}

CClaimTrie::iterator CClaimTrieCacheBase::cacheData(const std::string& name, bool create)
{
    // get data from the cache. if no data, create empty one
    const auto insert = [this](CClaimTrie::iterator& it) {
        auto& key = it.key();
        // we only ever cache nodes once per cache instance
        if (!alreadyCachedNodes.count(key)) {
            // do not insert nodes that are already present
            alreadyCachedNodes.insert(key);
            cache.insert(key, it.data());
        }
    };

    // we need all parent nodes and their one level deep children
    // to calculate merkle hash
    auto nodes = base->nodes(name);
    for (auto& node: nodes) {
        for (auto& child : node.children())
            if (!alreadyCachedNodes.count(child.key()))
                cache.copy(child);
        insert(node);
    }

    auto it = cache.find(name);
    if (!it && create) {
        it = cache.insert(name, CClaimTrieData{});
        confirmTakeoverWorkaroundNeeded(name);
    }

    // make sure takeover height is updated
    if (it && it->nHeightOfLastTakeover <= 0) {
        uint160 unused;
        getLastTakeoverForName(name, unused, it->nHeightOfLastTakeover);
    }

    return it;
}

bool CClaimTrieCacheBase::getLastTakeoverForName(const std::string& name, uint160& claimId, int& takeoverHeight) const
{
    // takeoverCache always contains the most recent takeover occurring before the current block
    auto cit = takeoverCache.find(name);
    if (cit != takeoverCache.end()) {
        std::tie(claimId, takeoverHeight) = cit->second;
        return true;
    }
    if (auto it = base->find(name)) {
        takeoverHeight = it->nHeightOfLastTakeover;
        CClaimValue claim;
        if (it->getBestClaim(claim))
        {
            claimId = claim.claimId;
            return true;
        }
    }
    return false;
}

void CClaimTrieCacheBase::markAsDirty(const std::string& name, bool fCheckTakeover)
{
    for (auto& node : cache.nodes(name))
        node->hash.SetNull();

    if (fCheckTakeover)
        namesToCheckForTakeover.insert(name);
}

bool CClaimTrieCacheBase::insertClaimIntoTrie(const std::string& name, const CClaimValue& claim, bool fCheckTakeover)
{
    auto it = cacheData(name);
    it->insertClaim(claim);
    auto supports = getSupportsForName(name);
    it->reorderClaims(supports);
    markAsDirty(name, fCheckTakeover);
    return true;
}

bool CClaimTrieCacheBase::removeClaimFromTrie(const std::string& name, const COutPoint& outPoint, CClaimValue& claim, bool fCheckTakeover)
{
    auto it = cacheData(name, false);

    if (!it || !it->removeClaim(outPoint, claim)) {
        LogPrintf("%s: Removing a claim was unsuccessful. name = %s, txhash = %s, nOut = %d", __func__, name, outPoint.hash.GetHex(), outPoint.n);
        return false;
    }

    if (!it->claims.empty()) {
        auto supports = getSupportsForName(name);
        it->reorderClaims(supports);
    } else {
        // in case we pull a child into our spot; we will then need their kids for hash
        bool hasChild = false;
        for (auto& child: it.children()) {
            hasChild = true;
            cacheData(child.key(), false);
        }

        cache.erase(name);
        nodesToDelete.insert(name);

        // NOTE: old code had a bug in it where nodes with no claims but with children would get left in the cache.
        // This would cause the getNumBlocksOfContinuousOwnership to return zero (causing incorrect takeover height calc).
        if (hasChild && nNextHeight < Params().GetConsensus().nMaxTakeoverWorkaroundHeight) {
            removalWorkaround.insert(name);
        }
    }

    markAsDirty(name, fCheckTakeover);
    return true;
}

bool CClaimTrieCacheBase::addClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight)
{
    assert(nHeight == nNextHeight);
    int delayForClaim = getDelayForName(name, claimId);
    CClaimValue claim(outPoint, claimId, nAmount, nHeight, nHeight + delayForClaim);
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, claimId: %s, nAmount: %d, nHeight: %d, nValidHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, claimId.GetHex(), nAmount, nHeight, claim.nValidAtHeight);
    addClaimToQueues(name, claim);
    CClaimIndexElement element = {name, claim};
    claimsToAdd.push_back(element);
    return true;
}

bool CClaimTrieCacheBase::undoSpendClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight, int nValidAtHeight)
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, claimId: %s, nAmount: %d, nHeight: %d, nValidAtHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, claimId.GetHex(), nAmount, nHeight, nValidAtHeight, nNextHeight);
    CClaimValue claim(outPoint, claimId, nAmount, nHeight, nValidAtHeight);
    if (nValidAtHeight < nNextHeight) {
        CNameOutPointType entry(adjustNameForValidHeight(name, nValidAtHeight), claim.outPoint);
        addToExpirationQueue(claim.nHeight + base->nExpirationTime, entry);
        CClaimIndexElement element = {name, claim};
        claimsToAdd.push_back(element);
        return insertClaimIntoTrie(name, claim, false);
    }
    addClaimToQueues(name, claim);
    CClaimIndexElement element = {name, claim};
    claimsToAdd.push_back(element);
    return true;
}

bool CClaimTrieCacheBase::addClaimToQueues(const std::string& name, const CClaimValue& claim)
{
    claimQueueEntryType entry(name, claim);
    auto itQueueRow = getQueueCacheRow(claim.nValidAtHeight, true);
    auto itQueueNameRow = getQueueCacheNameRow(name, true);
    itQueueRow->second.push_back(entry);
    itQueueNameRow->second.emplace_back(claim.outPoint, claim.nValidAtHeight);
    CNameOutPointType expireEntry(name, claim.outPoint);
    addToExpirationQueue(claim.nHeight + base->nExpirationTime, expireEntry);
    return true;
}

bool CClaimTrieCacheBase::removeClaimFromQueue(const std::string& name, const COutPoint& outPoint, CClaimValue& claim)
{
    auto itQueueNameRow = getQueueCacheNameRow(name);
    if (!itQueueNameRow) {
        return false;
    }

    auto itQueueName = itQueueNameRow->second.cbegin();
    for (; itQueueName != itQueueNameRow->second.cend(); ++itQueueName) {
        if (itQueueName->outPoint == outPoint) {
            if (auto itQueueRow = getQueueCacheRow(itQueueName->nHeight)) {
                auto itQueue = itQueueRow->second.begin();
                for (; itQueue != itQueueRow->second.end(); ++itQueue) {
                    if (outPoint == itQueue->second.outPoint && name == itQueue->first) {
                        std::swap(claim, itQueue->second);
                        itQueueNameRow->second.erase(itQueueName);
                        itQueueRow->second.erase(itQueue);
                        return true;
                    }
                }
            }
            break;
        }
    }
    if (itQueueName != itQueueNameRow->second.cend())
        LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, itQueueName->nHeight, nNextHeight);
    return false;
}

bool CClaimTrieCacheBase::undoAddClaim(const std::string& name, const COutPoint& outPoint, int nHeight)
{
    int throwaway;
    return removeClaim(name, outPoint, nHeight, throwaway, false);
}

bool CClaimTrieCacheBase::spendClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight)
{
    return removeClaim(name, outPoint, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCacheBase::removeClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover)
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %u, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nNextHeight);

    CClaimValue claim;
    nValidAtHeight = nHeight + getDelayForName(name);
    std::string adjusted = adjustNameForValidHeight(name, nValidAtHeight);

    if (removeClaimFromQueue(adjusted, outPoint, claim) || removeClaimFromTrie(name, outPoint, claim, fCheckTakeover)) {
        int expirationHeight = claim.nHeight + base->nExpirationTime;
        removeFromExpirationQueue(adjusted, outPoint, expirationHeight);
        claimsToDelete.insert(claim);
        nValidAtHeight = claim.nValidAtHeight;
        return true;
    }
    return false;
}

void CClaimTrieCacheBase::addToExpirationQueue(int nExpirationHeight, CNameOutPointType& entry)
{
    auto itQueueRow = getExpirationQueueCacheRow(nExpirationHeight, true);
    itQueueRow->second.push_back(entry);
}

void CClaimTrieCacheBase::removeFromExpirationQueue(const std::string& name, const COutPoint& outPoint, int expirationHeight)
{
    if (auto itQueueRow = getExpirationQueueCacheRow(expirationHeight)) {
        auto itQueue = itQueueRow->second.cbegin();
        for (; itQueue != itQueueRow->second.cend(); ++itQueue) {
            if (name == itQueue->name && outPoint == itQueue->outPoint) {
                itQueueRow->second.erase(itQueue);
                break;
            }
        }
    }
}

bool CClaimTrieCacheBase::insertSupportIntoMap(const std::string& name, const CSupportValue& support, bool fCheckTakeover)
{
    auto sit = cacheSupports.find(name);
    if (sit == cacheSupports.end()) {
        sit = cacheSupports.emplace(name, getSupportsForName(name)).first;
    }
    sit->second.push_back(support);

    addTakeoverWorkaroundPotential(name);

    if (auto it = cacheData(name, false)) {
        markAsDirty(name, fCheckTakeover);
        it->reorderClaims(sit->second);
    }

    return true;
}

bool CClaimTrieCacheBase::removeSupportFromMap(const std::string& name, const COutPoint& outPoint, CSupportValue& support, bool fCheckTakeover)
{
    auto sit = cacheSupports.find(name);
    if (sit == cacheSupports.end()) {
        sit = cacheSupports.emplace(name, getSupportsForName(name)).first;
    }

    for (auto it = sit->second.begin(); it != sit->second.end(); ++it) {
        if (it->outPoint == outPoint) {
            support = *it;
            sit->second.erase(it);

            addTakeoverWorkaroundPotential(name);

            if (auto dit = cacheData(name, false)) {
                markAsDirty(name, fCheckTakeover);
                dit->reorderClaims(sit->second);
            }
            return true;
        }
    }

    LogPrintf("CClaimTrieCacheBase::%s() : asked to remove a support that doesn't exist for %s#%s\n",
            __func__, name, outPoint.ToString());
    return false;
}

bool CClaimTrieCacheBase::addSupportToQueues(const std::string& name, const CSupportValue& support)
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, support.nValidAtHeight);
    supportQueueEntryType entry(name, support);
    auto itQueueRow = getSupportQueueCacheRow(support.nValidAtHeight, true);
    auto itQueueNameRow = getSupportQueueCacheNameRow(name, true);
    itQueueRow->second.push_back(entry);
    itQueueNameRow->second.emplace_back(support.outPoint, support.nValidAtHeight);
    CNameOutPointType expireEntry(name, support.outPoint);
    addSupportToExpirationQueue(support.nHeight + base->nExpirationTime, expireEntry);
    return true;
}

bool CClaimTrieCacheBase::removeSupportFromQueue(const std::string& name, const COutPoint& outPoint, CSupportValue& support)
{
    auto itQueueNameRow = getSupportQueueCacheNameRow(name);
    if (!itQueueNameRow) {
        return false;
    }

    auto itQueueName = itQueueNameRow->second.cbegin();
    for (; itQueueName != itQueueNameRow->second.cend(); ++itQueueName) {
        if (itQueueName->outPoint == outPoint) {
            if (auto itQueueRow = getSupportQueueCacheRow(itQueueName->nHeight)) {
                auto itQueue = itQueueRow->second.begin();
                for (; itQueue != itQueueRow->second.end(); ++itQueue) {
                    if (outPoint == itQueue->second.outPoint && name == itQueue->first) {
                        std::swap(support, itQueue->second);
                        itQueueNameRow->second.erase(itQueueName);
                        itQueueRow->second.erase(itQueue);
                        return true;
                    }
                }
            }
            break;
        }
    }

    if (itQueueName != itQueueNameRow->second.cend())
        LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named support queue but not in height support queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, itQueueName->nHeight, nNextHeight);
    return false;
}

bool CClaimTrieCacheBase::addSupport(const std::string& name, const COutPoint& outPoint, CAmount nAmount, const uint160& supportedClaimId, int nHeight)
{
    assert(nHeight == nNextHeight);
    int delayForSupport = getDelayForName(name, supportedClaimId);
    CSupportValue support(outPoint, supportedClaimId, nAmount, nHeight, nHeight + delayForSupport);
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedClaimId: %s, nHeight: %d, nValidHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nAmount, supportedClaimId.GetHex(), nHeight, support.nValidAtHeight);
    return addSupportToQueues(name, support);
}

bool CClaimTrieCacheBase::undoSpendSupport(const std::string& name, const COutPoint& outPoint, const uint160& supportedClaimId, CAmount nAmount, int nHeight, int nValidAtHeight)
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedClaimId: %s, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nAmount, supportedClaimId.GetHex(), nHeight, nNextHeight);
    CSupportValue support(outPoint, supportedClaimId, nAmount, nHeight, nValidAtHeight);
    if (nValidAtHeight < nNextHeight) {
        CNameOutPointType entry(adjustNameForValidHeight(name, nValidAtHeight), support.outPoint);
        addSupportToExpirationQueue(support.nHeight + base->nExpirationTime, entry);
        return insertSupportIntoMap(name, support, false);
    } else {
        return addSupportToQueues(name, support);
    }
}

bool CClaimTrieCacheBase::removeSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover)
{
    CSupportValue support;
    nValidAtHeight = nHeight + getDelayForName(name);
    std::string adjusted = adjustNameForValidHeight(name, nValidAtHeight);

    if (removeSupportFromQueue(adjusted, outPoint, support) || removeSupportFromMap(name, outPoint, support, fCheckTakeover)) {
        int expirationHeight = support.nHeight + base->nExpirationTime;
        removeSupportFromExpirationQueue(adjusted, outPoint, expirationHeight);
        nValidAtHeight = support.nValidAtHeight;
        return true;
    }
    return false;
}

void CClaimTrieCacheBase::addSupportToExpirationQueue(int nExpirationHeight, CNameOutPointType& entry)
{
    auto itQueueRow = getSupportExpirationQueueCacheRow(nExpirationHeight, true);
    itQueueRow->second.push_back(entry);
}

void CClaimTrieCacheBase::removeSupportFromExpirationQueue(const std::string& name, const COutPoint& outPoint, int expirationHeight)
{
    if (auto itQueueRow = getSupportExpirationQueueCacheRow(expirationHeight)) {
        auto itQueue = itQueueRow->second.cbegin();
        for (; itQueue != itQueueRow->second.end(); ++itQueue) {
            if (name == itQueue->name && outPoint == itQueue->outPoint) {
                itQueueRow->second.erase(itQueue);
                break;
            }
        }
        if (itQueue != itQueueRow->second.end())
            itQueueRow->second.erase(itQueue);
    }
}

void CClaimTrieCacheBase::dumpToLog(CClaimTrie::const_iterator it, bool diffFromBase) const {
    if (diffFromBase) {
        auto hit = base->find(it.key());
        if (hit && hit->hash == it->hash)
            return;
    }

    std::string indent(it.depth(), ' ');
    auto children = it.children();
    auto empty = children.size() == 0 && it->claims.size() == 0;
    LogPrintf("%s%s, %s, %zu = %s,%s take: %d, kids: %zu\n", indent, it.key(), HexStr(it.key().begin(), it.key().end()),
            empty ? " empty," : "", it.depth(), it->hash.ToString(), it->nHeightOfLastTakeover, children.size());
    for (auto& claim: it->claims)
        LogPrintf("%s   claim: %s, %ld, %ld, %d, %d\n", indent, claim.claimId.ToString(), claim.nAmount, claim.nEffectiveAmount, claim.nHeight, claim.nValidAtHeight);
    auto supports = getSupportsForName(it.key());
    for (auto& support: supports)
        LogPrintf("%s   suprt: %s, %ld, %d, %d\n", indent, support.supportedClaimId.ToString(), support.nAmount, support.nHeight, support.nValidAtHeight);

    for (auto& child: it.children())
        dumpToLog(child, diffFromBase);
}

bool CClaimTrieCacheBase::undoAddSupport(const std::string& name, const COutPoint& outPoint, int nHeight)
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nNextHeight);
    int throwaway;
    return removeSupport(name, outPoint, nHeight, throwaway, false);
}

bool CClaimTrieCacheBase::spendSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight)
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nNextHeight);
    return removeSupport(name, outPoint, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCacheBase::shouldUseTakeoverWorkaround(const std::string& key) const {
    auto it = takeoverWorkaround.find(key);
    return it != takeoverWorkaround.end() && it->second;
}

void CClaimTrieCacheBase::addTakeoverWorkaroundPotential(const std::string& key) {
    // the old code would add to the cache using a shortcut in the add/removeSupport methods
    // this logic mimics the effects of that.
    // (and the shortcut would later lead to a miscalculation of the takeover height)
    if (nNextHeight > Params().GetConsensus().nMinTakeoverWorkaroundHeight
        && nNextHeight < Params().GetConsensus().nMaxTakeoverWorkaroundHeight
        && !cache.contains(key) && base->contains(key))
        takeoverWorkaround.emplace(key, false);
}
void CClaimTrieCacheBase::confirmTakeoverWorkaroundNeeded(const std::string& key) {
    // This is a super ugly hack to work around bug in old code.
    // The bug: un/support a name then update it. This will cause its takeover height to be reset to current.
    // This is because the old code with add to the cache without setting block originals when dealing in supports.
    // Disable this takeoverWorkaround stuff on a future hard fork.
    if (nNextHeight > Params().GetConsensus().nMinTakeoverWorkaroundHeight
        && nNextHeight < Params().GetConsensus().nMaxTakeoverWorkaroundHeight) {
        auto it = takeoverWorkaround.find(key);
        if (it != takeoverWorkaround.end())
            (*it).second = true;
    }
}


bool CClaimTrieCacheBase::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    if (auto itQueueRow = getQueueCacheRow(nNextHeight)) {
        for (const auto& itEntry : itQueueRow->second) {
            bool found = false;
            if (auto itQueueNameRow = getQueueCacheNameRow(itEntry.first)) {
                auto itQueueName = itQueueNameRow->second.cbegin();
                for (; itQueueName != itQueueNameRow->second.cend(); ++itQueueName) {
                    if (itQueueName->outPoint == itEntry.second.outPoint && itQueueName->nHeight == nNextHeight) {
                        itQueueNameRow->second.erase(itQueueName);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in height queue but not in named queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itEntry.first, itEntry.second.outPoint.hash.GetHex(), itEntry.second.outPoint.n, itEntry.second.nValidAtHeight, nNextHeight);
                    LogPrintf("Claims found for that name:\n");
                    for (const auto& itQueueName : itQueueNameRow->second)
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itQueueName.outPoint.hash.GetHex(), itQueueName.outPoint.n, itQueueName.nHeight);
                }
            } else {
                LogPrintf("No claims found for that name\n");
            }
            assert(found);
            insertClaimIntoTrie(itEntry.first, itEntry.second, true);
            insertUndo.emplace_back(itEntry.first, itEntry.second.outPoint, itEntry.second.nValidAtHeight);
        }
        itQueueRow->second.clear();
    }

    if (auto itExpirationRow = getExpirationQueueCacheRow(nNextHeight)) {
        for (const auto& itEntry : itExpirationRow->second) {
            CClaimValue claim;
            assert(removeClaimFromTrie(itEntry.name, itEntry.outPoint, claim, true));
            claimsToDelete.insert(claim);
            expireUndo.emplace_back(itEntry.name, claim);
            LogPrintf("Expiring claim %s: %s, nHeight: %d, nValidAtHeight: %d\n", claim.claimId.GetHex(), itEntry.name, claim.nHeight, claim.nValidAtHeight);
        }
        itExpirationRow->second.clear();
    }

    if (auto itSupportRow = getSupportQueueCacheRow(nNextHeight)) {
        for (const auto& itSupport : itSupportRow->second) {
            bool found = false;
            if (auto itSupportNameRow = getSupportQueueCacheNameRow(itSupport.first)) {
                auto itSupportName = itSupportNameRow->second.cbegin();
                for (; itSupportName != itSupportNameRow->second.cend(); ++itSupportName) {
                    if (itSupportName->outPoint == itSupport.second.outPoint && itSupportName->nHeight == itSupport.second.nValidAtHeight) {
                        itSupportNameRow->second.erase(itSupportName);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nFound in height queue but not in named queue: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itSupport.first, itSupport.second.outPoint.hash.GetHex(), itSupport.second.outPoint.n, itSupport.second.nValidAtHeight, nNextHeight);
                    LogPrintf("Supports found for that name:\n");
                    for (const auto& itSupportName : itSupportNameRow->second)
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itSupportName.outPoint.hash.GetHex(), itSupportName.outPoint.n, itSupportName.nHeight);
                }
            } else {
                LogPrintf("No support found for that name\n");
            }
            assert(found);
            insertSupportIntoMap(itSupport.first, itSupport.second, true);
            insertSupportUndo.emplace_back(itSupport.first, itSupport.second.outPoint, itSupport.second.nValidAtHeight);
        }
        itSupportRow->second.clear();
    }

    if (auto itSupportExpirationRow = getSupportExpirationQueueCacheRow(nNextHeight)) {
        for (const auto& itEntry : itSupportExpirationRow->second) {
            CSupportValue support;
            assert(removeSupportFromMap(itEntry.name, itEntry.outPoint, support, true));
            expireSupportUndo.emplace_back(itEntry.name, support);
            LogPrintf("Expiring support %s: %s, nHeight: %d, nValidAtHeight: %d\n", support.supportedClaimId.GetHex(), itEntry.name, support.nHeight, support.nValidAtHeight);
        }
        itSupportExpirationRow->second.clear();
    }
    // check each potentially taken over name to see if a takeover occurred.
    // if it did, then check the claim and support insertion queues for
    // the names that have been taken over, immediately insert all claim and
    // supports for those names, and stick them in the insertUndo or
    // insertSupportUndo vectors, with the nValidAtHeight they had prior to
    // this block.
    // Run through all names that have been taken over
    for (const auto& itNamesToCheck : namesToCheckForTakeover) {
        // Check if a takeover has occurred (only going to hit each name once)
        auto itCachedNode = cache.find(itNamesToCheck);
        // many possibilities
        // if this node is new, don't put it into the undo -- there will be nothing to restore, after all
        // if all of this node's claims were deleted, it should be put into the undo -- there could be
        // claims in the queue for that name and the takeover height should be the current height
        // if the node is not in the cache, or getbestclaim fails, that means all of its claims were
        // deleted
        // if getLastTakeoverForName returns false, that means it's new and shouldn't go into the undo
        // if both exist, and the current best claim is not the same as or the parent to the new best
        // claim, then ownership has changed and the current height of last takeover should go into
        // the queue
        CClaimValue claimInCache;
        bool haveClaimInCache = itCachedNode && itCachedNode->getBestClaim(claimInCache);
        uint160 ownersClaimId;
        int ownersTakeoverHeight = 0;
        bool haveClaimInTrie = getLastTakeoverForName(itNamesToCheck, ownersClaimId, ownersTakeoverHeight);
        bool takeoverHappened = !haveClaimInCache || !haveClaimInTrie || claimInCache.claimId != ownersClaimId;

        if (takeoverHappened) {
            // Get all pending claims for that name and activate them all in the case that our winner is defunct.
            if (auto itQueueNameRow = getQueueCacheNameRow(itNamesToCheck)) {
                for (const auto& itQueueName : itQueueNameRow->second) {
                    bool found = false;
                    // Pull those claims out of the height-based queue
                    if (auto itQueueRow = getQueueCacheRow(itQueueName.nHeight)) {
                        auto itQueue = itQueueRow->second.begin();
                        for (; itQueue != itQueueRow->second.end(); ++itQueue) {
                            if (itNamesToCheck == itQueue->first && itQueue->second.outPoint == itQueueName.outPoint && itQueue->second.nValidAtHeight == itQueueName.nHeight) {
                                // Insert them into the queue undo with their previous nValidAtHeight
                                insertUndo.emplace_back(itQueue->first, itQueue->second.outPoint, itQueue->second.nValidAtHeight);
                                // Insert them into the name trie with the new nValidAtHeight
                                itQueue->second.nValidAtHeight = nNextHeight;
                                insertClaimIntoTrie(itQueue->first, itQueue->second, false);
                                // Delete them from the height-based queue
                                itQueueRow->second.erase(itQueue);
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found)
                        LogPrintf("%s(): An inconsistency was found in the claim queue. Please report this to the developers:\nClaim found in name queue but not in height based queue:\nname: %s, txid: %s, nOut: %d, nValidAtHeight in name based queue: %d, current height: %d\n", __func__, itNamesToCheck, itQueueName.outPoint.hash.GetHex(), itQueueName.outPoint.n, itQueueName.nHeight, nNextHeight);
                    assert(found);
                }
                // remove all claims from the queue for that name
                itQueueNameRow->second.clear();
            }

            // Then, get all supports in the queue for that name
            if (auto itSupportQueueNameRow = getSupportQueueCacheNameRow(itNamesToCheck)) {
                for (const auto& itSupportQueueName : itSupportQueueNameRow->second) {
                    // Pull those supports out of the height-based queue
                    if (auto itSupportQueueRow = getSupportQueueCacheRow(itSupportQueueName.nHeight)) {
                        auto itSupportQueue = itSupportQueueRow->second.begin();
                        for (; itSupportQueue != itSupportQueueRow->second.end(); ++itSupportQueue) {
                            if (itNamesToCheck == itSupportQueue->first && itSupportQueue->second.outPoint == itSupportQueueName.outPoint && itSupportQueue->second.nValidAtHeight == itSupportQueueName.nHeight) {
                                // Insert them into the support queue undo with the previous nValidAtHeight
                                insertSupportUndo.emplace_back(itSupportQueue->first, itSupportQueue->second.outPoint, itSupportQueue->second.nValidAtHeight);
                                // Insert them into the support map with the new nValidAtHeight
                                itSupportQueue->second.nValidAtHeight = nNextHeight;
                                insertSupportIntoMap(itSupportQueue->first, itSupportQueue->second, false);
                                // Delete them from the height-based queue
                                itSupportQueueRow->second.erase(itSupportQueue);
                                break;
                            } else {
                                // here be problems TODO: show error, assert false
                            }
                        }
                    } else {
                        // here be problems
                    }
                }
                // remove all supports from the queue for that name
                itSupportQueueNameRow->second.clear();
            }
        }

        // not sure if this should happen above or below the above code:
        auto shouldUse = shouldUseTakeoverWorkaround(itNamesToCheck);
        if (!takeoverHappened && shouldUse)
            LogPrintf("TakeoverHeight workaround affects block: %d, name: %s, th: %d\n", nNextHeight, itNamesToCheck, ownersTakeoverHeight);
        takeoverHappened |= shouldUse;

        if (haveClaimInTrie && takeoverHappened)
            takeoverHeightUndo.emplace_back(itNamesToCheck, ownersTakeoverHeight);

        // some possible conditions:
        // 1. we added a new claim
        // 2. we updated a claim
        // 3. we had a claim fall out of the queue early and take over (or not)
        // 4. we removed a claim
        // 5. we got new supports and so a new claim took over (or not)
        // 6. we removed supports and so a new claim took over (or not)
        // claim removal is handled by "else" below
        // if there was a takeover, we set it to current height
        // if there was no takeover, we set it to old height if we have one
        // else set it to new height

        if ((itCachedNode = cache.find(itNamesToCheck))) {
            if (takeoverHappened) {
                itCachedNode->nHeightOfLastTakeover = nNextHeight;
                CClaimValue winner;
                if (itCachedNode->getBestClaim(winner))
                    takeoverCache[itNamesToCheck] = std::make_pair(winner.claimId, nNextHeight);
            }
            assert(itCachedNode->hash.IsNull());
        }
    }

    namesToCheckForTakeover.clear();
    takeoverWorkaround.clear();
    nNextHeight++;
    return true;
}

bool CClaimTrieCacheBase::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    nNextHeight--;

    if (!expireSupportUndo.empty()) {
        for (auto itSupportExpireUndo = expireSupportUndo.crbegin(); itSupportExpireUndo != expireSupportUndo.crend(); ++itSupportExpireUndo) {
            insertSupportIntoMap(itSupportExpireUndo->first, itSupportExpireUndo->second, false);
            if (nNextHeight == itSupportExpireUndo->second.nHeight + base->nExpirationTime) {
                auto itSupportExpireRow = getSupportExpirationQueueCacheRow(nNextHeight, true);
                itSupportExpireRow->second.emplace_back(itSupportExpireUndo->first, itSupportExpireUndo->second.outPoint);
            }
        }
    }

    for (auto itSupportUndo = insertSupportUndo.crbegin(); itSupportUndo != insertSupportUndo.crend(); ++itSupportUndo) {
        CSupportValue support;
        assert(removeSupportFromMap(itSupportUndo->name, itSupportUndo->outPoint, support, false));
        if (itSupportUndo->nHeight >= 0) {
            // support.nValidHeight may have been changed if this was inserted before activation height
            // due to a triggered takeover, change it back to original nValidAtHeight
            support.nValidAtHeight = itSupportUndo->nHeight;
            auto itSupportRow = getSupportQueueCacheRow(itSupportUndo->nHeight, true);
            auto itSupportNameRow = getSupportQueueCacheNameRow(itSupportUndo->name, true);
            itSupportRow->second.emplace_back(itSupportUndo->name, support);
            itSupportNameRow->second.emplace_back(support.outPoint, support.nValidAtHeight);
        }
    }

    if (!expireUndo.empty()) {
        for (auto itExpireUndo = expireUndo.crbegin(); itExpireUndo != expireUndo.crend(); ++itExpireUndo) {
            insertClaimIntoTrie(itExpireUndo->first, itExpireUndo->second, false);
            CClaimIndexElement element = {itExpireUndo->first, itExpireUndo->second};
            claimsToAdd.push_back(element);
            if (nNextHeight == itExpireUndo->second.nHeight + base->nExpirationTime) {
                auto itExpireRow = getExpirationQueueCacheRow(nNextHeight, true);
                itExpireRow->second.emplace_back(itExpireUndo->first, itExpireUndo->second.outPoint);
            }
        }
    }

    for (auto itInsertUndo = insertUndo.crbegin(); itInsertUndo != insertUndo.crend(); ++itInsertUndo) {
        CClaimValue claim;
        assert(removeClaimFromTrie(itInsertUndo->name, itInsertUndo->outPoint, claim, false));
        if (itInsertUndo->nHeight >= 0) { // aka it became valid at height rather than being rename/normalization
            // claim.nValidHeight may have been changed if this was inserted before activation height
            // due to a triggered takeover, change it back to original nValidAtHeight
            claim.nValidAtHeight = itInsertUndo->nHeight;
            auto itQueueRow = getQueueCacheRow(itInsertUndo->nHeight, true);
            auto itQueueNameRow = getQueueCacheNameRow(itInsertUndo->name, true);
            itQueueRow->second.emplace_back(itInsertUndo->name, claim);
            itQueueNameRow->second.emplace_back(itInsertUndo->outPoint, claim.nValidAtHeight);
        } else {
            claimsToDelete.insert(claim);
        }
    }

    return true;
}

bool CClaimTrieCacheBase::finalizeDecrement(std::vector<std::pair<std::string, int>>& takeoverHeightUndo) {
    for (auto itTakeoverHeightUndo = takeoverHeightUndo.crbegin(); itTakeoverHeightUndo != takeoverHeightUndo.crend(); ++itTakeoverHeightUndo) {
        auto it = cacheData(itTakeoverHeightUndo->first, false);
        if (it && itTakeoverHeightUndo->second) {
            it->nHeightOfLastTakeover = itTakeoverHeightUndo->second;
            CClaimValue winner;
            if (it->getBestClaim(winner)) {
                assert(itTakeoverHeightUndo->second <= nNextHeight);
                takeoverCache[itTakeoverHeightUndo->first] = std::make_pair(winner.claimId, itTakeoverHeightUndo->second);
            }
        }
    }

    return true;
}

int CClaimTrieCacheBase::getNumBlocksOfContinuousOwnership(const std::string& name)
{
    const auto hit = removalWorkaround.find(name);
    if (hit != removalWorkaround.end()) {
        removalWorkaround.erase(hit);
        return 0;
    }

    auto it = find(name);
    return it && !it->empty() ? nNextHeight - it->nHeightOfLastTakeover : 0;
}

int CClaimTrieCacheBase::getDelayForName(const std::string& name)
{
    if (!fRequireTakeoverHeights)
        return 0;
    int nBlocksOfContinuousOwnership = getNumBlocksOfContinuousOwnership(name);
    return std::min(nBlocksOfContinuousOwnership / base->nProportionalDelayFactor, 4032);
}

int CClaimTrieCacheBase::getDelayForName(const std::string& name, const uint160& claimId)
{
    uint160 winningClaimId;
    int winningTakeoverHeight;
    if (getLastTakeoverForName(name, winningClaimId, winningTakeoverHeight) && winningClaimId == claimId) {
        assert(winningTakeoverHeight <= nNextHeight);
        return 0;
    } else {
        return getDelayForName(name);
    }
}

std::string CClaimTrieCacheBase::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return name;
}

bool CClaimTrieCacheBase::clear()
{
    cache.clear();
    claimsToAdd.clear();
    cacheSupports.clear();
    nodesToDelete.clear();
    claimsToDelete.clear();
    takeoverCache.clear();
    claimQueueCache.clear();
    supportQueueCache.clear();
    alreadyCachedNodes.clear();
    takeoverWorkaround.clear();
    removalWorkaround.clear();
    claimQueueNameCache.clear();
    expirationQueueCache.clear();
    supportQueueNameCache.clear();
    namesToCheckForTakeover.clear();
    supportExpirationQueueCache.clear();
    return true;
}

bool CClaimTrieCacheBase::getProofForName(const std::string& name, CClaimTrieProof& proof)
{
    COutPoint outPoint;
    // cache the parent nodes
    cacheData(name, false);
    getMerkleHash();
    bool fNameHasValue = false;
    int nHeightOfLastTakeover = 0;
    std::vector<CClaimTrieProofNode> nodes;
    for (const auto& it : cache.nodes(name)) {
        CClaimValue claim;
        const auto& key = it.key();
        bool fNodeHasValue = it->getBestClaim(claim);
        uint256 valueHash;
        if (fNodeHasValue) {
            valueHash = getValueHash(claim.outPoint, it->nHeightOfLastTakeover);
        }
        const auto pos = key.size();
        std::vector<std::pair<unsigned char, uint256>> children;
        for (const auto& child : it.children()) {
            auto childKey = child.key();
            if (name.find(childKey) == 0) {
                for (auto i = pos; i + 1 < childKey.size(); ++i) {
                    children.emplace_back(childKey[i], uint256{});
                    nodes.emplace_back(std::move(children), fNodeHasValue, valueHash);
                    valueHash.SetNull();
                    fNodeHasValue = false;
                }
                children.emplace_back(childKey.back(), uint256{});
                continue;
            }
            auto hash = child->hash;
            completeHash(hash, childKey, pos);
            children.emplace_back(childKey[pos], hash);
        }
        if (key == name) {
            fNameHasValue = fNodeHasValue;
            if (fNameHasValue) {
                outPoint = claim.outPoint;
                nHeightOfLastTakeover = it->nHeightOfLastTakeover;
            }
            valueHash.SetNull();
        }
        nodes.emplace_back(std::move(children), fNodeHasValue, valueHash);
    }
    proof = CClaimTrieProof(std::move(nodes), fNameHasValue, outPoint, nHeightOfLastTakeover);
    return true;
}
