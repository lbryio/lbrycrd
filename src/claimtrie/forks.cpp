
#include <forks.h>
#include <hashes.h>
#include <log.h>
#include <trie.h>

#include <iomanip>
#include <sstream>
#include <string>

#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/locale/localization_backend.hpp>

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

bool CClaimTrieCacheExpirationFork::incrementBlock()
{
    if (CClaimTrieCacheBase::incrementBlock()) {
        expirationHeight = nNextHeight;
        return true;
    }
    return false;
}

bool CClaimTrieCacheExpirationFork::decrementBlock()
{
    if (CClaimTrieCacheBase::decrementBlock()) {
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

bool CClaimTrieCacheExpirationFork::finalizeDecrement()
{
    auto ret = CClaimTrieCacheBase::finalizeDecrement();
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

    db << "UPDATE claim SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?" << extension << height;
    db << "UPDATE support SET expirationHeight = expirationHeight + ? WHERE expirationHeight >= ?" << extension << height;

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

bool CClaimTrieCacheNormalizationFork::normalizeAllNamesInTrieIfNecessary()
{
    ensureTransacting();

    // make the new nodes
    db << "INSERT INTO node(name) SELECT NORMALIZED(name) AS nn FROM claim WHERE nn != nodeName "
          "AND activationHeight <= ?1 AND expirationHeight > ?1 ON CONFLICT(name) DO UPDATE SET hash = NULL, claimsHash = NULL" << nNextHeight;

    // there's a subtlety here: names in supports don't make new nodes
    db << "UPDATE node SET hash = NULL, claimsHash = NULL WHERE name IN "
          "(SELECT NORMALIZED(name) AS nn FROM support WHERE nn != nodeName "
          "AND activationHeight <= ?1 AND expirationHeight > ?1)" << nNextHeight;

    // update the claims and supports
    db << "UPDATE claim SET nodeName = NORMALIZED(name) WHERE activationHeight <= ?1 AND expirationHeight > ?1" << nNextHeight;
    db << "UPDATE support SET nodeName = NORMALIZED(name) WHERE activationHeight <= ?1 AND expirationHeight > ?1" << nNextHeight;

    // remove the old nodes
    db << "UPDATE node SET hash = NULL, claimsHash = NULL WHERE name NOT IN "
          "(SELECT nodeName FROM claim WHERE activationHeight <= ?1 AND expirationHeight > ?1 "
          "UNION SELECT nodeName FROM support WHERE activationHeight <= ?1 AND expirationHeight > ?1)" << nNextHeight;

    // work around a bug in the old implementation:
    db << "UPDATE claim SET activationHeight = ?1 " // force a takeover on these
          "WHERE updateHeight < ?1 AND activationHeight > ?1 AND nodeName != name" << nNextHeight;

    return true;
}

bool CClaimTrieCacheNormalizationFork::unnormalizeAllNamesInTrieIfNecessary()
{
    ensureTransacting();

    db << "INSERT INTO node(name) SELECT name FROM claim WHERE name != nodeName "
          "AND activationHeight < ?1 AND expirationHeight > ?1 ON CONFLICT(name) DO UPDATE SET hash = NULL, claimsHash = NULL" << nNextHeight;

    db << "UPDATE node SET hash = NULL, claimsHash = NULL WHERE name IN "
          "(SELECT nodeName FROM support WHERE name != nodeName "
          "UNION SELECT nodeName FROM claim WHERE name != nodeName)";

    db << "UPDATE claim SET nodeName = name";
    db << "UPDATE support SET nodeName = name";

    // we need to let the tree structure method do the actual node delete
    db << "UPDATE node SET hash = NULL, claimsHash = NULL WHERE name NOT IN "
          "(SELECT DISTINCT name FROM claim)";

    return true;
}

bool CClaimTrieCacheNormalizationFork::incrementBlock()
{
    if (nNextHeight == base->nNormalizedNameForkHeight)
        normalizeAllNamesInTrieIfNecessary();
    return CClaimTrieCacheExpirationFork::incrementBlock();
}

bool CClaimTrieCacheNormalizationFork::decrementBlock()
{
    auto ret = CClaimTrieCacheExpirationFork::decrementBlock();
    if (ret && nNextHeight == base->nNormalizedNameForkHeight)
        unnormalizeAllNamesInTrieIfNecessary();
    return ret;
}

bool CClaimTrieCacheNormalizationFork::getProofForName(const std::string& name, const uint160& claim, CClaimTrieProof& proof)
{
    return CClaimTrieCacheExpirationFork::getProofForName(normalizeClaimName(name), claim, proof);
}

bool CClaimTrieCacheNormalizationFork::getInfoForName(const std::string& name, CClaimValue& claim, int offsetHeight)
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
    return normalizeClaimName(name, validHeight > base->nNormalizedNameForkHeight);
}

CClaimTrieCacheHashFork::CClaimTrieCacheHashFork(CClaimTrie* base) : CClaimTrieCacheNormalizationFork(base)
{
}

static const auto emptyTrieHash = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
static const auto leafHash = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
static const auto emptyHash = uint256S("0000000000000000000000000000000000000000000000000000000000000003");

uint256 ComputeMerkleRoot(std::vector<uint256> hashes)
{
    while (hashes.size() > 1) {
        if (hashes.size() & 1)
            hashes.push_back(hashes.back());

        sha256n_way(hashes);
        hashes.resize(hashes.size() / 2);
    }
    return hashes.empty() ? uint256{} : hashes[0];
}

std::vector<uint256> CClaimTrieCacheHashFork::childrenHashes(const std::string& name, const std::function<void(const std::string&)>& callback)
{
    using row_type = sqlite::row_iterator::reference;
    std::function<void(row_type)> visitor;
    if (callback)
        visitor = [&callback](row_type row) {
            std::string childName;
            row >> childName;
            callback(childName);
        };
    else
        visitor = [](row_type row) {
            /* name */ row.index()++;
        };
    std::vector<uint256> childHashes;
    for (auto&& row: childHashQuery << name) {
        visitor(row);
        row >> *childHashes.emplace(childHashes.end());
    }
    childHashQuery++;
    return childHashes;
}

std::vector<uint256> CClaimTrieCacheHashFork::claimsHashes(const std::string& name, int takeoverHeight, const std::function<void(const CClaimInfo&)>& callback)
{
    using row_type = sqlite::row_iterator::reference;
    std::function<COutPoint(row_type)> visitor;
    if (callback)
        visitor = [&callback](row_type row) -> COutPoint {
            CClaimInfo info;
            row >> info.outPoint.hash >> info.outPoint.n
                >> info.claimId >> info.updateHeight;
            // activationHeight, amount
            row.index() += 2;
            row >> info.originalHeight;
            callback(info);
            return info.outPoint;
        };
    else
        visitor = [](row_type row) -> COutPoint {
            COutPoint outPoint;
            row >> outPoint.hash >> outPoint.n;
            return outPoint;
        };
    std::vector<uint256> claimHashes;
    for (auto&& row: claimHashQuery << nNextHeight << name)
        claimHashes.push_back(getValueHash(visitor(row), takeoverHeight));
    claimHashQuery++;
    return claimHashes;
}

uint256 CClaimTrieCacheHashFork::computeNodeHash(const std::string& name, uint256& claimsHash, int takeoverHeight)
{
    if (nNextHeight < base->nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::computeNodeHash(name, claimsHash, takeoverHeight);

    auto childHashes = childrenHashes(name);

    if (takeoverHeight > 0) {
        if (claimsHash.IsNull()) {
            auto hashes = claimsHashes(name, takeoverHeight);
            claimsHash = hashes.empty() ? emptyHash : ComputeMerkleRoot(std::move(hashes));
        }
    } else {
        claimsHash = emptyHash;
    }

    if (name.empty() && childHashes.empty() && claimsHash == emptyHash
        && base->nMaxRemovalWorkaroundHeight < 0) // detecting regtest, but maybe all on next hard-fork?
        return emptyTrieHash; // here for compatibility with the functional tests

    const auto& childrenHash = childHashes.empty() ? leafHash : ComputeMerkleRoot(std::move(childHashes));

    return Hash2(childrenHash, claimsHash);
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

extern const std::string proofClaimQuery_s;

bool CClaimTrieCacheHashFork::getProofForName(const std::string& name, const uint160& claimId, CClaimTrieProof& proof)
{
    if (nNextHeight < base->nAllClaimsInMerkleForkHeight)
        return CClaimTrieCacheNormalizationFork::getProofForName(name, claimId, proof);

    auto fillPairs = [&proof](const std::vector<uint256>& hashes, uint32_t idx) {
        auto partials = ComputeMerklePath(hashes, idx);
        for (int i = partials.size() - 1; i >= 0; --i)
            proof.pairs.emplace_back((idx >> i) & 1, partials[i]);
    };

    // cache the parent nodes
    getMerkleHash();
    proof = CClaimTrieProof();
    for (auto&& row: db << proofClaimQuery_s << name) {
        std::string key;
        int takeoverHeight;
        row >> key >> takeoverHeight;

        uint32_t childIdx = 0, idx = 0;
        auto childHashes = childrenHashes(key, [&](const std::string& childKey) {
            if (name.find(childKey) == 0)
                childIdx = idx;
            ++idx;
        });

        uint32_t claimIdx = 0; idx = 0;
        std::function<void(const CClaimInfo&)> visitor;
        if (key == name)
            visitor = [&](const CClaimInfo& info) {
                if (info.claimId == claimId) {
                    claimIdx = idx;
                    proof.hasValue = true;
                    proof.outPoint = info.outPoint;
                }
                ++idx;
            };
        auto claimHashes = claimsHashes(key, takeoverHeight, visitor);

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
                fillPairs(childHashes, childIdx);
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
        db << "UPDATE node SET hash = NULL, claimsHash = NULL";
    }
}

bool CClaimTrieCacheHashFork::finalizeDecrement()
{
    auto ret = CClaimTrieCacheNormalizationFork::finalizeDecrement();
    if (ret && nNextHeight == base->nAllClaimsInMerkleForkHeight - 1) {
        ensureTransacting();
        db << "UPDATE node SET hash = NULL, claimsHash = NULL";
    }
    return ret;
}

bool CClaimTrieCacheHashFork::allowSupportMetadata() const
{
    return nNextHeight >= base->nAllClaimsInMerkleForkHeight;
}

CClaimTrieCacheClaimInfoHashFork::CClaimTrieCacheClaimInfoHashFork(CClaimTrie* base) : CClaimTrieCacheHashFork(base)
{
}

extern std::vector<unsigned char> heightToVch(int n);

// NOTE: the name is supposed to be the final one
// normalized or not is the caller responsibility
uint256 claimInfoHash(const std::string& name, const COutPoint& outPoint, int bid, int seq, int nHeightOfLastTakeover)
{
    auto hash = Hash2(Hash(name), Hash(outPoint.hash));
    hash = Hash2(hash, std::to_string(outPoint.n));
    hash = Hash2(hash, std::to_string(bid));
    hash = Hash2(hash, std::to_string(seq));
    return Hash2(hash, heightToVch(nHeightOfLastTakeover));
}

inline std::size_t indexOf(const std::vector<CClaimInfo>& infos, const CClaimInfo& info)
{
    return std::distance(infos.begin(), std::find(infos.begin(), infos.end(), info));
}

std::vector<uint256> CClaimTrieCacheClaimInfoHashFork::claimsHashes(const std::string& name, int takeoverHeight, const std::function<void(const CClaimInfo&)>& callback)
{
    if (nNextHeight < base->nClaimInfoInMerkleForkHeight)
        return CClaimTrieCacheHashFork::claimsHashes(name, takeoverHeight, callback);

    std::vector<CClaimInfo> claimsByBid;
    for (auto&& row: claimHashQuery << nNextHeight << name) {
        auto& cb = *claimsByBid.emplace(claimsByBid.end());
        row >> cb.outPoint.hash >> cb.outPoint.n
            >> cb.claimId >> cb.updateHeight;
        // activationHeight, amount
        row.index() += 2;
        row >> cb.originalHeight;
        if (callback) callback(cb);
    }
    claimHashQuery++;

    auto claimsBySeq = claimsByBid;
    std::sort(claimsBySeq.begin(), claimsBySeq.end());
    std::vector<uint256> claimsHashes;
    for (auto i = 0u; i < claimsByBid.size(); ++i) {
        auto& cb = claimsByBid[i];
        claimsHashes.push_back(claimInfoHash(name, cb.outPoint, i, indexOf(claimsBySeq, cb), takeoverHeight));
    }
    return claimsHashes;
}

void CClaimTrieCacheClaimInfoHashFork::initializeIncrement()
{
    CClaimTrieCacheHashFork::initializeIncrement();
    // we could do this in the constructor, but that would not allow for multiple increments in a row (as done in unit tests)
    if (nNextHeight == base->nClaimInfoInMerkleForkHeight - 1) {
        ensureTransacting();
        db << "UPDATE node SET hash = NULL";
    }
}

bool CClaimTrieCacheClaimInfoHashFork::finalizeDecrement()
{
    auto ret = CClaimTrieCacheHashFork::finalizeDecrement();
    if (ret && nNextHeight == base->nClaimInfoInMerkleForkHeight - 1) {
        ensureTransacting();
        db << "UPDATE node SET hash = NULL";
    }
    return ret;
}
