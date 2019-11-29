
#include <forks.h>
#include <hashes.h>
#include <log.h>
#include <trie.h>

#include <tuple>

#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/locale/localization_backend.hpp>
#include <boost/scoped_ptr.hpp>

#define logPrint CLogPrint::global()

CClaimTrieCacheExpirationFork::CClaimTrieCacheExpirationFork(CClaimTrie* base) : CClaimTrieCacheBase(base)
{
    expirationHeight = nNextHeight;
}

int CClaimTrieCacheExpirationFork::expirationTime() const
{
    if (expirationHeight < base->nExtendedClaimExpirationForkHeight)
        return CClaimTrieCacheBase::expirationTime();
    return base->nExtendedClaimExpirationTime;
}

bool CClaimTrieCacheExpirationFork::incrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo,
        insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo, takeoverUndoType& takeoverUndo)
{
    if (CClaimTrieCacheBase::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverUndo)) {
        expirationHeight = nNextHeight;
        return true;
    }
    return false;
}

bool CClaimTrieCacheExpirationFork::decrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo, insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo)
{
    if (CClaimTrieCacheBase::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo)) {
        expirationHeight = nNextHeight;
        return true;
    }
    return false;
}

void CClaimTrieCacheExpirationFork::initializeIncrement()
{
    // we could do this in the constructor, but that would not allow for multiple increments in a row (as done in unit tests)
    if (nNextHeight != base->nExtendedClaimExpirationForkHeight)
        return;

    forkForExpirationChange(true);
}

bool CClaimTrieCacheExpirationFork::finalizeDecrement(takeoverUndoType& takeoverUndo)
{
    auto ret = CClaimTrieCacheBase::finalizeDecrement(takeoverUndo);
    if (ret && nNextHeight == base->nExtendedClaimExpirationForkHeight)
       forkForExpirationChange(false);
    return ret;
}

bool CClaimTrieCacheExpirationFork::forkForExpirationChange(bool increment)
{
    ensureTransacting();

    /*
    If increment is True, we have forked to extend the expiration time, thus items in the expiration queue
    will have their expiration extended by "new expiration time - original expiration time"

    If increment is False, we are decremented a block to reverse the fork. Thus items in the expiration queue
    will have their expiration extension removed.
    */

    auto height = nNextHeight;
    int extension = base->nExtendedClaimExpirationTime - base->nOriginalClaimExpirationTime;
    if (!increment) {
        height += extension;
        extension *= -1;
    }

    db << "UPDATE claims SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?" << extension << height;
    db << "UPDATE supports SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?" << extension << height;

    return true;
}

CClaimTrieCacheNormalizationFork::CClaimTrieCacheNormalizationFork(CClaimTrie* base) : CClaimTrieCacheExpirationFork(base)
{
    db.define("NORMALIZED", [this](const std::string& str) { return normalizeClaimName(str, true); });
}

bool CClaimTrieCacheNormalizationFork::shouldNormalize() const
{
    return nNextHeight > base->nNormalizedNameForkHeight;
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
        logPrint << "CClaimTrieCacheNormalizationFork::" << __func__ << "() is invalid or dependencies are missing: " << e.what() << Clog::endl;
        throw;
    } catch (const std::exception& e) { // TODO: change to use ... with current_exception() in c++11
        logPrint << "CClaimTrieCacheNormalizationFork::" << __func__ << "() had an unexpected exception: " << e.what() << Clog::endl;
        return name;
    }

    return normalized;
}

bool CClaimTrieCacheNormalizationFork::normalizeAllNamesInTrieIfNecessary(takeoverUndoType& takeoverUndo)
{
    ensureTransacting();

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
        std::unique_ptr<CUint160> takeoverID;
        row >> name >> takeoverHeight >> takeoverID;
        if (name.empty()) continue; // preserve our root node
        if (takeoverHeight > 0)
            takeoverUndo.emplace_back(name, std::make_pair(takeoverHeight, takeoverID ? *takeoverID : CUint160()));
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
    ensureTransacting();

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
    if (nNextHeight == base->nNormalizedNameForkHeight)
        normalizeAllNamesInTrieIfNecessary(takeoverUndo);
    return CClaimTrieCacheExpirationFork::incrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo, takeoverUndo);
}

bool CClaimTrieCacheNormalizationFork::decrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo, insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo)
{
    auto ret = CClaimTrieCacheExpirationFork::decrementBlock(insertUndo, expireUndo, insertSupportUndo, expireSupportUndo);
    if (ret && nNextHeight == base->nNormalizedNameForkHeight)
        unnormalizeAllNamesInTrieIfNecessary();
    return ret;
}

bool CClaimTrieCacheNormalizationFork::getProofForName(const std::string& name, const CUint160& claim, CClaimTrieProof& proof)
{
    return CClaimTrieCacheExpirationFork::getProofForName(normalizeClaimName(name), claim, proof);
}

bool CClaimTrieCacheNormalizationFork::getInfoForName(const std::string& name, CClaimValue& claim, int offsetHeight) const
{
    return CClaimTrieCacheExpirationFork::getInfoForName(normalizeClaimName(name), claim, offsetHeight);
}

CClaimSupportToName CClaimTrieCacheNormalizationFork::getClaimsForName(const std::string& name) const
{
    return CClaimTrieCacheExpirationFork::getClaimsForName(normalizeClaimName(name));
}

int CClaimTrieCacheNormalizationFork::getDelayForName(const std::string& name, const CUint160& claimId) const
{
    return CClaimTrieCacheExpirationFork::getDelayForName(normalizeClaimName(name), claimId);
}

std::string CClaimTrieCacheNormalizationFork::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return normalizeClaimName(name, validHeight > base->nNormalizedNameForkHeight);
}

CClaimTrieCacheHashFork::CClaimTrieCacheHashFork(CClaimTrie* base) : CClaimTrieCacheNormalizationFork(base)
{
}

static const auto leafHash = CUint256S("0000000000000000000000000000000000000000000000000000000000000002");
static const auto emptyHash = CUint256S("0000000000000000000000000000000000000000000000000000000000000003");

CUint256 ComputeMerkleRoot(std::vector<CUint256> hashes)
{
    while (hashes.size() > 1) {
        if (hashes.size() & 1)
            hashes.push_back(hashes.back());

        for (std::size_t i = 0, j = 0; i < hashes.size(); i += 2)
            hashes[j++] = Hash(hashes[i].begin(), hashes[i].end(),
                               hashes[i+1].begin(), hashes[i+1].end());

        hashes.resize(hashes.size() / 2);
    }
    return hashes.empty() ? CUint256{} : hashes[0];
}

CUint256 CClaimTrieCacheHashFork::recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly)
{
    if (nNextHeight < base->nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::recursiveComputeMerkleHash(name, takeoverHeight, checkOnly);

    // it may be that using RAM for this is more expensive than preparing a new query statement in each recursive call
    std::vector<std::tuple<std::string, std::unique_ptr<CUint256>, int>> children;
    childHashQuery << name >> [&children](std::string name, std::unique_ptr<CUint256> hash, int takeoverHeight) {
        children.push_back(std::make_tuple(std::move(name), std::move(hash), takeoverHeight));
    };
    childHashQuery++;
    std::vector<CUint256> childHashes;
    for (auto& child: children) {
        auto& name = std::get<0>(child);
        auto& hash = std::get<1>(child);
        if (!hash || hash->IsNull())
            childHashes.push_back(recursiveComputeMerkleHash(name, std::get<2>(child), checkOnly));
        else
            childHashes.push_back(*hash);
    }

    std::vector<CUint256> claimHashes;
    //if (takeoverHeight > 0) {
        for (auto &&row: claimHashQuery << nNextHeight << name) {
            CTxOutPoint p;
            row >> p.hash >> p.n;
            auto claimHash = getValueHash(p, takeoverHeight);
            claimHashes.push_back(claimHash);
        }
        claimHashQuery++;
    //}

    auto left = childHashes.empty() ? leafHash : ComputeMerkleRoot(std::move(childHashes));
    auto right = claimHashes.empty() ? emptyHash : ComputeMerkleRoot(std::move(claimHashes));

    auto computedHash = Hash(left.begin(), left.end(), right.begin(), right.end());
    if (!checkOnly)
        db << "UPDATE nodes SET hash = ? WHERE name = ?" << computedHash << name;
    return computedHash;
}

std::vector<CUint256> ComputeMerklePath(const std::vector<CUint256>& hashes, uint32_t idx)
{
    uint32_t count = 0;
    int matchlevel = -1;
    bool matchh = false;
    CUint256 inner[32], h;
    const uint32_t one = 1;
    std::vector<CUint256> res;

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

bool CClaimTrieCacheHashFork::getProofForName(const std::string& name, const CUint160& claim, CClaimTrieProof& proof)
{
    if (nNextHeight < base->nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::getProofForName(name, claim, proof);

    auto fillPairs = [&proof](const std::vector<CUint256>& hashes, uint32_t idx) {
        auto partials = ComputeMerklePath(hashes, idx);
        for (int i = partials.size() - 1; i >= 0; --i)
            proof.pairs.emplace_back((idx >> i) & 1, partials[i]);
    };

    // cache the parent nodes
    getMerkleHash();
    proof = CClaimTrieProof();
    for (auto&& row: db << proofClaimQuery << name) {
        std::string key;
        int takeoverHeight;
        row >> key >> takeoverHeight;
        uint32_t nextCurrentIdx = 0;
        std::vector<CUint256> childHashes;
        for (auto&& child : childHashQuery << key) {
            std::string childKey;
            CUint256 childHash;
            child >> childKey >> childHash;
            if (name.find(childKey) == 0)
                nextCurrentIdx = uint32_t(childHashes.size());
            childHashes.push_back(childHash);
        }
        childHashQuery++;

        std::vector<CUint256> claimHashes;
        uint32_t claimIdx = 0;
        for (auto&& child: claimHashQuery << nNextHeight << key) {
            CTxOutPoint childOutPoint;
            CUint160 childClaimID;
            child >> childOutPoint.hash >> childOutPoint.n >> childClaimID;
            if (childClaimID == claim && key == name) {
                claimIdx = uint32_t(claimHashes.size());
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
            auto hash = childHashes.empty() ? leafHash : ComputeMerkleRoot(std::move(childHashes));
            proof.pairs.emplace_back(true, hash);
            if (!claimHashes.empty())
                fillPairs(claimHashes, claimIdx);
        } else {
            auto hash = claimHashes.empty() ? emptyHash : ComputeMerkleRoot(std::move(claimHashes));
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
    if (nNextHeight == base->nAllClaimsInMerkleForkHeight - 1) {
        ensureTransacting();
        db << "UPDATE nodes SET hash = NULL";
    }
}

bool CClaimTrieCacheHashFork::finalizeDecrement(takeoverUndoType& takeoverUndo)
{
    auto ret = CClaimTrieCacheNormalizationFork::finalizeDecrement(takeoverUndo);
    if (ret && nNextHeight == base->nAllClaimsInMerkleForkHeight - 1) {
        ensureTransacting();
        db << "UPDATE nodes SET hash = NULL";
    }
    return ret;
}

bool CClaimTrieCacheHashFork::allowSupportMetadata() const
{
    return nNextHeight >= base->nAllClaimsInMerkleForkHeight;
}