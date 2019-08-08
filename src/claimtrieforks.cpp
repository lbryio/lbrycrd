
#include <consensus/merkle.h>
#include <chainparams.h>
#include <claimtrie.h>
#include <hash.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/locale/localization_backend.hpp>
#include <boost/scope_exit.hpp>
#include <boost/scoped_ptr.hpp>

CClaimTrieCacheExpirationFork::CClaimTrieCacheExpirationFork(CClaimTrie* base)
    : CClaimTrieCacheBase(base)
{
    setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
}

void CClaimTrieCacheExpirationFork::setExpirationTime(int time)
{
    nExpirationTime = time;
}

int CClaimTrieCacheExpirationFork::expirationTime() const
{
    return nExpirationTime;
}

bool CClaimTrieCacheExpirationFork::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    if (CClaimTrieCacheBase::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo)) {
        setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
        return true;
    }
    return false;
}

bool CClaimTrieCacheExpirationFork::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    if (CClaimTrieCacheBase::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo)) {
        setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
        return true;
    }
    return false;
}

void CClaimTrieCacheExpirationFork::initializeIncrement()
{
    // we could do this in the constructor, but that would not allow for multiple increments in a row (as done in unit tests)
    if (nNextHeight != Params().GetConsensus().nExtendedClaimExpirationForkHeight)
        return;

    forkForExpirationChange(true);
}

bool CClaimTrieCacheExpirationFork::finalizeDecrement(std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    auto ret = CClaimTrieCacheBase::finalizeDecrement(takeoverHeightUndo);
    if (ret && nNextHeight == Params().GetConsensus().nExtendedClaimExpirationForkHeight)
       forkForExpirationChange(false);
    return ret;
}

bool CClaimTrieCacheExpirationFork::forkForExpirationChange(bool increment)
{
    /*
    If increment is True, we have forked to extend the expiration time, thus items in the expiration queue
    will have their expiration extended by "new expiration time - original expiration time"

    If increment is False, we are decremented a block to reverse the fork. Thus items in the expiration queue
    will have their expiration extension removed.
    */

    //look through db for expiration queues, if we haven't already found it in dirty expiration queue
    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());
    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, int> key;
        if (!pcursor->GetKey(key))
            continue;
        int height = key.second;
        if (key.first == CLAIM_EXP_QUEUE_ROW) {
            expirationQueueRowType row;
            if (pcursor->GetValue(row)) {
                reactivateClaim(row, height, increment);
            } else {
                return error("%s(): error reading expiration queue rows from disk", __func__);
            }
        } else if (key.first == SUPPORT_EXP_QUEUE_ROW) {
            expirationQueueRowType row;
            if (pcursor->GetValue(row)) {
                reactivateSupport(row, height, increment);
            } else {
                return error("%s(): error reading support expiration queue rows from disk", __func__);
            }
        }
    }

    return true;
}

bool CClaimTrieCacheNormalizationFork::shouldNormalize() const
{
    return nNextHeight > Params().GetConsensus().nNormalizedNameForkHeight;
}

std::string CClaimTrieCacheNormalizationFork::normalizeClaimName(const std::string& name, bool force) const
{
    if (!force && !shouldNormalize())
        return name;

    static std::locale utf8;
    static bool initialized = false;
    if (!initialized) {
        static boost::locale::localization_backend_manager manager =
            boost::locale::localization_backend_manager::global();
        manager.select("icu");

        static boost::locale::generator curLocale(manager);
        utf8 = curLocale("en_US.UTF8");
        initialized = true;
    }

    std::string normalized;
    try {
        // Check if it is a valid utf-8 string. If not, it will throw a
        // boost::locale::conv::conversion_error exception which we catch later
        normalized = boost::locale::conv::to_utf<char>(name, "UTF-8", boost::locale::conv::stop);
        if (normalized.empty())
            return name;

        // these methods supposedly only use the "UTF8" portion of the locale object:
        normalized = boost::locale::normalize(normalized, boost::locale::norm_nfd, utf8);
        normalized = boost::locale::fold_case(normalized, utf8);
    } catch (const boost::locale::conv::conversion_error& e) {
        return name;
    } catch (const std::bad_cast& e) {
        LogPrintf("%s() is invalid or dependencies are missing: %s\n", __func__, e.what());
        throw;
    } catch (const std::exception& e) { // TODO: change to use ... with current_exception() in c++11
        LogPrintf("%s() had an unexpected exception: %s\n", __func__, e.what());
        return name;
    }

    return normalized;
}

bool CClaimTrieCacheNormalizationFork::insertClaimIntoTrie(const std::string& name, const CClaimValue& claim, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::insertClaimIntoTrie(normalizeClaimName(name, overrideInsertNormalization), claim, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::removeClaimFromTrie(const std::string& name, const COutPoint& outPoint, CClaimValue& claim, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::removeClaimFromTrie(normalizeClaimName(name, overrideRemoveNormalization), outPoint, claim, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::insertSupportIntoMap(const std::string& name, const CSupportValue& support, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::insertSupportIntoMap(normalizeClaimName(name, overrideInsertNormalization), support, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::removeSupportFromMap(const std::string& name, const COutPoint& outPoint, CSupportValue& support, bool fCheckTakeover)
{
    return CClaimTrieCacheExpirationFork::removeSupportFromMap(normalizeClaimName(name, overrideRemoveNormalization), outPoint, support, fCheckTakeover);
}

bool CClaimTrieCacheNormalizationFork::normalizeAllNamesInTrieIfNecessary(insertUndoType& insertUndo, claimQueueRowType& removeUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    if (nNextHeight != Params().GetConsensus().nNormalizedNameForkHeight)
        return false;

    // run the one-time upgrade of all names that need to change
    // it modifies the (cache) trie as it goes, so we need to grab everything to be modified first

    boost::scoped_ptr<CDBIterator> pcursor(base->db->NewIterator());
    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, std::string> key;
        if (!pcursor->GetKey(key) || key.first != TRIE_NODE_CHILDREN)
            continue;

        const auto& name = key.second;
        const std::string normalized = normalizeClaimName(name, true);
        if (normalized == key.second)
            continue;

        auto supports = getSupportsForName(name);
        for (auto support : supports) {
            // if it's already going to expire just skip it
            if (support.nHeight + expirationTime() <= nNextHeight)
                continue;

            assert(removeSupportFromMap(name, support.outPoint, support, false));
            expireSupportUndo.emplace_back(name, support);
            assert(insertSupportIntoMap(normalized, support, false));
            insertSupportUndo.emplace_back(name, support.outPoint, -1);
        }

        namesToCheckForTakeover.insert(normalized);

        auto cached = cacheData(name, false);
        if (!cached || cached->empty())
            continue;

        auto claimsCopy = cached->claims;
        auto takeoverHeightCopy = cached->nHeightOfLastTakeover;
        for (auto claim : claimsCopy) {
            if (claim.nHeight + expirationTime() <= nNextHeight)
                continue;

            assert(removeClaimFromTrie(name, claim.outPoint, claim, false));
            removeUndo.emplace_back(name, claim);
            assert(insertClaimIntoTrie(normalized, claim, true));
            insertUndo.emplace_back(name, claim.outPoint, -1);
        }

        takeoverHeightUndo.emplace_back(name, takeoverHeightCopy);
    }
    return true;
}

bool CClaimTrieCacheNormalizationFork::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    overrideInsertNormalization = normalizeAllNamesInTrieIfNecessary(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo);
    BOOST_SCOPE_EXIT(&overrideInsertNormalization) { overrideInsertNormalization = false; }
    BOOST_SCOPE_EXIT_END
    return CClaimTrieCacheExpirationFork::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo);
}

bool CClaimTrieCacheNormalizationFork::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    overrideRemoveNormalization = shouldNormalize();
    BOOST_SCOPE_EXIT(&overrideRemoveNormalization) { overrideRemoveNormalization = false; }
    BOOST_SCOPE_EXIT_END
    return CClaimTrieCacheExpirationFork::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo);
}

bool CClaimTrieCacheNormalizationFork::getProofForName(const std::string& name, CClaimTrieProof& proof)
{
    return CClaimTrieCacheExpirationFork::getProofForName(normalizeClaimName(name), proof);
}

bool CClaimTrieCacheNormalizationFork::getInfoForName(const std::string& name, CClaimValue& claim) const
{
    return CClaimTrieCacheExpirationFork::getInfoForName(normalizeClaimName(name), claim);
}

CClaimSupportToName CClaimTrieCacheNormalizationFork::getClaimsForName(const std::string& name) const
{
    return CClaimTrieCacheExpirationFork::getClaimsForName(normalizeClaimName(name));
}

int CClaimTrieCacheNormalizationFork::getDelayForName(const std::string& name, const uint160& claimId) const
{
    return CClaimTrieCacheExpirationFork::getDelayForName(normalizeClaimName(name), claimId);
}

std::string CClaimTrieCacheNormalizationFork::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return normalizeClaimName(name, validHeight > Params().GetConsensus().nNormalizedNameForkHeight);
}

CClaimTrieCacheHashFork::CClaimTrieCacheHashFork(CClaimTrie* base) : CClaimTrieCacheNormalizationFork(base)
{
}

static const uint256 leafHash = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
static const uint256 emptyHash = uint256S("0000000000000000000000000000000000000000000000000000000000000003");

std::vector<uint256> getClaimHashes(const CClaimTrieData& data)
{
    std::vector<uint256> hashes;
    for (auto& claim : data.claims)
        hashes.push_back(getValueHash(claim.outPoint, data.nHeightOfLastTakeover));
    return hashes;
}

template <typename T>
using iCbType = std::function<uint256(T&)>;

template <typename T>
using decay = typename std::decay<T>::type;

template <typename Vector, typename T>
uint256 recursiveMerkleHash(const CClaimTrieData& data, Vector&& children, const iCbType<T>& childHash)
{
    static_assert(std::is_same<decay<Vector>, std::vector<decay<T>>>::value, "Vector should be std vector");
    static_assert(std::is_same<decltype(children[0]), T&>::value, "Vector element type should match callback type");

    std::vector<uint256> childHashes;
    for (auto& child : children)
        childHashes.emplace_back(childHash(child));

    std::vector<uint256> claimHashes;
    if (!data.empty())
        claimHashes = getClaimHashes(data);
    else if (children.empty())
        return {};

    auto left = childHashes.empty() ? leafHash : ComputeMerkleRoot(childHashes);
    auto right = claimHashes.empty() ? emptyHash : ComputeMerkleRoot(claimHashes);

    return Hash(left.begin(), left.end(), right.begin(), right.end());
}

extern const uint256 one;

bool CClaimTrieHashFork::checkConsistency(const uint256& rootHash) const
{
    if (nNextHeight < Params().GetConsensus().nAllClaimsInMerkleForkHeight)
        return CClaimTrie::checkConsistency(rootHash);

    CClaimTrieDataNode node;
    if (!find({}, node) || node.hash != rootHash) {
        if (rootHash == one)
            return true;

        return error("Mismatched root claim trie hashes. This may happen when there is not a clean process shutdown. Please run with -reindex.");
    }

    bool success = true;
    recurseNodes({}, node, [&success, this](const std::string& name, const CClaimTrieData& data, const std::vector<std::string>& children) {
        if (!success) return;

        iCbType<const std::string> callback = [&success, &name, this](const std::string& child) -> uint256 {
            auto key = name + child;
            CClaimTrieDataNode node;
            success &= find(key, node);
            return node.hash;
        };

        success &= !data.hash.IsNull();
        success &= data.hash == recursiveMerkleHash(data, children, callback);
    });

    return success;
}

uint256 CClaimTrieCacheHashFork::recursiveComputeMerkleHash(CClaimPrefixTrie::iterator& it)
{
    if (nNextHeight < Params().GetConsensus().nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::recursiveComputeMerkleHash(it);

    using iterator = CClaimPrefixTrie::iterator;
    iCbType<iterator> process = [&process](iterator& it) -> uint256 {
        if (it->hash.IsNull())
            it->hash = recursiveMerkleHash(it.data(), it.children(), process);
        return it->hash;
    };
    return process(it);
}

std::vector<uint256> ComputeMerklePath(const std::vector<uint256>& hashes, uint32_t idx)
{
    uint32_t count = 0;
    int matchlevel = -1;
    bool matchh = false;
    uint256 inner[32], h;
    const uint32_t one = 1;
    std::vector<uint256> res;

    const auto iterateInner = [&](int& level) {
        for (; !(count & (one << level)); level++) {
            const auto& ihash = inner[level];
            if (matchh) {
                res.push_back(ihash);
            } else if (matchlevel == level) {
                res.push_back(h);
                matchh = true;
            }
            h = Hash(ihash.begin(), ihash.end(), h.begin(), h.end());
        }
    };

    while (count < hashes.size()) {
        h = hashes[count];
        matchh = count == idx;
        count++;
        int level = 0;
        iterateInner(level);
        // Store the resulting hash at inner position level.
        inner[level] = h;
        if (matchh)
            matchlevel = level;
    }

    int level = 0;
    while (!(count & (one << level)))
        level++;

    h = inner[level];
    matchh = matchlevel == level;

    while (count != (one << level)) {
        // If we reach this point, h is an inner value that is not the top.
        if (matchh)
            res.push_back(h);

        h = Hash(h.begin(), h.end(), h.begin(), h.end());
        // Increment count to the value it would have if two entries at this
        count += (one << level);
        level++;
        iterateInner(level);
    }
    return res;
}

bool CClaimTrieCacheHashFork::getProofForName(const std::string& name, CClaimTrieProof& proof)
{
    return getProofForName(name, proof, nullptr);
}

bool CClaimTrieCacheHashFork::getProofForName(const std::string& name, CClaimTrieProof& proof, const std::function<bool(const CClaimValue&)>& comp)
{
    if (nNextHeight < Params().GetConsensus().nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::getProofForName(name, proof);

    auto fillPairs = [&proof](const std::vector<uint256>& hashes, uint32_t idx) {
        auto partials = ComputeMerklePath(hashes, idx);
        for (int i = partials.size() - 1; i >= 0; --i)
            proof.pairs.emplace_back((idx >> i) & 1, partials[i]);
    };

    // cache the parent nodes
    cacheData(name, false);
    getMerkleHash();
    proof = CClaimTrieProof();
    for (auto& it : static_cast<const CClaimPrefixTrie&>(nodesToAddOrUpdate).nodes(name)) {
        std::vector<uint256> childHashes;
        uint32_t nextCurrentIdx = 0;
        for (auto& child : it.children()) {
            if (name.find(child.key()) == 0)
                nextCurrentIdx = uint32_t(childHashes.size());
            childHashes.push_back(child->hash);
        }

        std::vector<uint256> claimHashes;
        if (!it->empty())
            claimHashes = getClaimHashes(it.data());

        // I am on a node; I need a hash(children, claims)
        // if I am the last node on the list, it will be hash(children, x)
        // else it will be hash(x, claims)
        if (it.key() == name) {
            uint32_t nClaimIndex = 0;
            auto& claims = it->claims;
            auto itClaim = !comp ? claims.begin() : std::find_if(claims.begin(), claims.end(), comp);
            if (itClaim != claims.end()) {
                proof.hasValue = true;
                proof.outPoint = itClaim->outPoint;
                proof.nHeightOfLastTakeover = it->nHeightOfLastTakeover;
                nClaimIndex = std::distance(claims.begin(), itClaim);
            }
            auto hash = childHashes.empty() ? leafHash : ComputeMerkleRoot(childHashes);
            proof.pairs.emplace_back(true, hash);
            if (!claimHashes.empty())
                fillPairs(claimHashes, nClaimIndex);
        } else {
            auto hash = claimHashes.empty() ? emptyHash : ComputeMerkleRoot(claimHashes);
            proof.pairs.emplace_back(false, hash);
            if (!childHashes.empty())
                fillPairs(childHashes, nextCurrentIdx);
        }
    }
    std::reverse(proof.pairs.begin(), proof.pairs.end());
    return true;
}

void CClaimTrieCacheHashFork::copyAllBaseToCache()
{
    recurseNodes({}, [this](const std::string& name, const CClaimTrieData& data) {
        if (nodesAlreadyCached.insert(name).second)
            nodesToAddOrUpdate.insert(name, data);
    });

    for (auto it = nodesToAddOrUpdate.begin(); it != nodesToAddOrUpdate.end(); ++it) {
        it->hash.SetNull();
        it->flags |= CClaimTrieDataFlags::HASH_DIRTY;
    }
}

void CClaimTrieCacheHashFork::initializeIncrement()
{
    CClaimTrieCacheNormalizationFork::initializeIncrement();
    // we could do this in the constructor, but that would not allow for multiple increments in a row (as done in unit tests)
    if (nNextHeight != Params().GetConsensus().nAllClaimsInMerkleForkHeight - 1)
        return;

    // if we are forking, we load the entire base trie into the cache trie
    // we reset its hash computation so it can be recomputed completely
    copyAllBaseToCache();
}

bool CClaimTrieCacheHashFork::finalizeDecrement(std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    auto ret = CClaimTrieCacheNormalizationFork::finalizeDecrement(takeoverHeightUndo);
    if (ret && nNextHeight == Params().GetConsensus().nAllClaimsInMerkleForkHeight - 1)
        copyAllBaseToCache();
    return ret;
}
