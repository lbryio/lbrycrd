
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

bool CClaimTrieCacheExpirationFork::incrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo,
        insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo, takeoverUndoType& takeoverUndo)
{
    if (CClaimTrieCacheBase::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverUndo)) {
        setExpirationTime(Params().GetConsensus().GetExpirationTime(nNextHeight));
        return true;
    }
    return false;
}

bool CClaimTrieCacheExpirationFork::decrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo, insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo)
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

bool CClaimTrieCacheExpirationFork::finalizeDecrement(takeoverUndoType& takeoverUndo)
{
    auto ret = CClaimTrieCacheBase::finalizeDecrement(takeoverUndo);
    if (ret && nNextHeight == Params().GetConsensus().nExtendedClaimExpirationForkHeight)
       forkForExpirationChange(false);
    return ret;
}

bool CClaimTrieCacheExpirationFork::forkForExpirationChange(bool increment)
{
    if (!transacting) { transacting = true; db << "begin"; }

    /*
    If increment is True, we have forked to extend the expiration time, thus items in the expiration queue
    will have their expiration extended by "new expiration time - original expiration time"

    If increment is False, we are decremented a block to reverse the fork. Thus items in the expiration queue
    will have their expiration extension removed.
    */

    auto extension = Params().GetConsensus().nExtendedClaimExpirationTime - Params().GetConsensus().nOriginalClaimExpirationTime;
    if (increment) {
        db << "UPDATE claims SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?"
            << extension << nNextHeight;
        db << "UPDATE supports SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?"
            << extension << nNextHeight;
    }
    else {
        db << "UPDATE claims SET expirationHeight = expirationHeight - ? WHERE expirationHeight >= ?"
           << extension << nNextHeight + extension;
        db << "UPDATE supports SET expirationHeight = expirationHeight - ? WHERE expirationHeight >= ?"
           << extension << nNextHeight + extension;
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

bool CClaimTrieCacheNormalizationFork::normalizeAllNamesInTrieIfNecessary(takeoverUndoType& takeoverUndo)
{
    if (!transacting) { transacting = true; db << "begin"; }

    // run the one-time upgrade of all names that need to change
    db.define("NORMALIZED", [this](const std::string& str) { return normalizeClaimName(str, true); });

    // make the new nodes
    db << "INSERT INTO nodes(name) SELECT NORMALIZED(name) AS nn FROM claims WHERE nn != nodeName "
          "AND validHeight <= ?1 AND expirationHeight > ?1 ON CONFLICT(name) DO UPDATE SET hash = NULL" << nNextHeight;
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
          "(SELECT NORMALIZED(name) AS nn FROM supports WHERE nn != nodeName "
          "AND validHeight <= ?1 AND expirationHeight > ?1)" << nNextHeight;

    // update the claims and supports
    db << "UPDATE claims SET nodeName = NORMALIZED(name) WHERE validHeight <= ?1 AND expirationHeight > ?1" << nNextHeight;
    db << "UPDATE supports SET nodeName = NORMALIZED(name) WHERE validHeight <= ?1 AND expirationHeight > ?1" << nNextHeight;

    // remove the old nodes
    auto query = db << "SELECT name, IFNULL(takeoverHeight, 0), takeoverID FROM nodes WHERE name NOT IN "
                       "(SELECT nodeName FROM claims WHERE validHeight <= ?1 AND expirationHeight > ?1)";
    for (auto&& row: query) {
        std::string name;
        int takeoverHeight;
        std::unique_ptr<uint160> takeoverID;
        row >> name >> takeoverHeight >> takeoverID;
        if (name.empty()) continue; // preserve our root node
        if (takeoverHeight > 0)
            takeoverUndo.emplace_back(name, std::make_pair(takeoverHeight, takeoverID ? *takeoverID : uint160()));
        // we need to let the tree structure method do the actual node delete:
        db << "UPDATE nodes SET hash = NULL WHERE name = ?" << name;
    }
    db << "UPDATE nodes SET hash = NULL WHERE takeoverHeight IS NULL";

    // work around a bug in the old implementation:
    db << "UPDATE claims SET validHeight = ?1 " // force a takeover on these
          "WHERE blockHeight < ?1 AND validHeight > ?1 AND nodeName != name" << nNextHeight;

    return true;
}

bool CClaimTrieCacheNormalizationFork::unnormalizeAllNamesInTrieIfNecessary()
{
    if (!transacting) { transacting = true; db << "begin"; }

    // run the one-time upgrade of all names that need to change
    db.define("NORMALIZED", [this](const std::string& str) { return normalizeClaimName(str, true); });

    db << "INSERT INTO nodes(name) SELECT name FROM claims WHERE name != nodeName "
          "AND validHeight < ?1 AND expirationHeight > ?1 ON CONFLICT(name) DO UPDATE SET hash = NULL" << nNextHeight;
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
          "(SELECT name FROM supports WHERE name != nodeName UNION "
          "SELECT nodeName FROM supports WHERE name != nodeName UNION "
          "SELECT nodeName FROM claims WHERE name != nodeName)";

    db << "UPDATE claims SET nodeName = name";
    db << "UPDATE supports SET nodeName = name";
    // we need to let the tree structure method do the actual node delete
    db << "UPDATE nodes SET hash = NULL WHERE name NOT IN "
          "(SELECT name FROM claims)";

    return true;
}

bool CClaimTrieCacheNormalizationFork::incrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo,
        insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo, takeoverUndoType& takeoverUndo)
{
    if (nNextHeight == Params().GetConsensus().nNormalizedNameForkHeight)
        normalizeAllNamesInTrieIfNecessary(takeoverUndo);
    auto ret = CClaimTrieCacheExpirationFork::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverUndo);
//    if (nNextHeight == 588319) {
//        getMerkleHash();
//        auto q2 = db << "SELECT name, nodeName FROM claims WHERE nodeName NOT IN (SELECT name FROM nodes) "
//                        "AND validHeight < ?1 AND expirationHeight >= ?1" << nNextHeight;
//        for (auto&& row: q2) {
//            std::string name, nn;
//            row >> name >> nn;
//            LogPrintf("BAD NAME 2: %s, %s\n", name, nn);
//        }
//        std::ifstream input("dump588318.txt");
//        std::string line;
//        std::vector<std::string> lines;
//        while (std::getline(input, line)) {
//            lines.push_back(line);
//        }
//        std::sort(lines.begin(), lines.end());
//        auto q3 = db << "SELECT n.name, n.hash, IFNULL(n.takeoverHeight, 0), "
//                        "(SELECT COUNT(*) FROM claims c WHERE c.nodeName = n.name "
//                        "AND validHeight < ?1 AND expirationHeight >= ?1) as cc FROM nodes n ORDER BY n.name" << nNextHeight;
//        for (auto&& row: q3) {
//            std::string name; int takeover, childs; uint256 hash;
//            row >> name >> hash >> takeover >> childs;
//            std::string m = name + ", " + std::to_string(name.size()) + ", " + hash.GetHex() + ", " + std::to_string(takeover) + ", " + std::to_string(childs);
//            if (!std::binary_search(lines.begin(), lines.end(), m)) {
//                LogPrintf("BAD BAD: %s\n", m);
//            }
//        }
//        auto q4 = db << "SELECT n.name, n.parent FROM nodes n LEFT JOIN claims c ON n.name = c.nodeName LEFT JOIN nodes n2 ON n.name = n2.parent WHERE c.nodeName IS NULL AND n2.parent IS NULL";
//        for (auto&& row: q4) {
//            std::string name, nn;
//            row >> name >> nn;
//            LogPrintf("BAD NAME 3: %s, %s\n", name, nn);
//        }
//    }
    return ret;
}

bool CClaimTrieCacheNormalizationFork::decrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo, insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo)
{
    auto ret = CClaimTrieCacheExpirationFork::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo);
    if (ret && nNextHeight == Params().GetConsensus().nNormalizedNameForkHeight)
        unnormalizeAllNamesInTrieIfNecessary();
    return ret;
}

bool CClaimTrieCacheNormalizationFork::getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof)
{
    return CClaimTrieCacheExpirationFork::getProofForName(normalizeClaimName(name), finalClaim, proof);
}

bool CClaimTrieCacheNormalizationFork::getInfoForName(const std::string& name, CClaimValue& claim, int offsetHeight) const
{
    return CClaimTrieCacheExpirationFork::getInfoForName(normalizeClaimName(name), claim, offsetHeight);
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

uint256 CClaimTrieCacheHashFork::recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly)
{
    if (nNextHeight < Params().GetConsensus().nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::recursiveComputeMerkleHash(name, takeoverHeight, checkOnly);

    // it may be that using RAM for this is more expensive than preparing a new query statement in each recursive call
    struct Triple { std::string name; std::unique_ptr<uint256> hash; int takeoverHeight; };
    std::vector<Triple> children;
    for (auto&& row : childHashQuery << name) {
        children.emplace_back();
        auto& b = children.back();
        row >> b.name >> b.hash >> b.takeoverHeight;
    }
    childHashQuery++;

    std::vector<uint256> childHashes;
    for (auto& child: children) {
        if (child.hash == nullptr) child.hash = std::make_unique<uint256>();
        if (child.hash->IsNull()) {
            *child.hash = recursiveComputeMerkleHash(child.name, child.takeoverHeight, checkOnly);
        }
        childHashes.push_back(*child.hash);
    }

    std::vector<uint256> claimHashes;
    for (auto&& row: claimHashQuery << nNextHeight << name) {
        COutPoint p;
        row >> p.hash >> p.n;
        auto claimHash = getValueHash(p, takeoverHeight);
        claimHashes.push_back(claimHash);
    }
    claimHashQuery++;

    auto left = childHashes.empty() ? leafHash : ComputeMerkleRoot(childHashes);
    auto right = claimHashes.empty() ? emptyHash : ComputeMerkleRoot(claimHashes);

    auto computedHash = Hash(left.begin(), left.end(), right.begin(), right.end());
    if (!checkOnly)
        db << "UPDATE nodes SET hash = ? WHERE name = ?" << computedHash << name;
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
    auto nodeQuery = db << "SELECT name, IFNULL(takeoverHeight, 0) FROM nodes WHERE "
                                  "name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
                                  "SELECT POPS(p) FROM prefix WHERE p != '') SELECT p FROM prefix) "
                                  "ORDER BY name" << name;
    for (auto&& row: nodeQuery) {
        std::string key;;
        int takeoverHeight;
        row >> key >> takeoverHeight;
        std::vector<uint256> childHashes;
        uint32_t nextCurrentIdx = 0;
        for (auto&& child : childHashQuery << key) {
            std::string childKey;
            uint256 childHash;
            child >> childKey >> childHash;
            if (name.find(childKey) == 0)
                nextCurrentIdx = uint32_t(childHashes.size());
            childHashes.push_back(childHash);
        }
        childHashQuery++;

        std::vector<uint256> claimHashes;
        uint32_t finalClaimIdx = 0;
        for (auto&& child: claimHashQuery << nNextHeight << key) {
            COutPoint childOutPoint;
            uint160 childClaimID;
            child >> childOutPoint.hash >> childOutPoint.n >> childClaimID;
            if (childClaimID == finalClaim && key == name) {
                finalClaimIdx = uint32_t(claimHashes.size());
                proof.outPoint = childOutPoint;
                proof.hasValue = true;
            }
            claimHashes.push_back(getValueHash(childOutPoint, takeoverHeight));
        }
        claimHashQuery++;

        // I am on a node; I need a hash(children, claims)
        // if I am the last node on the list, it will be hash(children, x)
        // else it will be hash(x, claims)
        if (key == name) {
            proof.nHeightOfLastTakeover = takeoverHeight;
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
    if (nNextHeight == Params().GetConsensus().nAllClaimsInMerkleForkHeight - 1) {
        if (!transacting) { transacting = true; db << "begin"; }
        db << "UPDATE nodes SET hash = NULL";
    }
}

bool CClaimTrieCacheHashFork::finalizeDecrement(takeoverUndoType& takeoverUndo)
{
    auto ret = CClaimTrieCacheNormalizationFork::finalizeDecrement(takeoverUndo);
    if (ret && nNextHeight == Params().GetConsensus().nAllClaimsInMerkleForkHeight - 1) {
        if (!transacting) { transacting = true; db << "begin"; }
        db << "UPDATE nodes SET hash = NULL";
    }
    return ret;
}

bool CClaimTrieCacheHashFork::allowSupportMetadata() const
{
    return nNextHeight >= Params().GetConsensus().nAllClaimsInMerkleForkHeight;
}
