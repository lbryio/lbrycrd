
#include <forks.h>
#include <hashes.h>
#include <log.h>
#include <sqlite.h>
#include <trie.h>

#include <algorithm>
#include <memory>

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

void applyPragmas(sqlite::database& db, std::size_t cache)
{
    db << "PRAGMA cache_size=-" + std::to_string(cache); // in -KB
    db << "PRAGMA synchronous=OFF"; // don't disk sync after transaction commit
    db << "PRAGMA journal_mode=MEMORY";
    db << "PRAGMA temp_store=MEMORY";
    db << "PRAGMA case_sensitive_like=true";
}

CClaimTrie::CClaimTrie(std::size_t cacheBytes, bool fWipe, int height,
                       const std::string& dataDir,
                       int nNormalizedNameForkHeight,
                       int64_t nOriginalClaimExpirationTime,
                       int64_t nExtendedClaimExpirationTime,
                       int64_t nExtendedClaimExpirationForkHeight,
                       int64_t nAllClaimsInMerkleForkHeight,
                       int proportionalDelayFactor) :
                       nNextHeight(height),
                       dbCacheBytes(cacheBytes),
                       dbFile(dataDir + "/claims.sqlite"), db(dbFile, sharedConfig),
                       nProportionalDelayFactor(proportionalDelayFactor),
                       nNormalizedNameForkHeight(nNormalizedNameForkHeight),
                       nOriginalClaimExpirationTime(nOriginalClaimExpirationTime),
                       nExtendedClaimExpirationTime(nExtendedClaimExpirationTime),
                       nExtendedClaimExpirationForkHeight(nExtendedClaimExpirationForkHeight),
                       nAllClaimsInMerkleForkHeight(nAllClaimsInMerkleForkHeight)
{
    applyPragmas(db, 5U * 1024U); // in KB

    db << "CREATE TABLE IF NOT EXISTS node (name BLOB NOT NULL PRIMARY KEY, "
          "parent BLOB REFERENCES node(name) DEFERRABLE INITIALLY DEFERRED, "
          "hash BLOB)";

    db << "CREATE INDEX IF NOT EXISTS node_hash_len_name ON node (hash, LENGTH(name) DESC)";
    // db << "CREATE UNIQUE INDEX IF NOT EXISTS node_parent_name ON node (parent, name)"; // no apparent gain
    db << "CREATE INDEX IF NOT EXISTS node_parent ON node (parent)";

    db << "CREATE TABLE IF NOT EXISTS takeover (name BLOB NOT NULL, height INTEGER NOT NULL, "
           "claimID BLOB, PRIMARY KEY(name, height DESC));";

    db << "CREATE INDEX IF NOT EXISTS takeover_height ON takeover (height)";

    db << "CREATE TABLE IF NOT EXISTS claim (claimID BLOB NOT NULL PRIMARY KEY, name BLOB NOT NULL, "
           "nodeName BLOB NOT NULL REFERENCES node(name) DEFERRABLE INITIALLY DEFERRED, "
           "txID BLOB NOT NULL, txN INTEGER NOT NULL, blockHeight INTEGER NOT NULL, "
           "validHeight INTEGER NOT NULL, activationHeight INTEGER NOT NULL, "
           "expirationHeight INTEGER NOT NULL, amount INTEGER NOT NULL);";

    db << "CREATE INDEX IF NOT EXISTS claim_activationHeight ON claim (activationHeight)";
    db << "CREATE INDEX IF NOT EXISTS claim_expirationHeight ON claim (expirationHeight)";
    db << "CREATE INDEX IF NOT EXISTS claim_nodeName ON claim (nodeName)";

    db << "CREATE TABLE IF NOT EXISTS support (txID BLOB NOT NULL, txN INTEGER NOT NULL, "
           "supportedClaimID BLOB NOT NULL, name BLOB NOT NULL, nodeName BLOB NOT NULL, "
           "blockHeight INTEGER NOT NULL, validHeight INTEGER NOT NULL, activationHeight INTEGER NOT NULL, "
           "expirationHeight INTEGER NOT NULL, amount INTEGER NOT NULL, PRIMARY KEY(txID, txN));";

    db << "CREATE INDEX IF NOT EXISTS support_supportedClaimID ON support (supportedClaimID)";
    db << "CREATE INDEX IF NOT EXISTS support_activationHeight ON support (activationHeight)";
    db << "CREATE INDEX IF NOT EXISTS support_expirationHeight ON support (expirationHeight)";
    db << "CREATE INDEX IF NOT EXISTS support_nodeName ON support (nodeName)";

    if (fWipe) {
        db << "DELETE FROM node";
        db << "DELETE FROM claim";
        db << "DELETE FROM support";
        db << "DELETE FROM takeover";
    }

    db << "INSERT OR IGNORE INTO node(name, hash) VALUES(x'', ?)" << one; // ensure that we always have our root node
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
    db << "SELECT COUNT(*) FROM (SELECT 1 FROM claim WHERE activationHeight < ?1 AND expirationHeight >= ?1 LIMIT 1)" << nNextHeight >> count;
    return count == 0;
}

bool CClaimTrieCacheBase::haveClaim(const std::string& name, const CTxOutPoint& outPoint) const
{
    auto query = db << "SELECT 1 FROM claim WHERE nodeName = ?1 AND txID = ?2 AND txN = ?3 "
                        "AND activationHeight < ?4 AND expirationHeight >= ?4 LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    return query.begin() != query.end();
}

bool CClaimTrieCacheBase::haveSupport(const std::string& name, const CTxOutPoint& outPoint) const
{
    auto query = db << "SELECT 1 FROM support WHERE nodeName = ?1 AND txID = ?2 AND txN = ?3 "
                        "AND activationHeight < ?4 AND expirationHeight >= ?4 LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    return query.begin() != query.end();
}

supportEntryType CClaimTrieCacheBase::getSupportsForName(const std::string& name) const
{
    // includes values that are not yet valid
    auto query = db << "SELECT supportedClaimID, txID, txN, blockHeight, activationHeight, amount "
                        "FROM support WHERE nodeName = ? AND expirationHeight >= ?" << name << nNextHeight;
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
    auto query = db << "SELECT activationHeight FROM claim WHERE nodeName = ? AND txID = ? AND txN = ? "
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
    auto query = db << "SELECT activationHeight FROM support WHERE nodeName = ? AND txID = ? AND txN = ? "
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
    db  << "SELECT COUNT(*) FROM (SELECT 1 FROM claim WHERE nodeName = ?1 AND activationHeight < ?2 AND expirationHeight >= ?2 LIMIT 1)"
        << name << nNextHeight >> claims;
    if (claims > 0) return false; // still has claims
    // we now know it has no claims, but we need to check its children
    int64_t count;
    std::string childName;
    // this line assumes that we've set the parent on child nodes already,
    // which means we are len(name) desc in our parent method
    db << "SELECT COUNT(*), MAX(name) FROM node WHERE parent = ?" << name >> std::tie(count, childName);
    if (count > 1) return false; // still has multiple children
    logPrint << "Removing node " << name << " with " << count << " children" << Clog::endl;
    // okay. it's going away
    auto query = db << "SELECT parent FROM node WHERE name = ?" << name;
    auto it = query.begin();
    if (it == query.end())
        return true; // we'll assume that whoever deleted this node previously cleaned things up correctly
    *it >> parent;
    db << "DELETE FROM node WHERE name = ?" << name;
    auto ret = db.rows_modified() > 0;
    if (ret && count == 1) // make the child skip us and point to its grandparent:
        db << "UPDATE node SET parent = ? WHERE name = ?" << parent << childName;
    if (ret)
        db << "UPDATE node SET hash = NULL WHERE name = ?" << parent;
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
    db  << "SELECT name FROM node WHERE hash IS NULL"
        >> [&names](std::string name) {
            names.push_back(std::move(name));
        };
    if (names.empty()) return; // nothing to do
    std::sort(names.begin(), names.end()); // guessing this is faster than "ORDER BY name"

    // there's an assumption that all nodes with claims are here; we do that as claims are inserted

    // assume parents are not set correctly here:
    auto parentQuery = db << "SELECT MAX(name) FROM node WHERE "
                              "name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
                              "SELECT POPS(p) FROM prefix WHERE p != x'') SELECT p FROM prefix)";

    auto insertQuery = db << "INSERT INTO node(name, parent, hash) VALUES(?, ?, NULL) "
                             "ON CONFLICT(name) DO UPDATE SET parent = excluded.parent, hash = NULL";

    auto nodeQuery = db << "SELECT name FROM node WHERE parent = ?";
    auto updateQuery = db << "UPDATE node SET parent = ? WHERE name = ?";

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
        for (auto&& row : nodeQuery << parent) {
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
        nodeQuery++;

        logPrint << "Inserting or updating node " << name << ", parent " << parent << Clog::endl;
        insertQuery << name << parent;
        insertQuery++;
    }

    nodeQuery.used(true);
    updateQuery.used(true);
    parentQuery.used(true);
    insertQuery.used(true);

    // now we need to percolate the nulls up the tree
    // parents should all be set right
    db << "UPDATE node SET hash = NULL WHERE name IN (WITH RECURSIVE prefix(p) AS "
          "(SELECT parent FROM node WHERE hash IS NULL UNION SELECT parent FROM prefix, node "
          "WHERE name = prefix.p AND prefix.p != x'') SELECT p FROM prefix)";
}

std::size_t CClaimTrieCacheBase::getTotalNamesInTrie() const
{
    // you could do this select from the node table, but you would have to ensure it is not dirty first
    std::size_t ret;
    db << "SELECT COUNT(DISTINCT nodeName) FROM claim WHERE activationHeight < ?1 AND expirationHeight >= ?1"
        << nNextHeight >> ret;
    return ret;
}

std::size_t CClaimTrieCacheBase::getTotalClaimsInTrie() const
{
    std::size_t ret;
    db << "SELECT COUNT(*) FROM claim WHERE activationHeight < ?1 AND expirationHeight >= ?1"
        << nNextHeight >> ret;
    return ret;
}

int64_t CClaimTrieCacheBase::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    int64_t ret = 0;
    const std::string query = fControllingOnly ?
        "SELECT SUM(amount) FROM (SELECT c.amount as amount, "
        "(SELECT(SELECT IFNULL(SUM(s.amount),0)+c.amount FROM support s "
        "WHERE s.supportedClaimID = c.claimID AND c.nodeName = s.nodeName "
        "AND s.activationHeight < ?1 AND s.expirationHeight >= ?1) as effective "
        "ORDER BY effective DESC LIMIT 1) as winner FROM claim c "
        "WHERE c.activationHeight < ?1 AND c.expirationHeight >= ?1 GROUP BY c.nodeName)"
    :
        "SELECT SUM(amount) FROM (SELECT c.amount as amount "
        "FROM claim c WHERE c.activationHeight < ?1 AND c.expirationHeight >= ?1)";

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
                        "FROM claim WHERE nodeName = ? AND expirationHeight >= ?"
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
                        "FROM takeover t WHERE t.name = n.name ORDER BY t.height DESC LIMIT 1), 0) FROM node n";
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
        auto code = sqlite::commit(db);
        if (code != SQLITE_OK) {
            logPrint << "ERROR in CClaimTrieCacheBase::" << __func__ << "(): SQLite code: " << code << Clog::endl;
            return false;
        }
        transacting = false;
    }
    base->nNextHeight = nNextHeight;
    removalWorkaround.clear();
    return true;
}

const std::string childHashQuery_s = "SELECT name, hash FROM node WHERE parent = ? ORDER BY name";

const std::string claimHashQuery_s =
    "SELECT c.txID, c.txN, c.claimID, c.blockHeight, c.activationHeight, c.amount, "
    "(SELECT IFNULL(SUM(s.amount),0)+c.amount FROM support s "
    "WHERE s.supportedClaimID = c.claimID AND s.nodeName = c.nodeName "
    "AND s.activationHeight < ?1 AND s.expirationHeight >= ?1) as effectiveAmount "
    "FROM claim c WHERE c.nodeName = ?2 AND c.activationHeight < ?1 AND c.expirationHeight >= ?1 "
    "ORDER BY effectiveAmount DESC, c.blockHeight, c.txID, c.txN";

const std::string claimHashQueryLimit_s = claimHashQuery_s + " LIMIT 1";

extern const std::string proofClaimQuery_s =
    "SELECT n.name, IFNULL((SELECT CASE WHEN t.claimID IS NULL THEN 0 ELSE t.height END "
    "FROM takeover t WHERE t.name = n.name ORDER BY t.height DESC LIMIT 1), 0) FROM node n "
    "WHERE n.name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
    "SELECT POPS(p) FROM prefix WHERE p != x'') SELECT p FROM prefix) "
    "ORDER BY n.name";

CClaimTrieCacheBase::CClaimTrieCacheBase(CClaimTrie* base)
    : base(base), db(base->dbFile, sharedConfig), transacting(false),
      childHashQuery(db << childHashQuery_s),
      claimHashQuery(db << claimHashQuery_s),
      claimHashQueryLimit(db << claimHashQueryLimit_s)
{
    assert(base);
    nNextHeight = base->nNextHeight;

    applyPragmas(db, base->dbCacheBytes >> 10U); // in KB
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
    db  << "SELECT hash FROM node WHERE name = x''"
        >> [&hash](std::unique_ptr<CUint256> rootHash) {
            if (rootHash)
                hash = std::move(*rootHash);
        };
    if (!hash.IsNull())
        return hash;
    assert(transacting); // no data changed but we didn't have the root hash there already?
    auto updateQuery = db << "UPDATE node SET hash = ? WHERE name = ?";
    db << "SELECT n.name, IFNULL((SELECT CASE WHEN t.claimID IS NULL THEN 0 ELSE t.height END FROM takeover t WHERE t.name = n.name "
            "ORDER BY t.height DESC LIMIT 1), 0) FROM node n WHERE n.hash IS NULL ORDER BY LENGTH(n.name) DESC" // assumes n.name is blob
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
                       "WHERE t.name = ?1 ORDER BY t.height DESC LIMIT 1" << name;
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
        int64_t nAmount, int nHeight, int nValidHeight)
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

    db << "INSERT INTO claim(claimID, name, nodeName, txID, txN, amount, blockHeight, validHeight, activationHeight, expirationHeight) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        << claimId << name << nodeName << outPoint.hash << outPoint.n << nAmount << nHeight << nValidHeight << nValidHeight << expires;

    if (nValidHeight < nNextHeight)
        db << "INSERT INTO node(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << nodeName;

    return true;
}

bool CClaimTrieCacheBase::addSupport(const std::string& name, const CTxOutPoint& outPoint, const CUint160& supportedClaimId,
        int64_t nAmount, int nHeight, int nValidHeight)
{
    ensureTransacting();

    if (nValidHeight < 0)
        nValidHeight = nHeight + getDelayForName(name, supportedClaimId);

    auto nodeName = adjustNameForValidHeight(name, nValidHeight);
    auto expires = expirationTime() + nHeight;

    db << "INSERT INTO support(supportedClaimID, name, nodeName, txID, txN, amount, blockHeight, validHeight, activationHeight, expirationHeight) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        << supportedClaimId << name << nodeName << outPoint.hash << outPoint.n << nAmount << nHeight << nValidHeight << nValidHeight << expires;

    if (nValidHeight < nNextHeight)
        db << "UPDATE node SET hash = NULL WHERE name = ?" << nodeName;

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

    auto query = db << "SELECT nodeName, activationHeight FROM claim WHERE claimID = ? AND txID = ? AND txN = ? AND expirationHeight >= ?"
                    << claimId << outPoint.hash << outPoint.n << nNextHeight;
    auto it = query.begin();
    if (it == query.end()) return false;
    *it >> nodeName >> validHeight;
    db  << "DELETE FROM claim WHERE claimID = ? AND txID = ? and txN = ?" << claimId << outPoint.hash << outPoint.n;
    if (!db.rows_modified())
        return false;
    db << "UPDATE node SET hash = NULL WHERE name = ?" << nodeName;

    // when node should be deleted from cache but instead it's kept
    // because it's a parent one and should not be effectively erased
    // we had a bug in the old code where that situation would force a zero delay on re-add
    if (nNextHeight >= 297706) { // TODO: hard fork this out (which we already tried once but failed)
        // we have to jump through some hoops here to make the claim_nodeName index be used on partial blobs
        // neither LIKE nor SUBSTR will use an index on a blob (which is lame because it would be simple)
        auto end = nodeName;
        auto usingRange = false;
        if (!end.empty() && end.back() < std::numeric_limits<char>::max()) {
            ++end.back(); // the fast path
            usingRange = true;
        }
        // else
        //    end += std::string(256U, std::numeric_limits<char>::max()); // 256 is our supposed max length claim, but that's not enforced anywhere presently
        auto query = usingRange ?
                     (db << "SELECT nodeName FROM claim WHERE nodeName BETWEEN ?1 AND ?2 "
                            "AND nodeName != ?2 AND activationHeight < ?3 AND expirationHeight > ?3 ORDER BY nodeName LIMIT 1"
                         << nodeName << end << nNextHeight) :
                    (db << "SELECT nodeName FROM claim INDEXED BY claim_nodeName WHERE SUBSTR(nodeName, 1, ?3) = ?1 "
                           "AND activationHeight < ?2 AND expirationHeight > ?2 ORDER BY nodeName LIMIT 1"
                            << nodeName << nNextHeight << nodeName.size());
        for (auto&& row: query) {
            std::string shortestMatch;
            row >> shortestMatch;
            if (shortestMatch != nodeName)
                // set this when there are no more claims on that name and that node still has children
                removalWorkaround.insert(nodeName);
        }
    }
    return true;
}

bool CClaimTrieCacheBase::removeSupport(const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight)
{
    ensureTransacting();

    auto query = db << "SELECT nodeName, activationHeight FROM support WHERE txID = ? AND txN = ? AND expirationHeight >= ?"
                    << outPoint.hash << outPoint.n << nNextHeight;
    auto it = query.begin();
    if (it == query.end()) return false;
    *it >> nodeName >> validHeight;
    db << "DELETE FROM support WHERE txID = ? AND txN = ?" << outPoint.hash << outPoint.n;
    if (!db.rows_modified())
        return false;
    db << "UPDATE node SET hash = NULL WHERE name = ?" << nodeName;
    return true;
}

// hardcoded claims that should trigger a takeover
#include <takeoverworkarounds.h>

bool CClaimTrieCacheBase::incrementBlock()
{
    // the plan:
    // for every claim and support that becomes active this block set its node hash to null (aka, dirty)
    // for every claim and support that expires this block set its node hash to null and add it to the expire(Support)Undo
    // for all dirty nodes look for new takeovers
    ensureTransacting();

    db << "INSERT INTO node(name) SELECT nodeName FROM claim INDEXED BY claim_activationHeight "
          "WHERE activationHeight = ?1 AND expirationHeight > ?1 "
          "ON CONFLICT(name) DO UPDATE SET hash = NULL"
          << nNextHeight;

    // don't make new nodes for items in supports or items that expire this block that don't exist in claims
    db << "UPDATE node SET hash = NULL WHERE name IN "
          "(SELECT nodeName FROM claim WHERE expirationHeight = ?1 "
          "UNION SELECT nodeName FROM support WHERE expirationHeight = ?1 OR activationHeight = ?1)"
          << nNextHeight;

    auto insertTakeoverQuery = db << "INSERT INTO takeover(name, height, claimID) VALUES(?, ?, ?)";

    // takeover handling:
    db  << "SELECT name FROM node WHERE hash IS NULL"
        >> [this, &insertTakeoverQuery](const std::string& nameWithTakeover) {
        // if somebody activates on this block and they are the new best, then everybody activates on this block
        CClaimValue candidateValue;
        auto hasCandidate = getInfoForName(nameWithTakeover, candidateValue, 1);
        // now that they're all in get the winner:
        CUint160 existingID;
        int existingHeight = 0;
        auto hasCurrentWinner = getLastTakeoverForName(nameWithTakeover, existingID, existingHeight);
        // we have a takeover if we had a winner and its changing or we never had a winner
        auto takeoverHappening = !hasCandidate || !hasCurrentWinner || existingID != candidateValue.claimId;

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
    db << "UPDATE claim SET activationHeight = ?1 WHERE nodeName = ?2 AND activationHeight > ?1 AND expirationHeight > ?1" << nNextHeight << name;
    ret |= db.rows_modified() > 0;

    // then do the same for supports:
    db << "UPDATE support SET activationHeight = ?1 WHERE nodeName = ?2 AND activationHeight > ?1 AND expirationHeight > ?1" << nNextHeight << name;
    ret |= db.rows_modified() > 0;
    return ret;
}

bool CClaimTrieCacheBase::decrementBlock()
{
    ensureTransacting();

    nNextHeight--;

    db << "INSERT INTO node(name) SELECT nodeName FROM claim "
          "WHERE expirationHeight = ? ON CONFLICT(name) DO UPDATE SET hash = NULL"
          << nNextHeight;

    db << "UPDATE node SET hash = NULL WHERE name IN("
          "SELECT nodeName FROM support WHERE expirationHeight = ?1 OR activationHeight = ?1 "
          "UNION SELECT nodeName FROM claim WHERE activationHeight = ?1)"
          << nNextHeight;

    db << "UPDATE claim SET activationHeight = validHeight WHERE activationHeight = ?"
          << nNextHeight;

    db << "UPDATE support SET activationHeight = validHeight WHERE activationHeight = ?"
          << nNextHeight;

    return true;
}

bool CClaimTrieCacheBase::finalizeDecrement()
{
    db << "UPDATE node SET hash = NULL WHERE name IN "
          "(SELECT nodeName FROM claim WHERE activationHeight = ?1 AND expirationHeight > ?1 "
          "UNION SELECT nodeName FROM support WHERE activationHeight = ?1 AND expirationHeight > ?1 "
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
                        "FROM claim WHERE SUBSTR(claimID, ?1) = ?2 AND activationHeight < ?3 AND expirationHeight >= ?3"
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
    db  << "SELECT DISTINCT nodeName FROM claim WHERE activationHeight < ?1 AND expirationHeight >= ?1"
        << nNextHeight >> [&callback](const std::string& name) {
            callback(name);
        };
}

std::vector<CUint160> CClaimTrieCacheBase::getActivatedClaims(int height) {
    std::vector<CUint160> ret;
    auto query = db << "SELECT DISTINCT claimID FROM claim WHERE activationHeight = ?1 AND blockHeight < ?1" << height;
    for (auto&& row: query) {
        ret.emplace_back();
        row >> ret.back();
    }
    return ret;
}
std::vector<CUint160> CClaimTrieCacheBase::getClaimsWithActivatedSupports(int height) {
    std::vector<CUint160> ret;
    auto query = db << "SELECT DISTINCT supportedClaimID FROM support WHERE activationHeight = ?1 AND blockHeight < ?1" << height;
    for (auto&& row: query) {
        ret.emplace_back();
        row >> ret.back();
    }
    return ret;
}
std::vector<CUint160> CClaimTrieCacheBase::getExpiredClaims(int height) {
    std::vector<CUint160> ret;
    auto query = db << "SELECT DISTINCT claimID FROM claim WHERE expirationHeight = ?1 AND blockHeight < ?1" << height;
    for (auto&& row: query) {
        ret.emplace_back();
        row >> ret.back();
    }
    return ret;
}
std::vector<CUint160> CClaimTrieCacheBase::getClaimsWithExpiredSupports(int height) {
    std::vector<CUint160> ret;
    auto query = db << "SELECT DISTINCT supportedClaimID FROM support WHERE expirationHeight = ?1 AND blockHeight < ?1" << height;
    for (auto&& row: query) {
        ret.emplace_back();
        row >> ret.back();
    }
    return ret;
}