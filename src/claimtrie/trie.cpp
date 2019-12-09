
#include <forks.h>
#include <hashes.h>
#include <log.h>
#include <trie.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <iomanip>
#include <thread>
#include <tuple>

#include <boost/container/flat_map.hpp>

#define logPrint CLogPrint::global()

static const auto one = CUint256S("0000000000000000000000000000000000000000000000000000000000000001");

std::vector<unsigned char> heightToVch(int n)
{
    std::vector<uint8_t> vchHeight(8, 0);
    vchHeight[4] = n >> 24;
    vchHeight[5] = n >> 16;
    vchHeight[6] = n >> 8;
    vchHeight[7] = n;
    return vchHeight;
}

CUint256 getValueHash(const CTxOutPoint& outPoint, int nHeightOfLastTakeover)
{
    auto hash1 = Hash(outPoint.hash.begin(), outPoint.hash.end());
    auto snOut = std::to_string(outPoint.n);
    auto hash2 = Hash(snOut.begin(), snOut.end());
    auto vchHash = heightToVch(nHeightOfLastTakeover);
    auto hash3 = Hash(vchHash.begin(), vchHash.end());
    return Hash(hash1.begin(), hash1.end(), hash2.begin(), hash2.end(), hash3.begin(), hash3.end());
}

static const sqlite::sqlite_config sharedConfig {
    sqlite::OpenFlags::READWRITE | sqlite::OpenFlags::CREATE,
    nullptr, sqlite::Encoding::UTF8
};

CClaimTrie::CClaimTrie(bool fWipe, int height,
                       const std::string& dataDir,
                       int nNormalizedNameForkHeight,
                       int64_t nOriginalClaimExpirationTime,
                       int64_t nExtendedClaimExpirationTime,
                       int64_t nExtendedClaimExpirationForkHeight,
                       int64_t nAllClaimsInMerkleForkHeight,
                       int proportionalDelayFactor) :
                       nNextHeight(height),
                       dbFile(dataDir + "/claims.sqlite"), db(dbFile, sharedConfig),
                       nProportionalDelayFactor(proportionalDelayFactor),
                       nNormalizedNameForkHeight(nNormalizedNameForkHeight),
                       nOriginalClaimExpirationTime(nOriginalClaimExpirationTime),
                       nExtendedClaimExpirationTime(nExtendedClaimExpirationTime),
                       nExtendedClaimExpirationForkHeight(nExtendedClaimExpirationForkHeight),
                       nAllClaimsInMerkleForkHeight(nAllClaimsInMerkleForkHeight)
{
    db << "CREATE TABLE IF NOT EXISTS nodes (name TEXT NOT NULL PRIMARY KEY, "
          "parent TEXT REFERENCES nodes(name) DEFERRABLE INITIALLY DEFERRED, "
          "hash BLOB COLLATE BINARY)";

    db << "CREATE INDEX IF NOT EXISTS nodes_hash ON nodes (hash)";
    db << "CREATE INDEX IF NOT EXISTS nodes_parent ON nodes (parent)";

    db << "CREATE TABLE IF NOT EXISTS takeover (name TEXT NOT NULL, height INTEGER NOT NULL, "
           "claimID BLOB COLLATE BINARY, PRIMARY KEY(name, height));";

    db << "CREATE INDEX IF NOT EXISTS takeover_height ON takeover (height)";

    db << "CREATE TABLE IF NOT EXISTS claims (claimID BLOB NOT NULL COLLATE BINARY PRIMARY KEY, name TEXT NOT NULL, "
           "nodeName TEXT NOT NULL REFERENCES nodes(name) DEFERRABLE INITIALLY DEFERRED, "
           "txID BLOB NOT NULL COLLATE BINARY, txN INTEGER NOT NULL, blockHeight INTEGER NOT NULL, "
           "validHeight INTEGER NOT NULL, activationHeight INTEGER NOT NULL, "
           "expirationHeight INTEGER NOT NULL, amount INTEGER NOT NULL, metadata BLOB COLLATE BINARY);";

    db << "CREATE INDEX IF NOT EXISTS claims_activationHeight ON claims (activationHeight)";
    db << "CREATE INDEX IF NOT EXISTS claims_expirationHeight ON claims (expirationHeight)";
    db << "CREATE INDEX IF NOT EXISTS claims_nodeName ON claims (nodeName)";

    db << "CREATE TABLE IF NOT EXISTS supports (txID BLOB NOT NULL COLLATE BINARY, txN INTEGER NOT NULL, "
           "supportedClaimID BLOB NOT NULL COLLATE BINARY, name TEXT NOT NULL, nodeName TEXT NOT NULL, "
           "blockHeight INTEGER NOT NULL, validHeight INTEGER NOT NULL, activationHeight INTEGER NOT NULL, "
           "expirationHeight INTEGER NOT NULL, amount INTEGER NOT NULL, metadata BLOB COLLATE BINARY, PRIMARY KEY(txID, txN));";

    db << "CREATE INDEX IF NOT EXISTS supports_supportedClaimID ON supports (supportedClaimID)";
    db << "CREATE INDEX IF NOT EXISTS supports_activationHeight ON supports (activationHeight)";
    db << "CREATE INDEX IF NOT EXISTS supports_expirationHeight ON supports (expirationHeight)";
    db << "CREATE INDEX IF NOT EXISTS supports_nodeName ON supports (nodeName)";

    db << "PRAGMA cache_size=-" + std::to_string(5 * 1024); // in -KB
    db << "PRAGMA synchronous=OFF"; // don't disk sync after transaction commit
    db << "PRAGMA journal_mode=MEMORY";
    db << "PRAGMA temp_store=MEMORY";
    db << "PRAGMA case_sensitive_like=true";

    if (fWipe) {
        db << "DELETE FROM nodes";
        db << "DELETE FROM claims";
        db << "DELETE FROM supports";
        db << "DELETE FROM takeover";
    }

    db << "INSERT OR IGNORE INTO nodes(name, hash) VALUES('', ?)" << one; // ensure that we always have our root node
}

CClaimTrieCacheBase::~CClaimTrieCacheBase()
{
    if (transacting) {
        db << "rollback";
        transacting = false;
    }
    claimHashQuery.used(true);
    childHashQuery.used(true);
    claimHashQueryLimit.used(true);
}

bool CClaimTrie::SyncToDisk()
{
    // alternatively, switch to full sync after we are caught up on the chain
    auto rc = sqlite3_wal_checkpoint_v2(db.connection().get(), nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
    return rc == SQLITE_OK;
}

bool CClaimTrie::empty()
{
    int64_t count;
    db << "SELECT COUNT(*) FROM (SELECT 1 FROM claims WHERE activationHeight < ?1 AND expirationHeight >= ?1 LIMIT 1)" << nNextHeight >> count;
    return count == 0;
}

bool CClaimTrieCacheBase::haveClaim(const std::string& name, const CTxOutPoint& outPoint) const
{
    auto query = db << "SELECT 1 FROM claims WHERE nodeName = ?1 AND txID = ?2 AND txN = ?3 "
                        "AND activationHeight < ?4 AND expirationHeight >= ?4 LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    return query.begin() != query.end();
}

bool CClaimTrieCacheBase::haveSupport(const std::string& name, const CTxOutPoint& outPoint) const
{
    auto query = db << "SELECT 1 FROM supports WHERE nodeName = ?1 AND txID = ?2 AND txN = ?3 "
                        "AND activationHeight < ?4 AND expirationHeight >= ?4 LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    return query.begin() != query.end();
}

supportEntryType CClaimTrieCacheBase::getSupportsForName(const std::string& name) const
{
    // includes values that are not yet valid
    auto query = db << "SELECT supportedClaimID, txID, txN, blockHeight, activationHeight, amount "
                        "FROM supports WHERE nodeName = ? AND expirationHeight >= ?" << name << nNextHeight;
    supportEntryType ret;
    for (auto&& row: query) {
        CSupportValue value;
        row >> value.supportedClaimId >> value.outPoint.hash >> value.outPoint.n
            >> value.nHeight >> value.nValidAtHeight >> value.nAmount;
        ret.push_back(std::move(value));
    }
    return ret;
}

bool CClaimTrieCacheBase::haveClaimInQueue(const std::string& name, const CTxOutPoint& outPoint, int& nValidAtHeight) const
{
    auto query = db << "SELECT activationHeight FROM claims WHERE nodeName = ? AND txID = ? AND txN = ? "
                        "AND activationHeight >= ? AND expirationHeight >= activationHeight LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    for (auto&& row: query) {
        row >> nValidAtHeight;
        return true;
    }
    return false;
}

bool CClaimTrieCacheBase::haveSupportInQueue(const std::string& name, const CTxOutPoint& outPoint, int& nValidAtHeight) const
{
    auto query = db << "SELECT activationHeight FROM supports WHERE nodeName = ? AND txID = ? AND txN = ? "
                        "AND activationHeight >= ? AND expirationHeight >= activationHeight LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    for (auto&& row: query) {
        row >> nValidAtHeight;
        return true;
    }
    return false;
}

bool CClaimTrieCacheBase::deleteNodeIfPossible(const std::string& name, std::string& parent, int64_t& claims)
{
    if (name.empty()) return false;
    // to remove a node it must have one or less children and no claims
    db  << "SELECT COUNT(*) FROM (SELECT 1 FROM claims WHERE nodeName = ?1 AND activationHeight < ?2 AND expirationHeight >= ?2 LIMIT 1)"
        << name << nNextHeight >> claims;
    if (claims > 0) return false; // still has claims
    // we now know it has no claims, but we need to check its children
    int64_t count;
    std::string childName;
    // this line assumes that we've set the parent on child nodes already,
    // which means we are len(name) desc in our parent method
    db << "SELECT COUNT(*), MAX(name) FROM nodes WHERE parent = ?" << name >> std::tie(count, childName);
    if (count > 1) return false; // still has multiple children
    logPrint << "Removing node " << name << " with " << count << " children" << Clog::endl;
    // okay. it's going away
    auto query = db << "SELECT parent FROM nodes WHERE name = ?" << name;
    auto it = query.begin();
    if (it == query.end())
        return true; // we'll assume that whoever deleted this node previously cleaned things up correctly
    *it >> parent;
    db << "DELETE FROM nodes WHERE name = ?" << name;
    auto ret = db.rows_modified() > 0;
    if (ret && count == 1) // make the child skip us and point to its grandparent:
        db << "UPDATE nodes SET parent = ? WHERE name = ?" << parent << childName;
    if (ret)
        db << "UPDATE nodes SET hash = NULL WHERE name = ?" << parent;
    return ret;
}

void CClaimTrieCacheBase::ensureTreeStructureIsUpToDate()
{
    if (!transacting) return;

    // your children are your nodes that match your key but go at least one longer,
    // and have no trailing prefix in common with the other nodes in that set -- a hard query w/o parent field

    // when we get into this method, we have some claims that have been added, removed, and updated
    // those each have a corresponding node in the list with a null hash
    // some of our nodes will go away, some new ones will be added, some will be reparented


    // the plan: update all the claim hashes first
    std::vector<std::string> names;
    db  << "SELECT name FROM nodes WHERE hash IS NULL"
        >> [&names](std::string name) {
            names.push_back(std::move(name));
        };
    if (names.empty()) return; // nothing to do
    std::sort(names.begin(), names.end()); // guessing this is faster than "ORDER BY name"

    // there's an assumption that all nodes with claims are here; we do that as claims are inserted

    // assume parents are not set correctly here:
    auto parentQuery = db << "SELECT MAX(name) FROM nodes WHERE "
                              "name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
                              "SELECT POPS(p) FROM prefix WHERE p != '') SELECT p FROM prefix)";

    auto insertQuery = db << "INSERT INTO nodes(name, parent, hash) VALUES(?, ?, NULL) "
                             "ON CONFLICT(name) DO UPDATE SET parent = excluded.parent, hash = NULL";

    auto nodesQuery = db << "SELECT name FROM nodes WHERE parent = ? ORDER BY name";
    auto updateQuery = db << "UPDATE nodes SET parent = ? WHERE name = ?";

    for (auto& name: names) {
        int64_t claims;
        std::string parent, node;
        for (node = name; deleteNodeIfPossible(node, parent, claims);)
            node = parent;
        if (node != name || name.empty() || claims <= 0)
            continue; // if you have no claims but we couldn't delete you, you must have legitimate children

        parentQuery << name.substr(0, name.size() - 1);
        auto queryIt = parentQuery.begin();
        if (queryIt != parentQuery.end())
            *queryIt >> parent;
        else
            parent.clear();
        parentQuery++; // reusing knocks off about 10% of the query time

        // we know now that we need to insert it,
        // but we may need to insert a parent node for it first (also called a split)
        const auto psize = parent.size() + 1;
        for (auto&& row : nodesQuery << parent) {
            std::string sibling; row >> sibling;
            if (sibling.compare(0, psize, name, 0, psize) != 0)
                continue;
            auto splitPos = psize;
            while(splitPos < sibling.size() && splitPos < name.size() && sibling[splitPos] == name[splitPos])
                ++splitPos;
            auto newNodeName = name.substr(0, splitPos);
            // update the to-be-fostered sibling:
            updateQuery << newNodeName << sibling;
            updateQuery++;
            if (splitPos == name.size())
                // our new node is the same as the one we wanted to insert
                break;
            // insert the split node:
            logPrint << "Inserting split node " << newNodeName << " near " << sibling << ", parent " << parent << Clog::endl;
            insertQuery << newNodeName << parent;
            insertQuery++;

            parent = std::move(newNodeName);
            break;
        }
        nodesQuery++;

        logPrint << "Inserting or updating node " << name << ", parent " << parent << Clog::endl;
        insertQuery << name << parent;
        insertQuery++;
    }

    nodesQuery.used(true);
    updateQuery.used(true);
    parentQuery.used(true);
    insertQuery.used(true);

    // now we need to percolate the nulls up the tree
    // parents should all be set right
    db << "UPDATE nodes SET hash = NULL WHERE name IN (WITH RECURSIVE prefix(p) AS "
          "(SELECT parent FROM nodes WHERE hash IS NULL UNION SELECT parent FROM prefix, nodes "
          "WHERE name = prefix.p AND prefix.p != '') SELECT p FROM prefix)";
}

std::size_t CClaimTrieCacheBase::getTotalNamesInTrie() const
{
    // you could do this select from the nodes table, but you would have to ensure it is not dirty first
    std::size_t ret;
    db << "SELECT COUNT(DISTINCT nodeName) FROM claims WHERE activationHeight < ?1 AND expirationHeight >= ?1"
        << nNextHeight >> ret;
    return ret;
}

std::size_t CClaimTrieCacheBase::getTotalClaimsInTrie() const
{
    std::size_t ret;
    db << "SELECT COUNT(*) FROM claims WHERE activationHeight < ?1 AND expirationHeight >= ?1"
        << nNextHeight >> ret;
    return ret;
}

int64_t CClaimTrieCacheBase::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    int64_t ret = 0;
    const std::string query = fControllingOnly ?
        "SELECT SUM(amount) FROM (SELECT c.amount as amount, "
        "(SELECT(SELECT IFNULL(SUM(s.amount),0)+c.amount FROM supports s "
        "WHERE s.supportedClaimID = c.claimID AND c.nodeName = s.nodeName "
        "AND s.activationHeight < ?1 AND s.expirationHeight >= ?1) as effective "
        "ORDER BY effective DESC LIMIT 1) as winner FROM claims c "
        "WHERE c.activationHeight < ?1 AND c.expirationHeight >= ?1 GROUP BY c.nodeName)"
    :
        "SELECT SUM(amount) FROM (SELECT c.amount as amount "
        "FROM claims c WHERE c.activationHeight < ?1 AND c.expirationHeight >= ?1)";

    db << query << nNextHeight >> ret;
    return ret;
}

bool CClaimTrieCacheBase::getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset) const
{
    auto ret = false;
    auto nextHeight = nNextHeight + heightOffset;
    for (auto&& row: claimHashQueryLimit << nextHeight << name) {
        row >> claim.outPoint.hash >> claim.outPoint.n >> claim.claimId
            >> claim.nHeight >> claim.nValidAtHeight >> claim.nAmount >> claim.nEffectiveAmount;
        ret = true;
        break;
    }
    claimHashQueryLimit++;
    return ret;
}

CClaimSupportToName CClaimTrieCacheBase::getClaimsForName(const std::string& name) const
{
    CUint160 claimId;
    int nLastTakeoverHeight = 0;
    getLastTakeoverForName(name, claimId, nLastTakeoverHeight);

    auto supports = getSupportsForName(name);
    auto find = [&supports](decltype(supports)::iterator& it, const CClaimValue& claim) {
        it = std::find_if(it, supports.end(), [&claim](const CSupportValue& support) {
            return claim.claimId == support.supportedClaimId;
        });
        return it != supports.end();
    };

    auto query = db << "SELECT claimID, txID, txN, blockHeight, activationHeight, amount "
                        "FROM claims WHERE nodeName = ? AND expirationHeight >= ?"
                    << name << nNextHeight;

    // match support to claim
    std::vector<CClaimNsupports> claimsNsupports;
    for (auto &&row: query) {
        CClaimValue claim;
        row >> claim.claimId >> claim.outPoint.hash >> claim.outPoint.n
            >> claim.nHeight >> claim.nValidAtHeight >> claim.nAmount;
        int64_t nAmount = claim.nValidAtHeight < nNextHeight ? claim.nAmount : 0;
        auto ic = claimsNsupports.emplace(claimsNsupports.end(), claim, nAmount);
        for (auto it = supports.begin(); find(it, claim); it = supports.erase(it)) {
            if (it->nValidAtHeight < nNextHeight)
                ic->effectiveAmount += it->nAmount;
            ic->supports.push_back(std::move(*it));
        }
        ic->claim.nEffectiveAmount = ic->effectiveAmount;
    }
    std::sort(claimsNsupports.rbegin(), claimsNsupports.rend());
    return {name, nLastTakeoverHeight, std::move(claimsNsupports), std::move(supports)};
}

void completeHash(CUint256& partialHash, const std::string& key, int to)
{
    for (auto it = key.rbegin(); std::distance(it, key.rend()) > to + 1; ++it)
        partialHash = Hash(it, it + 1, partialHash.begin(), partialHash.end());
}

CUint256 CClaimTrieCacheBase::computeNodeHash(const std::string& name, int takeoverHeight)
{
    const auto pos = name.size();
    std::vector<uint8_t> vchToHash;
    // we have to free up the hash query so it can be reused by a child
    childHashQuery << name >> [&vchToHash, pos](std::string name, CUint256 hash) {
        completeHash(hash, name, pos);
        vchToHash.push_back(name[pos]);
        vchToHash.insert(vchToHash.end(), hash.begin(), hash.end());
    };
    childHashQuery++;

    CClaimValue claim;
    if (getInfoForName(name, claim)) {
        auto valueHash = getValueHash(claim.outPoint, takeoverHeight);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    return vchToHash.empty() ? one : Hash(vchToHash.begin(), vchToHash.end());
}

bool CClaimTrieCacheBase::checkConsistency()
{
    // verify that all claims hash to the values on the nodes

    auto query = db << "SELECT n.name, n.hash, "
                        "IFNULL((SELECT CASE WHEN t.claimID IS NULL THEN 0 ELSE t.height END "
                        "FROM takeover t WHERE t.name = n.name ORDER BY t.height DESC LIMIT 1), 0) FROM nodes n";
    for (auto&& row: query) {
        std::string name;
        CUint256 hash;
        int takeoverHeight;
        row >> name >> hash >> takeoverHeight;
        auto computedHash = computeNodeHash(name, takeoverHeight);
        if (computedHash != hash)
            return false;
    }
    return true;
}

bool CClaimTrieCacheBase::validateDb(int height, const CUint256& rootHash)
{
    base->nNextHeight = nNextHeight = height + 1;

    logPrint << "Checking claim trie consistency... " << Clog::flush;
    if (checkConsistency()) {
        logPrint << "consistent" << Clog::endl;
        if (rootHash != getMerkleHash()) {
            logPrint << "CClaimTrieCacheBase::" << __func__ << "(): the block's root claim hash doesn't match the persisted claim root hash." << Clog::endl;
            return false;
        }
        return true;
    }
    logPrint << "inconsistent!" << Clog::endl;
    return false;
}

bool CClaimTrieCacheBase::flush()
{
    if (transacting) {
        getMerkleHash();
        do {
            try {
                db << "commit";
            } catch (const sqlite::sqlite_exception& e) {
                auto code = e.get_code();
                if (code == SQLITE_LOCKED || code == SQLITE_BUSY) {
                    logPrint << "Retrying the commit in one second." << Clog::endl;
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s);
                    continue;
                } else {
                    logPrint << "ERROR in ClaimTrieCache::" << __func__ << "(): " << e.what() << Clog::endl;
                    return false;
                }
            }
            break;
        } while (true);
        transacting = false;
    }
    base->nNextHeight = nNextHeight;
    removalWorkaround.clear();
    return true;
}

const std::string childHashQuery_s = "SELECT name, hash FROM nodes WHERE parent = ? ORDER BY name";

const std::string claimHashQuery_s =
    "SELECT c.txID, c.txN, c.claimID, c.blockHeight, c.activationHeight, c.amount, "
    "(SELECT IFNULL(SUM(s.amount),0)+c.amount FROM supports s "
    "WHERE s.supportedClaimID = c.claimID AND s.nodeName = c.nodeName "
    "AND s.activationHeight < ?1 AND s.expirationHeight >= ?1) as effectiveAmount "
    "FROM claims c WHERE c.nodeName = ?2 AND c.activationHeight < ?1 AND c.expirationHeight >= ?1 "
    "ORDER BY effectiveAmount DESC, c.blockHeight, c.txID, c.txN";

const std::string claimHashQueryLimit_s = claimHashQuery_s + " LIMIT 1";

extern const std::string proofClaimQuery_s =
    "SELECT n.name, IFNULL((SELECT CASE WHEN t.claimID IS NULL THEN 0 ELSE t.height END "
    "FROM takeover t WHERE t.name = n.name ORDER BY t.height DESC LIMIT 1), 0) FROM nodes n "
    "WHERE n.name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
    "SELECT POPS(p) FROM prefix WHERE p != '') SELECT p FROM prefix) "
    "ORDER BY n.name";

CClaimTrieCacheBase::CClaimTrieCacheBase(CClaimTrie* base)
    : base(base), db(base->dbFile, sharedConfig), transacting(false),
      childHashQuery(db << childHashQuery_s),
      claimHashQuery(db << claimHashQuery_s),
      claimHashQueryLimit(db << claimHashQueryLimit_s)
{
    assert(base);
    nNextHeight = base->nNextHeight;

    db << "PRAGMA cache_size=-" + std::to_string(200 * 1024); // in -KB
    db << "PRAGMA synchronous=OFF"; // don't disk sync after transaction commit
    db << "PRAGMA journal_mode=MEMORY";
    db << "PRAGMA temp_store=MEMORY";
    db << "PRAGMA case_sensitive_like=true";

    db.define("SIZE", [](const std::string& s) -> int { return s.size(); });
    db.define("POPS", [](std::string s) -> std::string { if (!s.empty()) s.pop_back(); return s; });
}

void CClaimTrieCacheBase::ensureTransacting()
{
    if (!transacting) {
        transacting = true;
        db << "begin";
    }
}

int CClaimTrieCacheBase::expirationTime() const
{
    return base->nOriginalClaimExpirationTime;
}

CUint256 CClaimTrieCacheBase::getMerkleHash()
{
    ensureTreeStructureIsUpToDate();
    CUint256 hash;
    db  << "SELECT hash FROM nodes WHERE name = ''"
        >> [&hash](std::unique_ptr<CUint256> rootHash) {
            if (rootHash)
                hash = std::move(*rootHash);
        };
    if (!hash.IsNull())
        return hash;
    assert(transacting); // no data changed but we didn't have the root hash there already?
    auto updateQuery = db << "UPDATE nodes SET hash = ? WHERE name = ?";
    db << "SELECT n.name, IFNULL((SELECT CASE WHEN t.claimID IS NULL THEN 0 ELSE t.height END FROM takeover t WHERE t.name = n.name "
            "ORDER BY t.height DESC LIMIT 1), 0) FROM nodes n WHERE n.hash IS NULL ORDER BY n.name DESC"
        >> [this, &hash, &updateQuery](const std::string& name, int takeoverHeight) {
            hash = computeNodeHash(name, takeoverHeight);
            updateQuery << hash << name;
            updateQuery++;
        };
    updateQuery.used(true);
    return hash;
}

bool CClaimTrieCacheBase::getLastTakeoverForName(const std::string& name, CUint160& claimId, int& takeoverHeight) const
{
    auto query = db << "SELECT t.height, t.claimID FROM takeover t "
                       "WHERE t.name = ?2 ORDER BY t.height DESC LIMIT 1" << nNextHeight << name;
    auto it = query.begin();
    if (it != query.end()) {
        std::unique_ptr<CUint160> claimIdOrNull;
        *it >> takeoverHeight >> claimIdOrNull;
        if (claimIdOrNull) {
            claimId = *claimIdOrNull;
            return true;
        }
    }
    return false;
}

bool CClaimTrieCacheBase::addClaim(const std::string& name, const CTxOutPoint& outPoint, const CUint160& claimId,
        int64_t nAmount, int nHeight, int nValidHeight, const std::vector<unsigned char>& metadata)
{
    ensureTransacting();

    // in the update scenario the previous one should be removed already
    // in the downgrade scenario, the one ahead will be removed already and the old one's valid height is input
    // revisiting the update scenario we have two options:
    // 1. let them pull the old one first, in which case they will be responsible to pass in validHeight (since we can't determine it's a 0 delay)
    // 2. don't remove the old one; have this method do a kinder "update" situation.
    // Option 2 has the issue in that we don't actually update if we don't have an existing match,
    // and no way to know that here without an 'update' flag
    // In addition, as we currently do option 1 they use that to get the old valid height and store that for undo
    // We would have to make this method return that if we go without the removal
    // The other problem with 1 is that the outer shell would need to know if the one they removed was a winner or not

    if (nValidHeight < 0)
        nValidHeight = nHeight + getDelayForName(name, claimId); // sets nValidHeight to the old value

    auto nodeName = adjustNameForValidHeight(name, nValidHeight);
    auto expires = expirationTime() + nHeight;

    db << "INSERT INTO claims(claimID, name, nodeName, txID, txN, amount, blockHeight, validHeight, activationHeight, expirationHeight, metadata) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        << claimId << name << nodeName << outPoint.hash << outPoint.n << nAmount << nHeight << nValidHeight << nValidHeight << expires << metadata;

    if (nValidHeight < nNextHeight)
        db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << nodeName;

    return true;
}

bool CClaimTrieCacheBase::addSupport(const std::string& name, const CTxOutPoint& outPoint, const CUint160& supportedClaimId,
        int64_t nAmount, int nHeight, int nValidHeight, const std::vector<unsigned char>& metadata)
{
    ensureTransacting();

    if (nValidHeight < 0)
        nValidHeight = nHeight + getDelayForName(name, supportedClaimId);

    auto nodeName = adjustNameForValidHeight(name, nValidHeight);
    auto expires = expirationTime() + nHeight;

    db << "INSERT INTO supports(supportedClaimID, name, nodeName, txID, txN, amount, blockHeight, validHeight, activationHeight, expirationHeight, metadata) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        << supportedClaimId << name << nodeName << outPoint.hash << outPoint.n << nAmount << nHeight << nValidHeight << nValidHeight << expires << metadata;

    if (nValidHeight < nNextHeight)
        db << "UPDATE nodes SET hash = NULL WHERE name = ?" << nodeName;

    return true;
}

bool CClaimTrieCacheBase::removeClaim(const CUint160& claimId, const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight)
{
    ensureTransacting();

    // this gets tricky in that we may be removing an update
    // when going forward we spend a claim (aka, call removeClaim) before updating it (aka, call addClaim)
    // when going backwards we first remove the update by calling removeClaim
    // we then undo the spend of the previous one by calling addClaim with the original data
    // in order to maintain the proper takeover height the udpater will need to use our height returned here

    auto query = db << "SELECT nodeName, activationHeight FROM claims WHERE claimID = ? AND txID = ? AND txN = ? AND expirationHeight >= ?"
                    << claimId << outPoint.hash << outPoint.n << nNextHeight;
    auto it = query.begin();
    if (it == query.end()) return false;
    *it >> nodeName >> validHeight;
    db  << "DELETE FROM claims WHERE claimID = ? AND txID = ? and txN = ?" << claimId << outPoint.hash << outPoint.n;
    if (!db.rows_modified())
        return false;
    db << "UPDATE nodes SET hash = NULL WHERE name = ?" << nodeName;

    // when node should be deleted from cache but instead it's kept
    // because it's a parent one and should not be effectively erased
    // we had a bug in the old code where that situation would force a zero delay on re-add
    if (true) { // TODO: hard fork this out (which we already tried once but failed)
        db << "SELECT nodeName FROM claims WHERE nodeName LIKE ?1 "
                "AND activationHeight < ?2 AND expirationHeight > ?2 ORDER BY nodeName LIMIT 1"
           << nodeName + '%' << nNextHeight
           >> [this, &nodeName](const std::string& shortestMatch) {
                if (shortestMatch != nodeName)
                    // set this when there are no more claims on that name and that node still has children
                    removalWorkaround.insert(nodeName);
            };
    }
    return true;
}

bool CClaimTrieCacheBase::removeSupport(const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight)
{
    ensureTransacting();

    auto query = db << "SELECT nodeName, activationHeight FROM supports WHERE txID = ? AND txN = ? AND expirationHeight >= ?"
                    << outPoint.hash << outPoint.n << nNextHeight;
    auto it = query.begin();
    if (it == query.end()) return false;
    *it >> nodeName >> validHeight;
    db << "DELETE FROM supports WHERE txID = ? AND txN = ?" << outPoint.hash << outPoint.n;
    if (!db.rows_modified())
        return false;
    db << "UPDATE nodes SET hash = NULL WHERE name = ?" << nodeName;
    return true;
}

static const boost::container::flat_map<std::pair<int, std::string>, int> takeoverWorkarounds = {
        {{496856, "HunterxHunterAMV"}, 496835},
        {{542978, "namethattune1"}, 542429},
        {{543508, "namethattune-5"}, 543306},
        {{546780, "forecasts"}, 546624},
        {{548730, "forecasts"}, 546780},
        {{551540, "forecasts"}, 548730},
        {{552380, "chicthinkingofyou"}, 550804},
        {{560363, "takephotowithlbryteam"}, 559962},
        {{563710, "test-img"}, 563700},
        {{566750, "itila"}, 543261},
        {{567082, "malabarismo-com-bolas-de-futebol-vs-chap"}, 563592},
        {{596860, "180mphpullsthrougheurope"}, 596757},
        {{617743, "vaccines"}, 572756},
        {{619609, "copface-slamshandcuffedteengirlintoconcrete"}, 539940},
        {{620392, "banker-exposes-satanic-elite"}, 597788},
        {{624997, "direttiva-sulle-armi-ue-in-svizzera-di"}, 567908},
        {{624997, "best-of-apex"}, 585580},
        {{629970, "cannot-ignore-my-veins"}, 629914},
        {{633058, "bio-waste-we-programmed-your-brain"}, 617185},
        {{633601, "macrolauncher-overview-first-look"}, 633058},
        {{640186, "its-up-to-you-and-i-2019"}, 639116},
        {{640241, "tor-eas-3-20"}, 592645},
        {{640522, "seadoxdark"}, 619531},
        {{640617, "lbry-przewodnik-1-instalacja"}, 451186},
        {{640623, "avxchange-2019-the-next-netflix-spotify"}, 606790},
        {{640684, "algebra-introduction"}, 624152},
        {{640684, "a-high-school-math-teacher-does-a"}, 600885},
        {{640684, "another-random-life-update"}, 600884},
        {{640684, "who-is-the-taylor-series-for"}, 600882},
        {{640684, "tedx-talk-released"}, 612303},
        {{640730, "e-mental"}, 615375},
        {{641143, "amiga-1200-bespoke-virgin-cinema"}, 623542},
        {{641161, "dreamscape-432-omega"}, 618894},
        {{641162, "2019-topstone-carbon-force-etap-axs-bike"}, 639107},
        {{641186, "arin-sings-big-floppy-penis-live-jazz-2"}, 638904},
        {{641421, "edward-snowden-on-bitcoin-and-privacy"}, 522729},
        {{641421, "what-is-libra-facebook-s-new"}, 598236},
        {{641421, "what-are-stablecoins-counter-party-risk"}, 583508},
        {{641421, "anthony-pomp-pompliano-discusses-crypto"}, 564416},
        {{641421, "tim-draper-crypto-invest-summit-2019"}, 550329},
        {{641421, "mass-adoption-and-what-will-it-take-to"}, 549781},
        {{641421, "dragonwolftech-youtube-channel-trailer"}, 567128},
        {{641421, "naomi-brockwell-s-weekly-crypto-recap"}, 540006},
        {{641421, "blockchain-based-youtube-twitter"}, 580809},
        {{641421, "andreas-antonopoulos-on-privacy-privacy"}, 533522},
        {{641817, "mexico-submits-and-big-tech-worsens"}, 582977},
        {{641817, "why-we-need-travel-bans"}, 581354},
        {{641880, "censored-by-patreon-bitchute-shares"}, 482460},
        {{641880, "crypto-wonderland"}, 485218},
        {{642168, "1-diabolo-julio-cezar-16-cbmcp-freestyle"}, 374999},
        {{642314, "tough-students"}, 615780},
        {{642697, "gamercauldronep2"}, 642153},
        {{643406, "the-most-fun-i-ve-had-in-a-long-time"}, 616506},
        {{643893, "spitshine69-and-uk-freedom-audits"}, 616876},
        {{644480, "my-mum-getting-attacked-a-duck"}, 567624},
        {{644486, "the-cryptocurrency-experiment"}, 569189},
        {{644486, "tag-you-re-it"}, 558316},
        {{644486, "orange-county-mineral-society-rock-and"}, 397138},
        {{644486, "sampling-with-the-gold-rush-nugget"}, 527960},
        {{644562, "september-15-21-a-new-way-of-doing"}, 634792},
        {{644562, "july-week-3-collective-frequency-general"}, 607942},
        {{644562, "september-8-14-growing-up-general"}, 630977},
        {{644562, "august-4-10-collective-frequency-general"}, 612307},
        {{644562, "august-11-17-collective-frequency"}, 617279},
        {{644562, "september-1-7-gentle-wake-up-call"}, 627104},
        {{644607, "no-more-lol"}, 643497},
        {{644607, "minion-masters-who-knew"}, 641313},
        {{645236, "danganronpa-3-the-end-of-hope-s-peak"}, 644153},
        {{645348, "captchabot-a-discord-bot-to-protect-your"}, 592810},
        {{645701, "the-xero-hour-saint-greta-of-thunberg"}, 644081},
        {{645701, "batman-v-superman-theological-notions"}, 590189},
        {{645918, "emacs-is-great-ep-0-init-el-from-org"}, 575666},
        {{645918, "emacs-is-great-ep-1-packages"}, 575666},
        {{645918, "emacs-is-great-ep-40-pt-2-hebrew"}, 575668},
        {{645923, "nasal-snuff-review-osp-batch-2"}, 575658},
        {{645923, "why-bit-coin"}, 575658},
        {{645929, "begin-quest"}, 598822},
        {{645929, "filthy-foe"}, 588386},
        {{645929, "unsanitary-snow"}, 588386},
        {{645929, "famispam-1-music-box"}, 588386},
        {{645929, "running-away"}, 598822},
        {{645931, "my-beloved-chris-madsen"}, 589114},
        {{645931, "space-is-consciousness-chris-madsen"}, 589116},
        {{645947, "gasifier-rocket-stove-secondary-burn"}, 590595},
        {{645949, "mouse-razer-abyssus-v2-e-mousepad"}, 591139},
        {{645949, "pr-temporada-2018-league-of-legends"}, 591138},
        {{645949, "windows-10-build-9901-pt-br"}, 591137},
        {{645949, "abrindo-pacotes-do-festival-lunar-2018"}, 591139},
        {{645949, "unboxing-camisetas-personalizadas-play-e"}, 591138},
        {{645949, "abrindo-envelopes-do-festival-lunar-2017"}, 591138},
        {{645951, "grub-my-grub-played-guruku-tersayang"}, 618033},
        {{645951, "ismeeltimepiece"}, 618038},
        {{645951, "thoughts-on-doom"}, 596485},
        {{645951, "thoughts-on-god-of-war-about-as-deep-as"}, 596485},
        {{645956, "linux-lite-3-6-see-what-s-new"}, 645195},
        {{646191, "kahlil-gibran-the-prophet-part-1"}, 597637},
        {{646551, "crypto-market-crash-should-you-sell-your"}, 442613},
        {{646551, "live-crypto-trading-and-market-analysis"}, 442615},
        {{646551, "5-reasons-trading-is-always-better-than"}, 500850},
        {{646551, "digitex-futures-dump-panic-selling-or"}, 568065},
        {{646552, "how-to-install-polarr-on-kali-linux-bynp"}, 466235},
        {{646586, "electoral-college-kids-civics-lesson"}, 430818},
        {{646602, "grapes-full-90-minute-watercolour"}, 537108},
        {{646602, "meizu-mx4-the-second-ubuntu-phone"}, 537109},
        {{646609, "how-to-set-up-the-ledger-nano-x"}, 569992},
        {{646609, "how-to-buy-ethereum"}, 482354},
        {{646609, "how-to-install-setup-the-exodus-multi"}, 482356},
        {{646609, "how-to-manage-your-passwords-using"}, 531987},
        {{646609, "cryptodad-s-live-q-a-friday-may-3rd-2019"}, 562303},
        {{646638, "resident-evil-ada-chapter-5-final"}, 605612},
        {{646639, "taurus-june-2019-career-love-tarot"}, 586910},
        {{646652, "digital-bullpen-ep-5-building-a-digital"}, 589274},
        {{646661, "sunlight"}, 591076},
        {{646661, "grasp-lab-nasa-open-mct-series"}, 589414},
        {{646663, "bunnula-s-creepers-tim-pool-s-beanie-a"}, 599669},
        {{646663, "bunnula-music-hey-ya-by-outkast"}, 605685},
        {{646663, "bunnula-tv-s-music-television-eunoia"}, 644437},
        {{646663, "the-pussy-centipede-40-sneakers-and"}, 587265},
        {{646663, "bunnula-reacts-ashton-titty-whitty"}, 596988},
        {{646677, "filip-reviews-jeromes-dream-cataracts-so"}, 589751},
        {{646691, "fascism-and-its-mobilizing-passions"}, 464342},
        {{646692, "hsb-color-layers-action-for-adobe"}, 586533},
        {{646692, "master-colorist-action-pack-extracting"}, 631830},
        {{646693, "how-to-protect-your-garden-from-animals"}, 588476},
        {{646693, "gardening-for-the-apocalypse-epic"}, 588472},
        {{646693, "my-first-bee-hive-foundationless-natural"}, 588469},
        {{646693, "dragon-fruit-and-passion-fruit-planting"}, 588470},
        {{646693, "installing-my-first-foundationless"}, 588469},
        {{646705, "first-naza-fpv"}, 590411},
        {{646717, "first-burning-man-2019-detour-034"}, 630247},
        {{646717, "why-bob-marley-was-an-idiot-test-driving"}, 477558},
        {{646717, "we-are-addicted-to-gambling-ufc-207-w"}, 481398},
        {{646717, "ghetto-swap-meet-selling-storage-lockers"}, 498291},
        {{646738, "1-kings-chapter-7-summary-and-what-god"}, 586599},
        {{646814, "brand-spanking-new-junior-high-school"}, 592378},
        {{646814, "lupe-fiasco-freestyle-at-end-of-the-weak"}, 639535},
        {{646824, "how-to-one-stroke-painting-doodles-mixed"}, 592404},
        {{646824, "acrylic-pouring-landscape-with-a-tree"}, 592404},
        {{646824, "how-to-make-a-diy-concrete-paste-planter"}, 595976},
        {{646824, "how-to-make-a-rustic-sand-planter-sand"}, 592404},
        {{646833, "3-day-festival-at-the-galilee-lake-and"}, 592842},
        {{646833, "rainbow-circle-around-the-noon-sun-above"}, 592842},
        {{646833, "energetic-self-control-demonstration"}, 623811},
        {{646833, "bees-congregating"}, 592842},
        {{646856, "formula-offroad-honefoss-sunday-track2"}, 592872},
        {{646862, "h3video1-dc-vs-mb-1"}, 593237},
        {{646862, "h3video1-iwasgoingto-load-up-gmod-but"}, 593237},
        {{646883, "watch-this-game-developer-make-a-video"}, 592593},
        {{646883, "how-to-write-secure-javascript"}, 592593},
        {{646883, "blockchain-technology-explained-2-hour"}, 592593},
        {{646888, "fl-studio-bits"}, 608155},
        {{646914, "andy-s-shed-live-s03e02-the-longest"}, 592200},
        {{646914, "gpo-telephone-776-phone-restoration"}, 592201},
        {{646916, "toxic-studios-co-stream-pubg"}, 597126},
        {{646916, "hyperlapse-of-prague-praha-from-inside"}, 597109},
        {{646933, "videobits-1"}, 597378},
        {{646933, "clouds-developing-daytime-8"}, 597378},
        {{646933, "slechtvalk-in-watertoren-bodegraven"}, 597378},
        {{646933, "timelapse-maansverduistering-16-juli"}, 605880},
        {{646933, "startrails-27"}, 597378},
        {{646933, "passing-clouds-daytime-3"}, 597378},
        {{646940, "nerdgasm-unboxing-massive-playing-cards"}, 597421},
        {{646946, "debunking-cops-volume-3-the-murder-of"}, 630570},
        {{646961, "kingsong-ks16x-electric-unicycle-250km"}, 636725},
        {{646968, "wild-mountain-goats-amazing-rock"}, 621940},
        {{646968, "no-shelter-backcountry-camping-in"}, 621940},
        {{646968, "can-i-live-in-this-through-winter-lets"}, 645750},
        {{646968, "why-i-wear-a-chest-rig-backcountry-or"}, 621940},
        {{646989, "marc-ivan-o-gorman-promo-producer-editor"}, 645656},
        {{647045, "@moraltis"}, 646367},
        {{647045, "moraltis-twitch-highlights-first-edit"}, 646368},
        {{647075, "the-3-massive-tinder-convo-mistakes"}, 629464},
        {{647075, "how-to-get-friend-zoned-via-text"}, 592298},
        {{647075, "don-t-do-this-on-tinder"}, 624591},
        {{647322, "world-of-tanks-7-kills"}, 609905},
        {{647322, "the-tier-6-auto-loading-swedish-meatball"}, 591338},
        {{647416, "hypnotic-soundscapes-garden-of-the"}, 596923},
        {{647416, "hypnotic-soundscapes-the-cauldron-sacred"}, 596928},
        {{647416, "schumann-resonance-to-theta-sweep"}, 596920},
        {{647416, "conversational-indirect-hypnosis-why"}, 596913},
        {{647493, "mimirs-brunnr"}, 590498},
        {{648143, "live-ita-completiamo-the-evil-within-2"}, 646568},
        {{648203, "why-we-love-people-that-hurt-us"}, 591128},
        {{648203, "i-didn-t-like-my-baby-and-considered"}, 591128},
        {{648220, "trade-talk-001-i-m-a-vlogger-now-fielder"}, 597303},
        {{648220, "vise-restoration-record-no-6-vise"}, 597303},
        {{648540, "amv-reign"}, 571863},
        {{648540, "amv-virus"}, 571863},
        {{648588, "audial-drift-(a-journey-into-sound)"}, 630217},
        {{648616, "quick-zbrush-tip-transpose-master-scale"}, 463205},
        {{648616, "how-to-create-3d-horns-maya-to-zbrush-2"}, 463205},
        {{648815, "arduino-based-cartridge-game-handheld"}, 593252},
        {{648815, "a-maze-update-3-new-game-modes-amazing"}, 593252},
        {{649209, "denmark-trip"}, 591428},
        {{649209, "stunning-4k-drone-footage"}, 591428},
        {{649215, "how-to-create-a-channel-and-publish-a"}, 414908},
        {{649215, "lbryclass-11-how-to-get-your-deposit"}, 632420},
        {{649543, "spring-break-madness-at-universal"}, 599698},
        {{649921, "navegador-brave-navegador-da-web-seguro"}, 649261},
        {{650191, "stream-intro"}, 591301},
        {{650946, "platelet-chan-fan-art"}, 584601},
        {{650946, "aqua-fanart"}, 584601},
        {{650946, "virginmedia-stores-password-in-plain"}, 619537},
        {{650946, "running-linux-on-android-teaser"}, 604441},
        {{650946, "hatsune-miku-ievan-polka"}, 600126},
        {{650946, "digital-security-and-privacy-2-and-a-new"}, 600135},
        {{650993, "my-editorial-comment-on-recent-youtube"}, 590305},
        {{650993, "drive-7-18-2018"}, 590305},
        {{651011, "old-world-put-on-realm-realms-gg"}, 591899},
        {{651011, "make-your-own-soundboard-with-autohotkey"}, 591899},
        {{651011, "ark-survival-https-discord-gg-ad26xa"}, 637680},
        {{651011, "minecraft-featuring-seus-8-just-came-4"}, 596488},
        {{651057, "found-footage-bikinis-at-the-beach-with"}, 593586},
        {{651057, "found-footage-sexy-mom-a-mink-stole"}, 593586},
        {{651067, "who-are-the-gentiles-gomer"}, 597094},
        {{651067, "take-back-the-kingdom-ep-2-450-million"}, 597094},
        {{651067, "mmxtac-implemented-footstep-sounds-and"}, 597094},
        {{651067, "dynasoul-s-blender-to-unreal-animated"}, 597094},
        {{651103, "calling-a-scammer-syntax-error"}, 612532},
        {{651103, "quick-highlight-of-my-day"}, 647651},
        {{651103, "calling-scammers-and-singing-christmas"}, 612531},
        {{651109, "@livingtzm"}, 637322},
        {{651109, "living-tzm-juuso-from-finland-september"}, 643412},
        {{651373, "se-voc-rir-ou-sorrir-reinicie-o-v-deo"}, 649302},
        {{651476, "what-is-pagan-online-polished-new-arpg"}, 592157},
        {{651476, "must-have-elder-scrolls-online-addons"}, 592156},
        {{651476, "who-should-play-albion-online"}, 592156},
        {{651730, "person-detection-with-keras-tensorflow"}, 621276},
        {{651730, "youtube-censorship-take-two"}, 587249},
        {{651730, "new-red-tail-shark-and-two-silver-sharks"}, 587251},
        {{651730, "around-auckland"}, 587250},
        {{651730, "humanism-in-islam"}, 587250},
        {{651730, "tigers-at-auckland-zoo"}, 587250},
        {{651730, "gravity-demonstration"}, 587250},
        {{651730, "copyright-question"}, 587249},
        {{651730, "uberg33k-the-ultimate-software-developer"}, 599522},
        {{651730, "chl-e-swarbrick-auckland-mayoral"}, 587250},
        {{651730, "code-reviews"}, 587249},
        {{651730, "raising-robots"}, 587251},
        {{651730, "teaching-python"}, 587250},
        {{651730, "kelly-tarlton-2016"}, 587250},
        {{652172, "where-is-everything"}, 589491},
        {{652172, "some-guy-and-his-camera"}, 617062},
        {{652172, "practical-information-pt-1"}, 589491},
        {{652172, "latent-vibrations"}, 589491},
        {{652172, "maldek-compilation"}, 589491},
        {{652444, "thank-you-etika-thank-you-desmond"}, 652121},
        {{652611, "plants-vs-zombies-gw2-20190827183609"}, 624339},
        {{652611, "wolfenstein-the-new-order-playthrough-6"}, 650299},
        {{652887, "a-codeigniter-cms-open-source-download"}, 652737},
        {{652966, "@pokesadventures"}, 632391},
        {{653009, "flat-earth-uk-convention-is-a-bust"}, 585786},
        {{653009, "flat-earth-reset-flat-earth-money-tree"}, 585786},
        {{653011, "veil-of-thorns-dispirit-brutal-leech-3"}, 652475},
        {{653069, "being-born-after-9-11"}, 632218},
        {{653069, "8-years-on-youtube-what-it-has-done-for"}, 637130},
        {{653069, "answering-questions-how-original"}, 521447},
        {{653069, "talking-about-my-first-comedy-stand-up"}, 583450},
        {{653069, "doing-push-ups-in-public"}, 650920},
        {{653069, "vlog-extra"}, 465997},
        {{653069, "crying-myself"}, 465997},
        {{653069, "xbox-rejection"}, 465992},
        {{653354, "msps-how-to-find-a-linux-job-where-no"}, 642537},
        {{653354, "windows-is-better-than-linux-vlog-it-and"}, 646306},
        {{653354, "luke-smith-is-wrong-about-everything"}, 507717},
        {{653354, "advice-for-those-starting-out-in-tech"}, 612452},
        {{653354, "treating-yourself-to-make-studying-more"}, 623561},
        {{653354, "lpi-linux-essential-dns-tools-vlog-what"}, 559464},
        {{653354, "is-learning-linux-worth-it-in-2019-vlog"}, 570886},
        {{653354, "huawei-linux-and-cellphones-in-2019-vlog"}, 578501},
        {{653354, "how-to-use-webmin-to-manage-linux"}, 511507},
        {{653354, "latency-concurrency-and-the-best-value"}, 596857},
        {{653354, "how-to-use-the-pomodoro-method-in-it"}, 506632},
        {{653354, "negotiating-compensation-vlog-it-and"}, 542317},
        {{653354, "procedural-goals-vs-outcome-goals-vlog"}, 626785},
        {{653354, "intro-to-raid-understanding-how-raid"}, 529341},
        {{653354, "smokeping"}, 574693},
        {{653354, "richard-stallman-should-not-be-fired"}, 634928},
        {{653354, "unusual-or-specialty-certifications-vlog"}, 620146},
        {{653354, "gratitude-and-small-projects-vlog-it"}, 564900},
        {{653354, "why-linux-on-the-smartphone-is-important"}, 649543},
        {{653354, "opportunity-costs-vlog-it-devops-career"}, 549708},
        {{653354, "double-giveaway-lpi-class-dates-and"}, 608129},
        {{653354, "linux-on-the-smartphone-in-2019-librem"}, 530426},
        {{653524, "celtic-folk-music-full-live-concert-mps"}, 589762},
        {{653745, "aftermath-of-the-mac"}, 592768},
        {{653745, "b-c-a-glock-17-threaded-barrel"}, 592770},
        {{653800, "middle-earth-shadow-of-mordor-by"}, 590229},
        {{654079, "tomand-jeremy-chirs45"}, 614296},
        {{654096, "achamos-carteira-com-grana-olha-o-que"}, 466262},
        {{654096, "viagem-bizarra-e-cansativa-ao-nordeste"}, 466263},
        {{654096, "tedio-na-tailandia-limpeza-de-area"}, 466265},
        {{654425, "schau-bung-2014-in-windischgarsten"}, 654410},
        {{654425, "mitternachtseinlage-ball-rk"}, 654410},
        {{654425, "zugabe-ball-rk-windischgarsten"}, 654412},
        {{654722, "skytrain-in-korea"}, 463145},
        {{654722, "luwak-coffee-the-shit-coffee"}, 463155},
        {{654722, "puppet-show-in-bangkok-thailand"}, 462812},
        {{654722, "kyaito-market-myanmar"}, 462813},
        {{654724, "wipeout-zombies-bo3-custom-zombies-1st"}, 589569},
        {{654724, "the-street-bo3-custom-zombies"}, 589544},
        {{654880, "wwii-airsoft-pow"}, 586968},
        {{654880, "dueling-geese-fight-to-the-death"}, 586968},
        {{654880, "wwii-airsoft-torgau-raw-footage-part4"}, 586968},
        {{655173, "april-2019-q-and-a"}, 554032},
        {{655173, "the-meaning-and-reality-of-individual"}, 607892},
        {{655173, "steven-pinker-progress-despite"}, 616984},
        {{655173, "we-make-stories-out-of-totem-poles"}, 549090},
        {{655173, "jamil-jivani-author-of-why-young-men"}, 542035},
        {{655173, "commentaries-on-jb-peterson-rebel-wisdom"}, 528898},
        {{655173, "auckland-clip-4-on-cain-and-abel"}, 629242},
        {{655173, "peterson-vs-zizek-livestream-tickets"}, 545285},
        {{655173, "auckland-clip-3-the-dawning-of-the-moral"}, 621154},
        {{655173, "religious-belief-and-the-enlightenment"}, 606269},
        {{655173, "auckland-lc-highlight-1-the-presumption"}, 565783},
        {{655173, "q-a-sir-roger-scruton-dr-jordan-b"}, 544184},
        {{655173, "cancellation-polish-national-foundation"}, 562529},
        {{655173, "the-coddling-of-the-american-mind-haidt"}, 440185},
        {{655173, "02-harris-weinstein-peterson-discussion"}, 430896},
        {{655173, "jordan-peterson-threatens-everything-of"}, 519737},
        {{655173, "on-claiming-belief-in-god-commentary"}, 581738},
        {{655173, "how-to-make-the-world-better-really-with"}, 482317},
        {{655173, "quillette-discussion-with-founder-editor"}, 413749},
        {{655173, "jb-peterson-on-free-thought-and-speech"}, 462849},
        {{655173, "marxism-zizek-peterson-official-video"}, 578453},
        {{655173, "patreon-problem-solution-dave-rubin-dr"}, 490394},
        {{655173, "next-week-st-louis-salt-lake-city"}, 445933},
        {{655173, "conversations-with-john-anderson-jordan"}, 529981},
        {{655173, "nz-australia-12-rules-tour-next-2-weeks"}, 518649},
        {{655173, "a-call-to-rebellion-for-ontario-legal"}, 285451},
        {{655173, "2016-personality-lecture-12"}, 578465},
        {{655173, "on-the-vital-necessity-of-free-speech"}, 427404},
        {{655173, "2017-01-23-social-justice-freedom-of"}, 578465},
        {{655173, "discussion-sam-harris-the-idw-and-the"}, 423332},
        {{655173, "march-2018-patreon-q-a"}, 413749},
        {{655173, "take-aim-even-badly"}, 490395},
        {{655173, "jp-f-wwbgo6a2w"}, 539940},
        {{655173, "patreon-account-deletion"}, 503477},
        {{655173, "canada-us-europe-tour-august-dec-2018"}, 413749},
        {{655173, "leaders-myth-reality-general-stanley"}, 514333},
        {{655173, "jp-ifi5kkxig3s"}, 539940},
        {{655173, "documentary-a-glitch-in-the-matrix-david"}, 413749},
        {{655173, "2017-08-14-patreon-q-and-a"}, 285451},
        {{655173, "postmodernism-history-and-diagnosis"}, 285451},
        {{655173, "23-minutes-from-maps-of-meaning-the"}, 413749},
        {{655173, "milo-forbidden-conversation"}, 578493},
        {{655173, "jp-wnjbasba-qw"}, 539940},
        {{655173, "uk-12-rules-tour-october-and-november"}, 462849},
        {{655173, "2015-maps-of-meaning-10-culture-anomaly"}, 578465},
        {{655173, "ayaan-hirsi-ali-islam-mecca-vs-medina"}, 285452},
        {{655173, "jp-f9393el2z1i"}, 539940},
        {{655173, "campus-indoctrination-the-parasitization"}, 285453},
        {{655173, "jp-owgc63khcl8"}, 539940},
        {{655173, "the-death-and-resurrection-of-christ-a"}, 413749},
        {{655173, "01-harris-weinstein-peterson-discussion"}, 430896},
        {{655173, "enlightenment-now-steven-pinker-jb"}, 413749},
        {{655173, "the-lindsay-shepherd-affair-update"}, 413749},
        {{655173, "jp-g3fwumq5k8i"}, 539940},
        {{655173, "jp-evvs3l-abv4"}, 539940},
        {{655173, "former-australian-deputy-pm-john"}, 413750},
        {{655173, "message-to-my-korean-readers-90-seconds"}, 477424},
        {{655173, "jp--0xbomwjkgm"}, 539940},
        {{655173, "ben-shapiro-jordan-peterson-and-a-12"}, 413749},
        {{655173, "jp-91jwsb7zyhw"}, 539940},
        {{655173, "deconstruction-the-lindsay-shepherd"}, 299272},
        {{655173, "september-patreon-q-a"}, 285451},
        {{655173, "jp-2c3m0tt5kce"}, 539940},
        {{655173, "australia-s-john-anderson-dr-jordan-b"}, 413749},
        {{655173, "jp-hdrlq7dpiws"}, 539940},
        {{655173, "stephen-hicks-postmodernism-reprise"}, 578480},
        {{655173, "october-patreon-q-a"}, 285451},
        {{655173, "an-animated-intro-to-truth-order-and"}, 413749},
        {{655173, "jp-bsh37-x5rny"}, 539940},
        {{655173, "january-2019-q-a"}, 503477},
        {{655173, "comedians-canaries-and-coalmines"}, 498586},
        {{655173, "the-democrats-apology-and-promise"}, 465433},
        {{655173, "jp-s4c-jodptn8"}, 539940},
        {{655173, "2014-personality-lecture-16-extraversion"}, 578465},
        {{655173, "dr-jordan-b-peterson-on-femsplainers"}, 490395},
        {{655173, "higher-ed-our-cultural-inflection-point"}, 527291},
        {{655173, "archetype-reality-friendship-and"}, 519736},
        {{655173, "sir-roger-scruton-dr-jordan-b-peterson"}, 490395},
        {{655173, "jp-cf2nqmqifxc"}, 539940},
        {{655173, "penguin-uk-12-rules-for-life"}, 413749},
        {{655173, "march-2019-q-and-a"}, 537138},
        {{655173, "jp-ne5vbomsqjc"}, 539940},
        {{655173, "dublin-london-harris-murray-new-usa-12"}, 413749},
        {{655173, "12-rules-12-cities-tickets-now-available"}, 413749},
        {{655173, "jp-j9j-bvdrgdi"}, 539940},
        {{655173, "responsibility-conscience-and-meaning"}, 499123},
        {{655173, "04-harris-murray-peterson-discussion"}, 436678},
        {{655173, "jp-ayhaz9k008q"}, 539940},
        {{655173, "with-jocko-willink-the-catastrophe-of"}, 490395},
        {{655173, "interview-with-the-grievance-studies"}, 501296},
        {{655173, "russell-brand-jordan-b-peterson-under"}, 413750},
        {{655173, "goodbye-to-patreon"}, 496771},
        {{655173, "revamped-podcast-announcement-with"}, 540943},
        {{655173, "swedes-want-to-know"}, 285453},
        {{655173, "auckland-clip-2-the-four-fundamental"}, 607892},
        {{655173, "jp-dtirzqmgbdm"}, 539940},
        {{655173, "political-correctness-a-force-for-good-a"}, 413750},
        {{655173, "sean-plunket-full-interview-new-zealand"}, 597638},
        {{655173, "q-a-the-meaning-and-reality-of"}, 616984},
        {{655173, "lecture-and-q-a-with-jordan-peterson-the"}, 413749},
        {{655173, "2017-personality-07-carl-jung-and-the"}, 578465},
        {{655173, "nina-paley-animator-extraordinaire"}, 413750},
        {{655173, "truth-as-the-antidote-to-suffering-with"}, 455127},
        {{655173, "bishop-barron-word-on-fire"}, 599814},
        {{655173, "zizek-vs-peterson-april-19"}, 527291},
        {{655173, "revamped-podcast-with-westwood-one"}, 540943},
        {{655173, "2016-11-19-university-of-toronto-free"}, 578465},
        {{655173, "jp-1emrmtrj5jc"}, 539940},
        {{655173, "who-is-joe-rogan-with-jordan-peterson"}, 585578},
        {{655173, "who-dares-say-he-believes-in-god"}, 581738},
        {{655252, "games-with-live2d"}, 589978},
        {{655252, "kaenbyou-rin-live2d"}, 589978},
        {{655374, "steam-groups-are-crazy"}, 607590},
        {{655379, "asmr-captain-falcon-happily-beats-you-up"}, 644574},
        {{655379, "pixel-art-series-5-link-holding-the"}, 442952},
        {{655379, "who-can-cross-the-planck-length-the-hero"}, 610830},
        {{655379, "ssbb-the-yoshi-grab-release-crash"}, 609747},
        {{655379, "tas-captain-falcon-s-bizarre-adventure"}, 442958},
        {{655379, "super-smash-bros-in-360-test"}, 442963},
        {{655379, "what-if-luigi-was-b-u-f-f"}, 442971},
        {{655803, "sun-time-lapse-test-7"}, 610634},
        {{655952, "upper-build-complete"}, 591728},
        {{656758, "cryptocurrency-awareness-adoption-the"}, 541770},
        {{656829, "3d-printing-technologies-comparison"}, 462685},
        {{656829, "3d-printing-for-everyone"}, 462685},
        {{657052, "tni-punya-ilmu-kanuragan-gaya-baru"}, 657045},
        {{657052, "papa-sunimah-nelpon-sri-utami-emon"}, 657045},
        {{657274, "rapforlife-4-win"}, 656856},
        {{657274, "bizzilion-proof-of-withdrawal"}, 656856},
        {{657420, "quick-drawing-prince-tribute-colored"}, 605630},
        {{657453, "white-boy-tom-mcdonald-facts"}, 597169},
        {{657453, "is-it-ok-to-look-when-you-with-your-girl"}, 610508},
        {{657584, "need-for-speed-ryzen-5-1600-gtx-1050-ti"}, 657161},
        {{657584, "quantum-break-ryzen-5-1600-gtx-1050-ti-4"}, 657161},
        {{657584, "nightcore-legends-never-die"}, 657161},
        {{657706, "mtb-enduro-ferragosto-2019-sestri"}, 638904},
        {{657706, "warface-free-for-all"}, 638908},
        {{657782, "nick-warren-at-loveland-but-not-really"}, 444299},
        {{658098, "le-temps-nous-glisse-entre-les-doigts"}, 600099},
};

bool CClaimTrieCacheBase::incrementBlock()
{
    // the plan:
    // for every claim and support that becomes active this block set its node hash to null (aka, dirty)
    // for every claim and support that expires this block set its node hash to null and add it to the expire(Support)Undo
    // for all dirty nodes look for new takeovers
    ensureTransacting();

    db << "INSERT INTO nodes(name) SELECT nodeName FROM claims INDEXED BY claims_activationHeight "
          "WHERE activationHeight = ?1 AND expirationHeight > ?1 "
          "ON CONFLICT(name) DO UPDATE SET hash = NULL"
          << nNextHeight;

    // don't make new nodes for items in supports or items that expire this block that don't exist in claims
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
          "(SELECT nodeName FROM claims WHERE expirationHeight = ?1 "
          "UNION SELECT nodeName FROM supports WHERE expirationHeight = ?1 OR activationHeight = ?1)"
          << nNextHeight;

    auto insertTakeoverQuery = db << "INSERT INTO takeover(name, height, claimID) VALUES(?, ?, ?)";

    // takeover handling:
    db  << "SELECT name FROM nodes WHERE hash IS NULL"
        >> [this, &insertTakeoverQuery](const std::string& nameWithTakeover) {
        // if somebody activates on this block and they are the new best, then everybody activates on this block
        CClaimValue candidateValue;
        auto hasCandidate = getInfoForName(nameWithTakeover, candidateValue, 1);
        // now that they're all in get the winner:
        CUint160 existingID;
        int existingHeight = 0;
        auto hasCurrentWinner = getLastTakeoverForName(nameWithTakeover, existingID, existingHeight);
        // we have a takeover if we had a winner and its changing or we never had a winner
        auto takeoverHappening = (hasCandidate && !hasCurrentWinner) || (hasCurrentWinner && existingID != candidateValue.claimId);

        if (takeoverHappening && activateAllFor(nameWithTakeover))
            hasCandidate = getInfoForName(nameWithTakeover, candidateValue, 1);

        // This is a super ugly hack to work around bug in old code.
        // The bug: un/support a name then update it. This will cause its takeover height to be reset to current.
        // This is because the old code with add to the cache without setting block originals when dealing in supports.
        if (nNextHeight < 658300) {
            auto wit = takeoverWorkarounds.find(std::make_pair(nNextHeight, nameWithTakeover));
            takeoverHappening |= wit != takeoverWorkarounds.end();
        }

        logPrint << "Takeover on " << nameWithTakeover << " at " << nNextHeight << ", happening: " << takeoverHappening << ", set before: " << hasCurrentWinner << Clog::endl;

        if (takeoverHappening) {
            if (hasCandidate)
                insertTakeoverQuery << nameWithTakeover << nNextHeight << candidateValue.claimId;
            else
                insertTakeoverQuery << nameWithTakeover << nNextHeight << nullptr;
            insertTakeoverQuery++;
        }
    };

    insertTakeoverQuery.used(true);

    nNextHeight++;
    return true;
}

bool CClaimTrieCacheBase::activateAllFor(const std::string& name)
{
    // now that we know a takeover is happening, we bring everybody in:
    auto ret = false;
    // all to activate now:
    db << "UPDATE claims SET activationHeight = ?1 WHERE nodeName = ?2 AND activationHeight > ?1 AND expirationHeight > ?1" << nNextHeight << name;
    ret |= db.rows_modified() > 0;

    // then do the same for supports:
    db << "UPDATE supports SET activationHeight = ?1 WHERE nodeName = ?2 AND activationHeight > ?1 AND expirationHeight > ?1" << nNextHeight << name;
    ret |= db.rows_modified() > 0;
    return ret;
}

bool CClaimTrieCacheBase::decrementBlock()
{
    ensureTransacting();

    nNextHeight--;

    db << "INSERT INTO nodes(name) SELECT nodeName FROM claims "
          "WHERE expirationHeight = ? ON CONFLICT(name) DO UPDATE SET hash = NULL"
          << nNextHeight;

    db << "UPDATE nodes SET hash = NULL WHERE name IN("
          "SELECT nodeName FROM supports WHERE expirationHeight = ?1 OR activationHeight = ?1 "
          "UNION SELECT nodeName FROM claims WHERE activationHeight = ?1)"
          << nNextHeight;

    db << "UPDATE claims SET activationHeight = validHeight WHERE activationHeight = ?"
          << nNextHeight;

    db << "UPDATE supports SET activationHeight = validHeight WHERE activationHeight = ?"
          << nNextHeight;

    return true;
}

bool CClaimTrieCacheBase::finalizeDecrement()
{
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
          "(SELECT nodeName FROM claims WHERE activationHeight = ?1 AND expirationHeight > ?1 "
          "UNION SELECT nodeName FROM supports WHERE activationHeight = ?1 AND expirationHeight > ?1 "
          "UNION SELECT name FROM takeover WHERE height = ?1)" << nNextHeight;

    db << "DELETE FROM takeover WHERE height >= ?" << nNextHeight;

    return true;
}

int CClaimTrieCacheBase::getDelayForName(const std::string& name, const CUint160& claimId) const
{
    CUint160 winningClaimId;
    int winningTakeoverHeight;
    auto hasCurrentWinner = getLastTakeoverForName(name, winningClaimId, winningTakeoverHeight);
    if (hasCurrentWinner && winningClaimId == claimId) {
        assert(winningTakeoverHeight <= nNextHeight);
        return 0;
    }

    // NOTE: old code had a bug in it where nodes with no claims but with children would get left in the cache after removal.
    // This would cause the getNumBlocksOfContinuousOwnership to return zero (causing incorrect takeover height calc).
    auto hit = removalWorkaround.find(name);
    if (hit != removalWorkaround.end()) {
        removalWorkaround.erase(hit);
        return 0;
    }
    return hasCurrentWinner ? std::min((nNextHeight - winningTakeoverHeight) / base->nProportionalDelayFactor, 4032) : 0;
}

std::string CClaimTrieCacheBase::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return name;
}

bool CClaimTrieCacheBase::getProofForName(const std::string& name, const CUint160& finalClaim, CClaimTrieProof& proof)
{
    // cache the parent nodes
    getMerkleHash();
    proof = CClaimTrieProof();
    for (auto&& row: db << proofClaimQuery_s << name) {
        CClaimValue claim;
        std::string key;
        int takeoverHeight;
        row >> key >> takeoverHeight;
        bool fNodeHasValue = getInfoForName(key, claim);
        CUint256 valueHash;
        if (fNodeHasValue)
            valueHash = getValueHash(claim.outPoint, takeoverHeight);

        const auto pos = key.size();
        std::vector<std::pair<unsigned char, CUint256>> children;
        for (auto&& child : childHashQuery << key) {
            std::string childKey;
            CUint256 hash;
            child >> childKey >> hash;
            if (name.find(childKey) == 0) {
                for (auto i = pos; i + 1 < childKey.size(); ++i) {
                    children.emplace_back(childKey[i], CUint256{});
                    proof.nodes.emplace_back(children, fNodeHasValue, valueHash);
                    children.clear();
                    valueHash.SetNull();
                    fNodeHasValue = false;
                }
                children.emplace_back(childKey.back(), CUint256{});
                continue;
            }
            completeHash(hash, childKey, pos);
            children.emplace_back(childKey[pos], hash);
        }
        childHashQuery++;
        if (key == name) {
            proof.hasValue = fNodeHasValue && claim.claimId == finalClaim;
            if (proof.hasValue) {
                proof.outPoint = claim.outPoint;
                proof.nHeightOfLastTakeover = takeoverHeight;
            }
            valueHash.SetNull();
        }
        proof.nodes.emplace_back(std::move(children), fNodeHasValue, valueHash);
    }
    return true;
}

bool CClaimTrieCacheBase::findNameForClaim(std::vector<unsigned char> claim, CClaimValue& value, std::string& name) const
{
    std::reverse(claim.begin(), claim.end());
    auto query = db << "SELECT nodeName, claimID, txID, txN, amount, activationHeight, blockHeight "
                        "FROM claims WHERE SUBSTR(claimID, ?1) = ?2 AND activationHeight < ?3 AND expirationHeight >= ?3"
                    << -int(claim.size()) << claim << nNextHeight;
    auto hit = false;
    for (auto&& row: query) {
        if (hit) return false;
        row >> name >> value.claimId >> value.outPoint.hash >> value.outPoint.n
            >> value.nAmount >> value.nValidAtHeight >> value.nHeight;
        hit = true;
    }
    return hit;
}

void CClaimTrieCacheBase::getNamesInTrie(std::function<void(const std::string&)> callback) const
{
    db  << "SELECT DISTINCT nodeName FROM claims WHERE activationHeight < ?1 AND expirationHeight >= ?1"
        << nNextHeight >> [&callback](const std::string& name) {
            callback(name);
        };
}
