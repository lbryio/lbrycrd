
#include <claimtrie.h>
#include <coins.h>
#include <hash.h>
#include <logging.h>
#include <util.h>

#include <algorithm>
#include <memory>

extern const uint256 one = uint256S("0000000000000000000000000000000000000000000000000000000000000001");

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

template <typename T>
bool equals(const T& lhs, const T& rhs)
{
    return lhs == rhs;
}

template <typename T>
bool equals(const T& value, const COutPoint& outPoint)
{
    return value.outPoint == outPoint;
}

template <typename K, typename V>
bool equals(const std::pair<K, V>& pair, const CNameOutPointType& point)
{
    return pair.first == point.name && pair.second.outPoint == point.outPoint;
}

template <typename T, typename C>
auto findOutPoint(T& cont, const C& point) -> decltype(cont.begin())
{
    using type = typename T::value_type;
    static_assert(std::is_same<typename std::decay<T>::type, std::vector<type>>::value, "T should be a vector type");
    return std::find_if(cont.begin(), cont.end(), [&point](const type& val) {
        return equals(val, point);
    });
}

template <typename T, typename C>
bool eraseOutPoint(std::vector<T>& cont, const C& point, T* value = nullptr)
{
    auto it = findOutPoint(cont, point);
    if (it == cont.end())
        return false;
    if (value)
        std::swap(*it, *value);
    cont.erase(it);
    return true;
}

bool CClaimTrieData::insertClaim(const CClaimValue& claim)
{
    claims.push_back(claim);
    return true;
}

bool CClaimTrieData::removeClaim(const COutPoint& outPoint, CClaimValue& claim)
{
    if (eraseOutPoint(claims, outPoint, &claim))
        return true;

    if (LogAcceptCategory(BCLog::CLAIMS)) {
        LogPrintf("CClaimTrieData::%s() : asked to remove a claim that doesn't exist\n", __func__);
        LogPrintf("CClaimTrieData::%s() : claims that do exist:\n", __func__);
        for (auto& iClaim : claims)
            LogPrintf("\t%s\n", iClaim.outPoint.ToString());
    }
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
    return findOutPoint(claims, outPoint) != claims.end();
}

void CClaimTrieData::reorderClaims(const supportEntryType& supports)
{
    for (auto& claim : claims) {
        claim.nEffectiveAmount = claim.nAmount;
        for (const auto& support : supports)
            if (support.supportedClaimId == claim.claimId)
                claim.nEffectiveAmount += support.nAmount;
    }

    std::sort(claims.rbegin(), claims.rend());
}

CClaimTrie::CClaimTrie(bool fMemory, bool fWipe, int proportionalDelayFactor, std::size_t cacheMB)
{
    nProportionalDelayFactor = proportionalDelayFactor;
    db.reset(new CDBWrapper(GetDataDir() / "claimtrie", cacheMB * 1024ULL * 1024ULL, fMemory, fWipe, false));
}

bool CClaimTrie::SyncToDisk()
{
    return db && db->Sync();
}

template <typename T>
using rm_ref = typename std::remove_reference<T>::type;

template <typename Key, typename Map>
auto getRow(const CDBWrapper& db, uint8_t dbkey, const Key& key, Map& queue) -> COptional<rm_ref<decltype(queue.at(key))>>
{
    auto it = queue.find(key);
    if (it != queue.end())
        return {&(it->second)};
    typename Map::mapped_type row;
    if (db.Read(std::make_pair(dbkey, key), row))
        return {std::move(row)};
    return {};
}

template <typename Key, typename Value>
Value* getQueue(const CDBWrapper& db, uint8_t dbkey, const Key& key, std::map<Key, Value>& queue, bool create)
{
    auto row = getRow(db, dbkey, key, queue);
    if (row.unique() || (!row && create)) {
        auto ret = queue.emplace(key, row ? std::move(*row) : Value{});
        assert(ret.second);
        return &(ret.first->second);
    }
    return row;
}

template <typename T>
inline constexpr bool supportedType()
{
    static_assert(std::is_same<T, CClaimValue>::value || std::is_same<T, CSupportValue>::value, "T is unsupported type");
    return true;
}

template <>
std::vector<queueEntryType<CClaimValue>>* CClaimTrieCacheBase::getQueueCacheRow(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), CLAIM_QUEUE_ROW, nHeight, claimQueueCache, createIfNotExists);
}

template <>
std::vector<queueEntryType<CSupportValue>>* CClaimTrieCacheBase::getQueueCacheRow(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), SUPPORT_QUEUE_ROW, nHeight, supportQueueCache, createIfNotExists);
}

template <typename T>
std::vector<queueEntryType<T>>* CClaimTrieCacheBase::getQueueCacheRow(int, bool)
{
    supportedType<T>();
    return nullptr;
}

template <>
COptional<const std::vector<queueEntryType<CClaimValue>>> CClaimTrieCacheBase::getQueueCacheRow(int nHeight) const
{
    return getRow(*(base->db), CLAIM_QUEUE_ROW, nHeight, claimQueueCache);
}

template <>
COptional<const std::vector<queueEntryType<CSupportValue>>> CClaimTrieCacheBase::getQueueCacheRow(int nHeight) const
{
    return getRow(*(base->db), SUPPORT_QUEUE_ROW, nHeight, supportQueueCache);
}

template <typename T>
COptional<const std::vector<queueEntryType<T>>> CClaimTrieCacheBase::getQueueCacheRow(int) const
{
    supportedType<T>();
    return {};
}

template <>
queueNameRowType* CClaimTrieCacheBase::getQueueCacheNameRow<CClaimValue>(const std::string& name, bool createIfNoExists)
{
    return getQueue(*(base->db), CLAIM_QUEUE_NAME_ROW, name, claimQueueNameCache, createIfNoExists);
}

template <>
queueNameRowType* CClaimTrieCacheBase::getQueueCacheNameRow<CSupportValue>(const std::string& name, bool createIfNoExists)
{
    return getQueue(*(base->db), SUPPORT_QUEUE_NAME_ROW, name, supportQueueNameCache, createIfNoExists);
}

template <typename T>
queueNameRowType* CClaimTrieCacheBase::getQueueCacheNameRow(const std::string&, bool)
{
    supportedType<T>();
    return nullptr;
}

template <>
COptional<const queueNameRowType> CClaimTrieCacheBase::getQueueCacheNameRow<CClaimValue>(const std::string& name) const
{
    return getRow(*(base->db), CLAIM_QUEUE_NAME_ROW, name, claimQueueNameCache);
}

template <>
COptional<const queueNameRowType> CClaimTrieCacheBase::getQueueCacheNameRow<CSupportValue>(const std::string& name) const
{
    return getRow(*(base->db), SUPPORT_QUEUE_NAME_ROW, name, supportQueueNameCache);
}

template <typename T>
COptional<const queueNameRowType> CClaimTrieCacheBase::getQueueCacheNameRow(const std::string&) const
{
    supportedType<T>();
    return {};
}

template <>
expirationQueueRowType* CClaimTrieCacheBase::getExpirationQueueCacheRow<CClaimValue>(int nHeight, bool createIfNoExists)
{
    return getQueue(*(base->db), CLAIM_EXP_QUEUE_ROW, nHeight, expirationQueueCache, createIfNoExists);
}

template <>
expirationQueueRowType* CClaimTrieCacheBase::getExpirationQueueCacheRow<CSupportValue>(int nHeight, bool createIfNoExists)
{
    return getQueue(*(base->db), SUPPORT_EXP_QUEUE_ROW, nHeight, supportExpirationQueueCache, createIfNoExists);
}

template <typename T>
expirationQueueRowType* CClaimTrieCacheBase::getExpirationQueueCacheRow(int, bool)
{
    supportedType<T>();
    return nullptr;
}

bool CClaimTrieCacheBase::haveClaim(const std::string& name, const COutPoint& outPoint) const
{
    auto it = find(name);
    return it && it->haveClaim(outPoint);
}

bool CClaimTrieCacheBase::haveSupport(const std::string& name, const COutPoint& outPoint) const
{
    const auto supports = getSupportsForName(name);
    return findOutPoint(supports, outPoint) != supports.end();
}

supportEntryType CClaimTrieCacheBase::getSupportsForName(const std::string& name) const
{
    auto sit = supportCache.find(name);
    if (sit != supportCache.end())
        return sit->second;

    supportEntryType supports;
    if (base->db->Read(std::make_pair(SUPPORT, name), supports)) // don't trust the try/catch in here
        return supports;
    return {};
}

template <typename T>
bool CClaimTrieCacheBase::haveInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    supportedType<T>();
    if (auto nameRow = getQueueCacheNameRow<T>(name)) {
        auto itNameRow = findOutPoint(*nameRow, outPoint);
        if (itNameRow != nameRow->end()) {
            nValidAtHeight = itNameRow->nHeight;
            if (auto row = getQueueCacheRow<T>(nValidAtHeight)) {
                auto iRow = findOutPoint(*row, CNameOutPointType{name, outPoint});
                if (iRow != row->end()) {
                    if (iRow->second.nValidAtHeight != nValidAtHeight)
                        LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, iRow->second.nValidAtHeight, nNextHeight);
                    return true;
                }
            }
        }
        LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nNextHeight);
    }
    return false;
}

bool CClaimTrieCacheBase::haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    return haveInQueue<CClaimValue>(name, outPoint, nValidAtHeight);
}

bool CClaimTrieCacheBase::haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    return haveInQueue<CSupportValue>(name, outPoint, nValidAtHeight);
}

std::size_t CClaimTrie::getTotalNamesInTrie() const
{
    std::size_t count = 0;
    for (auto it = begin(); it != end(); ++it)
        if (!it->empty()) ++count;
    return count;
}

std::size_t CClaimTrie::getTotalClaimsInTrie() const
{
    std::size_t count = 0;
    for (auto it = begin(); it != end(); ++it)
        count += it->claims.size();
    return count;
}

CAmount CClaimTrie::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    CAmount value_in_subtrie = 0;
    for (auto it = begin(); it != end(); ++it) {
        for (auto& claim : it->claims) {
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

template <typename T>
void CClaimTrieCacheBase::insertRowsFromQueue(std::vector<T>& result, const std::string& name) const
{
    supportedType<T>();
    if (auto nameRows = getQueueCacheNameRow<T>(name))
        for (auto& nameRow : *nameRows)
            if (auto rows = getQueueCacheRow<T>(nameRow.nHeight))
                for (auto& row : *rows)
                    if (row.first == name)
                        result.push_back(row.second);
}

CClaimSupportToName CClaimTrieCacheBase::getClaimsForName(const std::string& name) const
{
    claimEntryType claims;
    int nLastTakeoverHeight = 0;
    auto supports = getSupportsForName(name);
    insertRowsFromQueue(supports, name);

    if (auto it = find(name)) {
        claims = it->claims;
        nLastTakeoverHeight = it->nHeightOfLastTakeover;
    }
    insertRowsFromQueue(claims, name);

    auto find = [&supports](decltype(supports)::iterator& it, const CClaimValue& claim) {
        it = std::find_if(it, supports.end(), [&claim](const CSupportValue& support) {
            return claim.claimId == support.supportedClaimId;
        });
        return it != supports.end();
    };

    // match support to claim
    std::vector<CClaimNsupports> claimsNsupports;
    for (const auto& claim : claims) {
        CAmount nAmount = claim.nValidAtHeight < nNextHeight ? claim.nAmount : 0;
        auto ic = claimsNsupports.emplace(claimsNsupports.end(), claim, nAmount);
        for (auto it = supports.begin(); find(it, claim); it = supports.erase(it)) {
            if (it->nValidAtHeight < nNextHeight)
                ic->effectiveAmount += it->nAmount;
            ic->supports.emplace_back(std::move(*it));
        }
    }
    return {name, nLastTakeoverHeight, std::move(claimsNsupports), std::move(supports)};
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

template <typename T>
using iCbType = std::function<void(T&)>;

template <typename TIterator>
uint256 recursiveMerkleHash(TIterator& it, const iCbType<TIterator>& process)
{
    std::vector<uint8_t> vchToHash;
    const auto pos = it.key().size();
    for (auto& child : it.children()) {
        process(child);
        auto& key = child.key();
        auto hash = child->hash;
        completeHash(hash, key, pos);
        vchToHash.push_back(key[pos]);
        vchToHash.insert(vchToHash.end(), hash.begin(), hash.end());
    }

    CClaimValue claim;
    if (it->getBestClaim(claim)) {
        uint256 valueHash = getValueHash(claim.outPoint, it->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    } else if (!it.hasChildren()) {
        return {};
    }

    return Hash(vchToHash.begin(), vchToHash.end());
}

bool CClaimTrieCacheBase::recursiveCheckConsistency(CClaimTrie::const_iterator& it, std::string& failed) const
{
    struct CRecursiveBreak {};
    using iterator = CClaimTrie::const_iterator;
    iCbType<iterator> process = [&failed, &process](iterator& it) {
        if (it->hash.IsNull() || it->hash != recursiveMerkleHash(it, process)) {
            failed = it.key();
            throw CRecursiveBreak();
        }
    };

    try {
        process(it);
    } catch (const CRecursiveBreak&) {
        return false;
    }
    return true;
}

bool CClaimTrieCacheBase::checkConsistency() const
{
    if (base->empty())
        return true;

    auto it = base->cbegin();
    std::string failed;
    auto consistent = recursiveCheckConsistency(it, failed);
    if (!consistent) {
        LogPrintf("\nPrinting base tree from its parent:\n");
        auto basePath = base->nodes(failed);
        if (basePath.size() > 1) basePath.pop_back();
        dumpToLog(basePath.back(), false);
        auto cachePath = nodesToAddOrUpdate.nodes(failed);
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

    for (const auto& claim : claimsToDeleteFromByIdIndex) {
        auto it = std::find_if(claimsToAddToByIdIndex.begin(), claimsToAddToByIdIndex.end(),
            [&claim](const CClaimIndexElement& e) {
                return e.claim.claimId == claim.claimId;
            }
        );
        if (it == claimsToAddToByIdIndex.end())
            batch.Erase(std::make_pair(CLAIM_BY_ID, claim.claimId));
    }

    for (const auto& e : claimsToAddToByIdIndex)
        batch.Write(std::make_pair(CLAIM_BY_ID, e.claim.claimId), e);

    getMerkleHash();

    for (const auto& nodeName : nodesToDelete) {
        if (nodesToAddOrUpdate.contains(nodeName))
            continue;
        auto nodes = base->nodes(nodeName);
        base->erase(nodeName);
        for (auto& node : nodes)
            if (!node)
                batch.Erase(std::make_pair(TRIE_NODE, node.key()));
    }

    for (auto it = nodesToAddOrUpdate.begin(); it != nodesToAddOrUpdate.end(); ++it) {
        auto old = base->find(it.key());
        if (!old || old.data() != it.data()) {
            base->copy(it);
            batch.Write(std::make_pair(TRIE_NODE, it.key()), it.data());
        }
    }

    BatchWriteQueue(batch, SUPPORT, supportCache);

    BatchWriteQueue(batch, CLAIM_QUEUE_ROW, claimQueueCache);
    BatchWriteQueue(batch, CLAIM_QUEUE_NAME_ROW, claimQueueNameCache);
    BatchWriteQueue(batch, CLAIM_EXP_QUEUE_ROW, expirationQueueCache);

    BatchWriteQueue(batch, SUPPORT_QUEUE_ROW, supportQueueCache);
    BatchWriteQueue(batch, SUPPORT_QUEUE_NAME_ROW, supportQueueNameCache);
    BatchWriteQueue(batch, SUPPORT_EXP_QUEUE_ROW, supportExpirationQueueCache);

    base->nNextHeight = nNextHeight;
    if (!nodesToAddOrUpdate.empty() && (LogAcceptCategory(BCLog::CLAIMS) || LogAcceptCategory(BCLog::BENCH))) {
        LogPrintf("TrieCache size: %zu nodes on block %d, batch writes %zu bytes.\n",
                nodesToAddOrUpdate.height(), nNextHeight, batch.SizeEstimate());
    }
    auto ret = base->db->WriteBatch(batch);

    clear();
    return ret;
}

bool CClaimTrieCacheBase::ReadFromDisk(const CBlockIndex* tip)
{
    LogPrintf("Loading the claim trie from disk...\n");

    base->nNextHeight = nNextHeight = tip ? tip->nHeight + 1 : 0;

    if (tip && base->db->Exists(std::make_pair(TRIE_NODE_CHILDREN, std::string()))) {
        LogPrintf("The claim trie database contains deprecated data and will need to be rebuilt.\n");
        return false;
    }

    clear();
    base->clear();
    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, std::string> key;
        if (!pcursor->GetKey(key) || key.first != TRIE_NODE)
            continue;

        CClaimTrieData data;
        if (pcursor->GetValue(data)) {
            if (data.empty()) {
                // we have a situation where our old trie had many empty nodes
                // we don't want to automatically throw those all into our prefix trie
                // we'll run a second pass to clean them up
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

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, std::string> key;
        if (!pcursor->GetKey(key) || key.first != TRIE_NODE)
            continue;
        auto hit = base->find(key.second);
        if (hit) {
            CClaimTrieData data;
            if (pcursor->GetValue(data))
                hit->hash = data.hash;
        }
        else {
            base->db->Erase(key); // this uses a lot of memory and it's 1-time upgrade from 12.4 so we aren't going to batch it
        }
    }

    LogPrintf("Checking claim trie consistency... ");
    if (checkConsistency()) {
        LogPrintf("consistent\n");
        if (tip && tip->hashClaimTrie != getMerkleHash())
            return error("%s(): hashes don't match when reading claimtrie from disk", __func__);
        return true;
    }
    LogPrintf("inconsistent!\n");
    return false;
}

CClaimTrieCacheBase::CClaimTrieCacheBase(CClaimTrie* base) : base(base)
{
    assert(base);
    nNextHeight = base->nNextHeight;
}

int CClaimTrieCacheBase::expirationTime() const
{
    return Params().GetConsensus().nOriginalClaimExpirationTime;
}

uint256 CClaimTrieCacheBase::recursiveComputeMerkleHash(CClaimTrie::iterator& it)
{
    using iterator = CClaimTrie::iterator;
    iCbType<iterator> process = [&process](iterator& it) {
        if (it->hash.IsNull())
            it->hash = recursiveMerkleHash(it, process);
        assert(!it->hash.IsNull());
    };
    process(it);
    return it->hash;
}

uint256 CClaimTrieCacheBase::getMerkleHash()
{
    auto it = nodesToAddOrUpdate.begin();
    if (!it && nodesToDelete.empty())
        it = base->begin();
    return !it ? one : recursiveComputeMerkleHash(it);
}

CClaimTrie::const_iterator CClaimTrieCacheBase::find(const std::string& name) const
{
    auto it = nodesToAddOrUpdate.find(name);
    if (it || nodesToDelete.count(name))
        return it;
    return base->find(name);
}

bool CClaimTrieCacheBase::empty() const
{
    return nodesToAddOrUpdate.empty(); // only used with the dump method, and we don't want to dump base
}

CClaimTrie::iterator CClaimTrieCacheBase::cacheData(const std::string& name, bool create)
{
    // get data from the cache. if no data, create empty one
    const auto insert = [this](CClaimTrie::iterator& it) {
        auto& key = it.key();
        // we only ever cache nodes once per cache instance
        if (!nodesAlreadyCached.count(key)) {
            // do not insert nodes that are already present
            nodesAlreadyCached.insert(key);
            nodesToAddOrUpdate.insert(key, it.data());
        }
    };

    // we need all parent nodes and their one level deep children
    // to calculate merkle hash
    auto nodes = base->nodes(name);
    for (auto& node: nodes) {
        for (auto& child : node.children())
            if (!nodesAlreadyCached.count(child.key()))
                nodesToAddOrUpdate.copy(child);
        insert(node);
    }

    auto it = nodesToAddOrUpdate.find(name);
    if (!it && create) {
        it = nodesToAddOrUpdate.insert(name, CClaimTrieData{});
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
        if (it->getBestClaim(claim)) {
            claimId = claim.claimId;
            return true;
        }
    }
    return false;
}

void CClaimTrieCacheBase::markAsDirty(const std::string& name, bool fCheckTakeover)
{
    for (auto& node : nodesToAddOrUpdate.nodes(name))
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
        LogPrint(BCLog::CLAIMS, "%s: Removing a claim was unsuccessful. name = %s, txhash = %s, nOut = %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n);
        return false;
    }

    if (!it->claims.empty()) {
        auto supports = getSupportsForName(name);
        it->reorderClaims(supports);
    } else {
        // in case we pull a child into our spot; we will then need their kids for hash
        bool hasChild = it.hasChildren();
        for (auto& child: it.children())
            cacheData(child.key(), false);

        nodesToAddOrUpdate.erase(name);
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

template <typename T>
T CClaimTrieCacheBase::add(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight)
{
    supportedType<T>();
    assert(nHeight == nNextHeight);
    auto delay = getDelayForName(name, claimId);
    T value(outPoint, claimId, nAmount, nHeight, nHeight + delay);
    addToQueue(name, value);
    return value;
}

bool CClaimTrieCacheBase::addClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight)
{
    auto claim = add<CClaimValue>(name, outPoint, claimId, nAmount, nHeight);
    claimsToAddToByIdIndex.emplace_back(name, claim);
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %d, claimId: %s, nAmount: %d, nHeight: %d, nValidHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, claimId.GetHex(), nAmount, nHeight, claim.nValidAtHeight);
    return true;
}

bool CClaimTrieCacheBase::addSupport(const std::string& name, const COutPoint& outPoint, CAmount nAmount, const uint160& supportedClaimId, int nHeight)
{
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedClaimId: %s, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nAmount, supportedClaimId.GetHex(), nHeight, nNextHeight);
    add<CSupportValue>(name, outPoint, supportedClaimId, nAmount, nHeight);
    return true;
}

template <typename T>
bool CClaimTrieCacheBase::addToQueue(const std::string& name, const T& value)
{
    supportedType<T>();
    const auto newName = adjustNameForValidHeight(name, value.nValidAtHeight);
    auto itQueueCache = getQueueCacheRow<T>(value.nValidAtHeight, true);
    itQueueCache->emplace_back(newName, value);
    auto itQueueName = getQueueCacheNameRow<T>(newName, true);
    itQueueName->emplace_back(value.outPoint, value.nValidAtHeight);
    auto itQueueExpiration = getExpirationQueueCacheRow<T>(value.nHeight + expirationTime(), true);
    itQueueExpiration->emplace_back(newName, value.outPoint);
    return true;
}

template <>
bool CClaimTrieCacheBase::addToCache(const std::string& name, const CClaimValue& value, bool fCheckTakeover)
{
    return insertClaimIntoTrie(name, value, fCheckTakeover);
}

template <>
bool CClaimTrieCacheBase::addToCache(const std::string& name, const CSupportValue& value, bool fCheckTakeover)
{
    return insertSupportIntoMap(name, value, fCheckTakeover);
}

template <typename T>
bool CClaimTrieCacheBase::addToCache(const std::string&, const T&, bool)
{
    supportedType<T>();
    return false;
}

template <typename T>
bool CClaimTrieCacheBase::undoSpend(const std::string& name, const T& value, int nValidAtHeight)
{
    supportedType<T>();
    if (nValidAtHeight < nNextHeight) {
        auto itQueueExpiration = getExpirationQueueCacheRow<T>(value.nHeight + expirationTime(), true);
        itQueueExpiration->emplace_back(adjustNameForValidHeight(name, nValidAtHeight), value.outPoint);
        return addToCache(name, value, false);
    }
    return addToQueue(name, value);
}

bool CClaimTrieCacheBase::undoSpendClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight, int nValidAtHeight)
{
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %d, claimId: %s, nAmount: %d, nHeight: %d, nValidAtHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, claimId.GetHex(), nAmount, nHeight, nValidAtHeight, nNextHeight);
    CClaimValue claim(outPoint, claimId, nAmount, nHeight, nValidAtHeight);
    claimsToAddToByIdIndex.emplace_back(name, claim);
    return undoSpend(name, claim, nValidAtHeight);
}

bool CClaimTrieCacheBase::undoSpendSupport(const std::string& name, const COutPoint& outPoint, const uint160& supportedClaimId, CAmount nAmount, int nHeight, int nValidAtHeight)
{
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedClaimId: %s, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nAmount, supportedClaimId.GetHex(), nHeight, nNextHeight);
    CSupportValue support(outPoint, supportedClaimId, nAmount, nHeight, nValidAtHeight);
    return undoSpend(name, support, nValidAtHeight);
}

template <typename T>
bool CClaimTrieCacheBase::removeFromQueue(const std::string& name, const COutPoint& outPoint, T& value)
{
    supportedType<T>();
    if (auto itQueueNameRow = getQueueCacheNameRow<T>(name, false)) {
        auto itQueueName = findOutPoint(*itQueueNameRow, outPoint);
        if (itQueueName != itQueueNameRow->end()) {
            if (auto itQueueRow = getQueueCacheRow<T>(itQueueName->nHeight, false)) {
                auto itQueue = findOutPoint(*itQueueRow, CNameOutPointType{name, outPoint});
                if (itQueue != itQueueRow->end()) {
                    std::swap(value, itQueue->second);
                    itQueueNameRow->erase(itQueueName);
                    itQueueRow->erase(itQueue);
                    return true;
                }
            }
            LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, itQueueName->nHeight, nNextHeight);
        }
    }
    return false;
}

bool CClaimTrieCacheBase::undoAddClaim(const std::string& name, const COutPoint& outPoint, int nHeight)
{
    int throwaway;
    return removeClaim(name, outPoint, nHeight, throwaway, false);
}

bool CClaimTrieCacheBase::undoAddSupport(const std::string& name, const COutPoint& outPoint, int nHeight)
{
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %d, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nNextHeight);
    int throwaway;
    return removeSupport(name, outPoint, nHeight, throwaway, false);
}

bool CClaimTrieCacheBase::spendClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight)
{
    return removeClaim(name, outPoint, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCacheBase::spendSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight)
{
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %d, nHeight: %d, nNextHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nNextHeight);
    return removeSupport(name, outPoint, nHeight, nValidAtHeight, true);
}

template <>
bool CClaimTrieCacheBase::removeFromCache(const std::string& name, const COutPoint& outPoint, CClaimValue& value, bool fCheckTakeover)
{
    return removeClaimFromTrie(name, outPoint, value, fCheckTakeover);
}

template <>
bool CClaimTrieCacheBase::removeFromCache(const std::string& name, const COutPoint& outPoint, CSupportValue& value, bool fCheckTakeover)
{
    return removeSupportFromMap(name, outPoint, value, fCheckTakeover);
}

template <typename T>
bool CClaimTrieCacheBase::removeFromCache(const std::string& name, const COutPoint& outPoint, T& value, bool fCheckTakeover)
{
    supportedType<T>();
    return false;
}

template <typename T>
bool CClaimTrieCacheBase::remove(T& value, const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover)
{
    supportedType<T>();
    nValidAtHeight = nHeight + getDelayForName(name);
    std::string adjusted = adjustNameForValidHeight(name, nValidAtHeight);

    if (removeFromQueue(adjusted, outPoint, value) || removeFromCache(name, outPoint, value, fCheckTakeover)) {
        int expirationHeight = value.nHeight + expirationTime();
        if (auto itQueueRow = getExpirationQueueCacheRow<T>(expirationHeight, false))
            eraseOutPoint(*itQueueRow, CNameOutPointType{adjusted, outPoint});
        nValidAtHeight = value.nValidAtHeight;
        return true;
    }
    return false;
}

bool CClaimTrieCacheBase::removeClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover)
{
    LogPrint(BCLog::CLAIMS, "%s: name: %s, txhash: %s, nOut: %s, nNextHeight: %s\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nNextHeight);

    CClaimValue claim;
    if (remove(claim, name, outPoint, nHeight, nValidAtHeight, fCheckTakeover)) {
        claimsToDeleteFromByIdIndex.insert(claim);
        return true;
    }
    return false;
}

bool CClaimTrieCacheBase::removeSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover)
{
    CSupportValue support;
    return remove(support, name, outPoint, nHeight, nValidAtHeight, fCheckTakeover);
}

bool CClaimTrieCacheBase::insertSupportIntoMap(const std::string& name, const CSupportValue& support, bool fCheckTakeover)
{
    auto sit = supportCache.find(name);
    if (sit == supportCache.end())
        sit = supportCache.emplace(name, getSupportsForName(name)).first;

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
    auto sit = supportCache.find(name);
    if (sit == supportCache.end())
        sit = supportCache.emplace(name, getSupportsForName(name)).first;

    if (eraseOutPoint(sit->second, outPoint, &support)) {
        addTakeoverWorkaroundPotential(name);

        if (auto dit = cacheData(name, false)) {
            markAsDirty(name, fCheckTakeover);
            dit->reorderClaims(sit->second);
        }
        return true;
    }
    LogPrint(BCLog::CLAIMS, "CClaimTrieCacheBase::%s() : asked to remove a support that doesn't exist\n", __func__);
    return false;
}

void CClaimTrieCacheBase::dumpToLog(CClaimTrie::const_iterator it, bool diffFromBase) const
{
    if (diffFromBase) {
        auto hit = base->find(it.key());
        if (hit && hit->hash == it->hash)
            return;
    }

    std::string indent(it.depth(), ' ');
    auto children = it.children();
    auto empty = children.empty() && it->claims.empty();
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

bool CClaimTrieCacheBase::shouldUseTakeoverWorkaround(const std::string& key) const
{
    auto it = takeoverWorkaround.find(key);
    return it != takeoverWorkaround.end() && it->second;
}

void CClaimTrieCacheBase::addTakeoverWorkaroundPotential(const std::string& key)
{
    // the old code would add to the cache using a shortcut in the add/removeSupport methods
    // this logic mimics the effects of that.
    // (and the shortcut would later lead to a miscalculation of the takeover height)
    if (nNextHeight > Params().GetConsensus().nMinTakeoverWorkaroundHeight
        && nNextHeight < Params().GetConsensus().nMaxTakeoverWorkaroundHeight
        && !nodesToAddOrUpdate.contains(key) && base->contains(key))
        takeoverWorkaround.emplace(key, false);
}

void CClaimTrieCacheBase::confirmTakeoverWorkaroundNeeded(const std::string& key)
{
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

template <typename T>
inline void addTo(std::set<T>* set, const T& value)
{
    set->insert(value);
}

template <>
inline void addTo(std::set<CSupportValue>*, const CSupportValue&)
{
}

template <typename T>
void CClaimTrieCacheBase::undoIncrement(insertUndoType& insertUndo, std::vector<queueEntryType<T>>& expireUndo, std::set<T>* deleted)
{
    supportedType<T>();
    if (auto itQueueRow = getQueueCacheRow<T>(nNextHeight, false)) {
        for (const auto& itEntry : *itQueueRow) {
            if (auto itQueueNameRow = getQueueCacheNameRow<T>(itEntry.first, false)) {
                auto& points = *itQueueNameRow;
                auto itQueueName = std::find_if(points.begin(), points.end(), [&itEntry, this](const COutPointHeightType& point) {
                     return point.outPoint == itEntry.second.outPoint && point.nHeight == nNextHeight;
                });
                if (itQueueName != points.end()) {
                    points.erase(itQueueName);
                } else {
                    LogPrintf("%s: An inconsistency was found in the queue. Please report this to the developers:\nFound in height queue but not in named queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itEntry.first, itEntry.second.outPoint.hash.GetHex(), itEntry.second.outPoint.n, itEntry.second.nValidAtHeight, nNextHeight);
                    LogPrintf("Elements found for that name:\n");
                    for (const auto& itQueueNameInner : points)
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itQueueNameInner.outPoint.hash.GetHex(), itQueueNameInner.outPoint.n, itQueueNameInner.nHeight);
                    assert(false);
                }
            } else {
                LogPrintf("Nothing found for %s\n", itEntry.first);
                assert(false);
            }
            addToCache(itEntry.first, itEntry.second, true);
            insertUndo.emplace_back(itEntry.first, itEntry.second.outPoint, itEntry.second.nValidAtHeight);
        }
        itQueueRow->clear();
    }

    if (auto itExpirationRow = getExpirationQueueCacheRow<T>(nNextHeight, false)) {
        for (const auto& itEntry : *itExpirationRow) {
            T value;
            assert(removeFromCache(itEntry.name, itEntry.outPoint, value, true));
            expireUndo.emplace_back(itEntry.name, value);
            addTo(deleted, value);
        }
        itExpirationRow->clear();
    }
}

template <typename T>
void CClaimTrieCacheBase::undoIncrement(const std::string& name, insertUndoType& insertUndo, std::vector<queueEntryType<T>>& expireUndo)
{
    supportedType<T>();
    if (auto itQueueNameRow = getQueueCacheNameRow<T>(name, false)) {
        for (const auto& itQueueName : *itQueueNameRow) {
            bool found = false;
            // Pull those claims out of the height-based queue
            if (auto itQueueRow = getQueueCacheRow<T>(itQueueName.nHeight, false)) {
                auto& points = *itQueueRow;
                auto itQueue = std::find_if(points.begin(), points.end(), [&name, &itQueueName](const queueEntryType<T>& point) {
                    return name == point.first && point.second.outPoint == itQueueName.outPoint && point.second.nValidAtHeight == itQueueName.nHeight;
                });
                if (itQueue != points.end()) {
                    // Insert them into the queue undo with their previous nValidAtHeight
                    insertUndo.emplace_back(itQueue->first, itQueue->second.outPoint, itQueue->second.nValidAtHeight);
                    // Insert them into the name trie with the new nValidAtHeight
                    itQueue->second.nValidAtHeight = nNextHeight;
                    addToCache(itQueue->first, itQueue->second, false);
                    // Delete them from the height-based queue
                    points.erase(itQueue);
                    found = true;
                }
            }
            if (!found)
                LogPrintf("%s(): An inconsistency was found in the queue. Please report this to the developers:\nFound in name queue but not in height based queue:\nname: %s, txid: %s, nOut: %d, nValidAtHeight in name based queue: %d, current height: %d\n", __func__, name, itQueueName.outPoint.hash.GetHex(), itQueueName.outPoint.n, itQueueName.nHeight, nNextHeight);
            assert(found);
        }
        // remove all claims from the queue for that name
        itQueueNameRow->clear();
    }
}

bool CClaimTrieCacheBase::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    undoIncrement(insertUndo, expireUndo, &claimsToDeleteFromByIdIndex);
    undoIncrement(insertSupportUndo, expireSupportUndo);

    // check each potentially taken over name to see if a takeover occurred.
    // if it did, then check the claim and support insertion queues for
    // the names that have been taken over, immediately insert all claim and
    // supports for those names, and stick them in the insertUndo or
    // insertSupportUndo vectors, with the nValidAtHeight they had prior to
    // this block.
    // Run through all names that have been taken over
    for (const auto& itNamesToCheck : namesToCheckForTakeover) {
        // Check if a takeover has occurred (only going to hit each name once)
        auto itCachedNode = nodesToAddOrUpdate.find(itNamesToCheck);
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
        uint160 ownersClaimId;
        CClaimValue claimInCache;
        int ownersTakeoverHeight = 0;
        bool haveClaimInTrie = getLastTakeoverForName(itNamesToCheck, ownersClaimId, ownersTakeoverHeight);
        bool haveClaimInCache = itCachedNode && itCachedNode->getBestClaim(claimInCache);
        bool takeoverHappened = !haveClaimInCache || !haveClaimInTrie || claimInCache.claimId != ownersClaimId;

        if (takeoverHappened) {
            // Get all pending claims for that name and activate them all in the case that our winner is defunct.
            undoIncrement(itNamesToCheck, insertUndo, expireUndo);
            undoIncrement(itNamesToCheck, insertSupportUndo, expireSupportUndo);
        }

        // not sure if this should happen above or below the above code:
        auto shouldUse = shouldUseTakeoverWorkaround(itNamesToCheck);
        if (!takeoverHappened && shouldUse)
            LogPrint(BCLog::CLAIMS, "TakeoverHeight workaround affects block: %d, name: %s, th: %d\n", nNextHeight, itNamesToCheck, ownersTakeoverHeight);
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

        if ((itCachedNode = nodesToAddOrUpdate.find(itNamesToCheck))) {
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

template <typename T>
inline void addToIndex(std::vector<CClaimIndexElement>*, const std::string&, const T&)
{
}

template <>
inline void addToIndex(std::vector<CClaimIndexElement>* index, const std::string& name, const CClaimValue& value)
{
    index->emplace_back(name, value);
}

template <typename T>
void CClaimTrieCacheBase::undoDecrement(insertUndoType& insertUndo, std::vector<queueEntryType<T>>& expireUndo, std::vector<CClaimIndexElement>* index, std::set<T>* deleted)
{
    supportedType<T>();
    if (!expireUndo.empty()) {
        for (auto itExpireUndo = expireUndo.crbegin(); itExpireUndo != expireUndo.crend(); ++itExpireUndo) {
            addToCache(itExpireUndo->first, itExpireUndo->second, false);
            addToIndex(index, itExpireUndo->first, itExpireUndo->second);
            if (nNextHeight == itExpireUndo->second.nHeight + expirationTime()) {
                auto itExpireRow = getExpirationQueueCacheRow<T>(nNextHeight, true);
                itExpireRow->emplace_back(itExpireUndo->first, itExpireUndo->second.outPoint);
            }
        }
    }

    for (auto itInsertUndo = insertUndo.crbegin(); itInsertUndo != insertUndo.crend(); ++itInsertUndo) {
        T value;
        assert(removeFromCache(itInsertUndo->name, itInsertUndo->outPoint, value, false));
        if (itInsertUndo->nHeight >= 0) { // aka it became valid at height rather than being rename/normalization
            // value.nValidHeight may have been changed if this was inserted before activation height
            // due to a triggered takeover, change it back to original nValidAtHeight
            value.nValidAtHeight = itInsertUndo->nHeight;
            auto itQueueRow = getQueueCacheRow<T>(itInsertUndo->nHeight, true);
            auto itQueueNameRow = getQueueCacheNameRow<T>(itInsertUndo->name, true);
            itQueueRow->emplace_back(itInsertUndo->name, value);
            itQueueNameRow->emplace_back(itInsertUndo->outPoint, value.nValidAtHeight);
        } else {
            addTo(deleted, value);
        }
    }
}

bool CClaimTrieCacheBase::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    nNextHeight--;

    undoDecrement(insertSupportUndo, expireSupportUndo);
    undoDecrement(insertUndo, expireUndo, &claimsToAddToByIdIndex, &claimsToDeleteFromByIdIndex);
    return true;
}

bool CClaimTrieCacheBase::finalizeDecrement(std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
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

template <typename T>
void CClaimTrieCacheBase::reactivate(const expirationQueueRowType& row, int height, bool increment)
{
    supportedType<T>();
    for (auto& e: row) {
        // remove and insert with new expiration time
        if (auto itQueueRow = getExpirationQueueCacheRow<T>(height, false))
            eraseOutPoint(*itQueueRow, CNameOutPointType{e.name, e.outPoint});

        int extend_expiration = Params().GetConsensus().nExtendedClaimExpirationTime - Params().GetConsensus().nOriginalClaimExpirationTime;
        int new_expiration_height = increment ? height + extend_expiration : height - extend_expiration;
        auto itQueueExpiration = getExpirationQueueCacheRow<T>(new_expiration_height, true);
        itQueueExpiration->emplace_back(e.name, e.outPoint);
    }
}

void CClaimTrieCacheBase::reactivateClaim(const expirationQueueRowType& row, int height, bool increment)
{
    reactivate<CClaimValue>(row, height, increment);
}

void CClaimTrieCacheBase::reactivateSupport(const expirationQueueRowType& row, int height, bool increment)
{
    reactivate<CSupportValue>(row, height, increment);
}

int CClaimTrieCacheBase::getNumBlocksOfContinuousOwnership(const std::string& name) const
{
    auto hit = removalWorkaround.find(name);
    if (hit != removalWorkaround.end()) {
        auto that = const_cast<CClaimTrieCacheBase*>(this);
        that->removalWorkaround.erase(hit);
        return 0;
    }
    auto it = nodesToAddOrUpdate.find(name);
    return (it || (it = base->find(name))) && !it->empty() ? nNextHeight - it->nHeightOfLastTakeover : 0;
}

int CClaimTrieCacheBase::getDelayForName(const std::string& name) const
{
    int nBlocksOfContinuousOwnership = getNumBlocksOfContinuousOwnership(name);
    return std::min(nBlocksOfContinuousOwnership / base->nProportionalDelayFactor, 4032);
}

int CClaimTrieCacheBase::getDelayForName(const std::string& name, const uint160& claimId) const
{
    uint160 winningClaimId;
    int winningTakeoverHeight;
    if (getLastTakeoverForName(name, winningClaimId, winningTakeoverHeight) && winningClaimId == claimId) {
        assert(winningTakeoverHeight <= nNextHeight);
        return 0;
    }
    return getDelayForName(name);
}

std::string CClaimTrieCacheBase::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return name;
}

bool CClaimTrieCacheBase::clear()
{
    supportCache.clear();
    nodesToDelete.clear();
    takeoverCache.clear();
    claimQueueCache.clear();
    supportQueueCache.clear();
    removalWorkaround.clear();
    nodesToAddOrUpdate.clear();
    nodesAlreadyCached.clear();
    takeoverWorkaround.clear();
    claimQueueNameCache.clear();
    expirationQueueCache.clear();
    supportQueueNameCache.clear();
    claimsToAddToByIdIndex.clear();
    namesToCheckForTakeover.clear();
    supportExpirationQueueCache.clear();
    claimsToDeleteFromByIdIndex.clear();
    return true;
}

bool CClaimTrieCacheBase::getProofForName(const std::string& name, CClaimTrieProof& proof)
{
    // cache the parent nodes
    cacheData(name, false);
    getMerkleHash();
    proof = CClaimTrieProof();
    for (auto& it : static_cast<const CClaimTrie&>(nodesToAddOrUpdate).nodes(name)) {
        CClaimValue claim;
        const auto& key = it.key();
        bool fNodeHasValue = it->getBestClaim(claim);
        uint256 valueHash;
        if (fNodeHasValue)
            valueHash = getValueHash(claim.outPoint, it->nHeightOfLastTakeover);

        const auto pos = key.size();
        std::vector<std::pair<unsigned char, uint256>> children;
        for (auto& child : it.children()) {
            auto& childKey = child.key();
            if (name.find(childKey) == 0) {
                for (auto i = pos; i + 1 < childKey.size(); ++i) {
                    children.emplace_back(childKey[i], uint256{});
                    proof.nodes.emplace_back(children, fNodeHasValue, valueHash);
                    children.clear();
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
            proof.hasValue = fNodeHasValue;
            if (proof.hasValue) {
                proof.outPoint = claim.outPoint;
                proof.nHeightOfLastTakeover = it->nHeightOfLastTakeover;
            }
            valueHash.SetNull();
        }
        proof.nodes.emplace_back(std::move(children), fNodeHasValue, valueHash);
    }
    return true;
}

void CClaimTrieCacheBase::iterate(std::function<void(const std::string&, const CClaimTrieData&)> callback) const
{
    if (nodesToAddOrUpdate.empty()) {
        for (auto it = base->cbegin(); it != base->cend(); ++it)
            if (!nodesToDelete.count(it.key()))
                callback(it.key(), it.data());
        return;
    }
    for (auto it = nodesToAddOrUpdate.begin(); it != nodesToAddOrUpdate.end(); ++it) {
        callback(it.key(), it.data());
        if (it.hasChildren() || nodesToDelete.count(it.key()))
            continue;
        auto children = base->find(it.key()).children();
        for (auto& child : children)
            for (; child; ++child)
                if (!nodesToDelete.count(child.key()))
                    callback(child.key(), child.data());
    }
}
