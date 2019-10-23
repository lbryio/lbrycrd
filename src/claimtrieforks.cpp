
#include <consensus/merkle.h>
#include <chainparams.h>
#include <claimtrie.h>
#include <hash.h>

#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/locale/localization_backend.hpp>
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

    auto extension = Params().GetConsensus().nExtendedClaimExpirationTime - Params().GetConsensus().nOriginalClaimExpirationTime;
    if (!increment) extension = -extension;
    base->_db << "UPDATE claims SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?" << extension << nNextHeight;
    base->_db << "UPDATE supports SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?" << extension << nNextHeight;
    base->_db << "UPDATE nodes SET hash = NULL, claimHash = NULL"; // recompute all hashes (as there aren't that many at this point)
    dirtyNodes = true;
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

bool CClaimTrieCacheNormalizationFork::normalizeAllNamesInTrieIfNecessary(bool forward)
{
    if (nNextHeight != Params().GetConsensus().nNormalizedNameForkHeight)
        return false;

    // run the one-time upgrade of all names that need to change
    // it modifies the (cache) trie as it goes, so we need to grab everything to be modified first

    base->_db.define("NORMALIZED", [this](const std::string& str) { return normalizeClaimName(str, true); });

    auto query = base->_db << "SELECT NORMALIZED(name), name, claimID as nn FROM claims HAVING nodeName != nn";
    for(auto&& row: query) {
        std::string newName, oldName;
        uint160 claimID;
        row >> newName >> oldName >> claimID;
        if (!forward) std::swap(newName, oldName);
        base->_db << "UPDATE claims SET nodeName = ? WHERE claimID = ?" << newName << claimID;
        base->_db << "DELETE FROM nodes WHERE name = ?" << oldName;
        base->_db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT DO UPDATE hash = NULL, claimHash = NULL" << newName;
    }
    dirtyNodes = true;

    return true;
}

bool CClaimTrieCacheNormalizationFork::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    normalizeAllNamesInTrieIfNecessary(true);
    return CClaimTrieCacheExpirationFork::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverHeightUndo);
}

bool CClaimTrieCacheNormalizationFork::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo)
{
    auto ret = CClaimTrieCacheExpirationFork::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo);
    if (ret)
        normalizeAllNamesInTrieIfNecessary(false);
    return ret;
}

bool CClaimTrieCacheNormalizationFork::getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof)
{
    return CClaimTrieCacheExpirationFork::getProofForName(normalizeClaimName(name), finalClaim, proof);
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

uint256 CClaimTrieCacheHashFork::recursiveComputeMerkleHash(const std::string& name, int lastTakeoverHeight, bool checkOnly)
{
    if (nNextHeight < Params().GetConsensus().nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::recursiveComputeMerkleHash(name, lastTakeoverHeight, checkOnly);

    auto childQuery = base->_db << "SELECT name, hash, lastTakeoverHeight FROM nodes WHERE parent = ? ORDER BY name" << name;

    std::vector<uint256> childHashes;
    for (auto&& row: childQuery) {
        std::string key;
        int keyLastTakeoverHeight;
        std::unique_ptr<uint256> hash;
        row >> key >> hash >> keyLastTakeoverHeight;
        if (hash == nullptr) hash = std::make_unique<uint256>();
        if (hash->IsNull()) {
            *hash = recursiveComputeMerkleHash(key, keyLastTakeoverHeight, checkOnly);
        }
        childHashes.push_back(*hash);
    }

    auto claimQuery = base->_db << "SELECT c.txID, c.txN, c.validHeight, c.amount + "
                      "SUM(SELECT s.amount FROM supports s WHERE s.supportedClaimID = c.claimID AND s.validHeight < ?) as effectiveAmount"
                      "FROM claims c WHERE c.nodeName = ? AND c.validHeight < ? "
                      "ORDER BY effectiveAmount DESC, c.blockHeight, c.txID, c.txN" << nNextHeight << name << nNextHeight;

    std::vector<uint256> claimHashes;
    for (auto&& row: claimQuery) {
        COutPoint p;
        row >> p.hash >> p.n;
        auto claimHash = getValueHash(p, lastTakeoverHeight);
        claimHashes.push_back(claimHash);
    }

    auto left = childHashes.empty() ? leafHash : ComputeMerkleRoot(childHashes);
    auto right = claimHashes.empty() ? emptyHash : ComputeMerkleRoot(claimHashes);

    auto computedHash = Hash(left.begin(), left.end(), right.begin(), right.end());
    if (!checkOnly)
        base->_db << "UPDATE nodes SET hash = ? WHERE name = ?" << computedHash << name;
    return computedHash;
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

bool CClaimTrieCacheHashFork::getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof)
{
    if (nNextHeight < Params().GetConsensus().nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::getProofForName(name, finalClaim, proof);

    auto fillPairs = [&proof](const std::vector<uint256>& hashes, uint32_t idx) {
        auto partials = ComputeMerklePath(hashes, idx);
        for (int i = partials.size() - 1; i >= 0; --i)
            proof.pairs.emplace_back((idx >> i) & 1, partials[i]);
    };

    // cache the parent nodes
    getMerkleHash();
    proof = CClaimTrieProof();
    auto nodeQuery = base->_db << "SELECT name, lastTakeoverHeight FROM nodes WHERE "
                                  "name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
                                  "SELECT SUBSTR(p, 0, LENGTH(p)) FROM prefix WHERE p != '') SELECT p FROM prefix) "
                                  "ORDER BY LENGTH(name)" << name;
    for (auto&& row: nodeQuery) {
        std::string key; int lastTakeover;
        row >> key >> lastTakeover;
        std::vector<uint256> childHashes;
        uint32_t nextCurrentIdx = 0;
        auto childQuery = base->_db << "SELECT name, hash FROM nodes WHERE parent = ?" << key;
        for (auto&& child : childQuery) {
            std::string childKey;
            uint256 childHash;
            child >> childKey >> childHash;
            if (name.find(childKey) == 0)
                nextCurrentIdx = uint32_t(childHashes.size());
            childHashes.push_back(childHash);
        }

        auto cns = getClaimsForName(key);
        std::vector<uint256> claimHashes;
        uint32_t finalClaimIdx = 0;
        COutPoint finalOutPoint;
        for (uint32_t i = 0; i < cns.claimsNsupports.size(); ++i) {
            auto& child = cns.claimsNsupports[i].claim;
            claimHashes.push_back(getValueHash(child.outPoint, lastTakeover));
            if (child.claimId == finalClaim) {
                finalClaimIdx = i;
                finalOutPoint = child.outPoint;
            }
        }

        // I am on a node; I need a hash(children, claims)
        // if I am the last node on the list, it will be hash(children, x)
        // else it will be hash(x, claims)
        if (key == name) {
            proof.outPoint = finalOutPoint;
            proof.nHeightOfLastTakeover = lastTakeover;
            proof.hasValue = true;
            auto hash = childHashes.empty() ? leafHash : ComputeMerkleRoot(childHashes);
            proof.pairs.emplace_back(true, hash);
            if (!claimHashes.empty())
                fillPairs(claimHashes, finalClaimIdx);
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

void CClaimTrieCacheHashFork::initializeIncrement()
{
    CClaimTrieCacheNormalizationFork::initializeIncrement();
    // we could do this in the constructor, but that would not allow for multiple increments in a row (as done in unit tests)
    if (nNextHeight == Params().GetConsensus().nAllClaimsInMerkleForkHeight - 1)
        base->_db << "UPDATE nodes SET hash = NULL";
}

bool CClaimTrieCacheHashFork::finalizeDecrement(std::vector<std::pair<std::string, int>>& takeoverHeightUndo)
{
    auto ret = CClaimTrieCacheNormalizationFork::finalizeDecrement(takeoverHeightUndo);
    if (ret && nNextHeight == Params().GetConsensus().nAllClaimsInMerkleForkHeight - 1)
        base->_db << "UPDATE nodes SET hash = NULL";
    return ret;
}

bool CClaimTrieCacheHashFork::allowSupportMetadata() const
{
    return nNextHeight >= Params().GetConsensus().nAllClaimsInMerkleForkHeight;
}
