#include <claimtrie.h>
#include <hash.h>
#include <logging.h>
#include <util.h>

#include <algorithm>
#include <memory>
#include <consensus/merkle.h>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

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

static const sqlite::sqlite_config sharedConfig{
    sqlite::OpenFlags::READWRITE | sqlite::OpenFlags::CREATE, // TODO: test with this: | sqlite::OpenFlags::SHAREDCACHE,
    nullptr, sqlite::Encoding::UTF8
};

CClaimTrie::CClaimTrie(bool fWipe, int height, int proportionalDelayFactor)
    : dbPath((GetDataDir() / "claims.sqlite").string()), db(dbPath, sharedConfig),
    nNextHeight(height), nProportionalDelayFactor(proportionalDelayFactor)
{
    db.define("MERKLE_ROOT", [](std::vector<uint256>& hashes, const std::vector<unsigned char>& blob) { hashes.emplace_back(uint256(blob)); },
              [](const std::vector<uint256>& hashes) { return ComputeMerkleRoot(hashes); });

    db.define("MERKLE_PAIR", [](const std::vector<unsigned char>& blob1, const std::vector<unsigned char>& blob2) { return Hash(blob1.begin(), blob1.end(), blob2.begin(), blob2.end()); });
    db.define("MERKLE", [](const std::vector<unsigned char>& blob1) { return Hash(blob1.begin(), blob1.end()); });

    db << "CREATE TABLE IF NOT EXISTS nodes (name TEXT NOT NULL PRIMARY KEY, parent TEXT, hash BLOB, takeoverHeight INTEGER, takeoverID BLOB)";
    db << "CREATE INDEX IF NOT EXISTS nodes_hash ON nodes (hash)";
    db << "CREATE INDEX IF NOT EXISTS nodes_parent ON nodes (parent)";

    db << "CREATE TABLE IF NOT EXISTS claims (claimID BLOB NOT NULL PRIMARY KEY, name TEXT NOT NULL, "
           "nodeName TEXT NOT NULL REFERENCES nodes(name) DEFERRABLE INITIALLY DEFERRED, "
           "txID BLOB NOT NULL, txN INTEGER NOT NULL, blockHeight INTEGER NOT NULL, "
           "validHeight INTEGER NOT NULL, expirationHeight INTEGER NOT NULL, "
           "amount INTEGER NOT NULL, metadata BLOB);";
    db << "CREATE INDEX IF NOT EXISTS claims_validHeight ON claims (validHeight)";
    db << "CREATE INDEX IF NOT EXISTS claims_expirationHeight ON claims (expirationHeight)";
    db << "CREATE INDEX IF NOT EXISTS claims_nodeName ON claims (nodeName)";

    db << "CREATE TABLE IF NOT EXISTS supports (txID BLOB NOT NULL, txN INTEGER NOT NULL, "
           "supportedClaimID BLOB NOT NULL, name TEXT NOT NULL, nodeName TEXT NOT NULL, "
           "blockHeight INTEGER NOT NULL, validHeight INTEGER NOT NULL, expirationHeight INTEGER NOT NULL, "
           "amount INTEGER NOT NULL, metadata BLOB, PRIMARY KEY(txID, txN));";
    db << "CREATE INDEX IF NOT EXISTS supports_supportedClaimID ON supports (supportedClaimID)";
    db << "CREATE INDEX IF NOT EXISTS supports_validHeight ON supports (validHeight)";
    db << "CREATE INDEX IF NOT EXISTS supports_expirationHeight ON supports (expirationHeight)";
    db << "CREATE INDEX IF NOT EXISTS supports_nodeName ON supports (nodeName)";

    db << "PRAGMA cache_size=-" + std::to_string(5 * 1024); // in -KB
    db << "PRAGMA synchronous=NORMAL"; // don't disk sync after transaction commit
    db << "PRAGMA journal_mode=MEMORY";
    db << "PRAGMA temp_store=MEMORY";
    db << "PRAGMA case_sensitive_like=true";

    if (fWipe) {
        db << "DELETE FROM nodes";
        db << "DELETE FROM claims";
        db << "DELETE FROM supports";
    }

    db << "INSERT OR IGNORE INTO nodes(name, hash) VALUES('', ?)" << one; // ensure that we always have our root node
}

CClaimTrieCacheBase::~CClaimTrieCacheBase() {
    if (transacting) {
        db << "rollback";
        transacting = false;
    }
}

bool CClaimTrie::SyncToDisk()
{
    // alternatively, switch to full sync after we are caught up on the chain
    auto rc = sqlite3_wal_checkpoint_v2(db.connection().get(), nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
    return rc == SQLITE_OK;
}

bool CClaimTrie::empty() {
    int64_t count;
    db << "SELECT COUNT(*) FROM claims WHERE validHeight < ? AND expirationHeight >= ?" << nNextHeight << nNextHeight >> count;
    return count == 0;
}

bool CClaimTrieCacheBase::haveClaim(const std::string& name, const COutPoint& outPoint) const
{
    auto query = db << "SELECT 1 FROM claims WHERE nodeName = ? AND txID = ? AND txN = ? "
                              "AND validHeight < ? AND expirationHeight >= ? LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight << nNextHeight;
    return query.begin() != query.end();
}

bool CClaimTrieCacheBase::haveSupport(const std::string& name, const COutPoint& outPoint) const
{
    auto query = db << "SELECT 1 FROM supports WHERE nodeName = ? AND txID = ? AND txN = ? "
                              "AND validHeight < ? AND expirationHeight >= ? LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight << nNextHeight;
    return query.begin() != query.end();
}

supportEntryType CClaimTrieCacheBase::getSupportsForName(const std::string& name) const
{
    // includes values that are not yet valid
    auto query = db << "SELECT supportedClaimID, txID, txN, blockHeight, validHeight, amount "
                              "FROM supports WHERE nodeName = ? AND expirationHeight >= ?" << name << nNextHeight;
    supportEntryType ret;
    for (auto&& row: query) {
        CSupportValue value;
        row >> value.supportedClaimId >> value.outPoint.hash >> value.outPoint.n
            >> value.nHeight >> value.nValidAtHeight >> value.nAmount;
        ret.push_back(value);
    }
    return ret;
}

bool CClaimTrieCacheBase::haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    auto query = db << "SELECT validHeight FROM claims WHERE nodeName = ? AND txID = ? AND txN = ? "
                              "AND validHeight >= ? AND expirationHeight > validHeight LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    for (auto&& row: query) {
        row >> nValidAtHeight;
        return true;
    }
    return false;
}

bool CClaimTrieCacheBase::haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    auto query = db << "SELECT validHeight FROM supports WHERE nodeName = ? AND txID = ? AND txN = ? "
                              "AND validHeight >= ? AND expirationHeight > validHeight LIMIT 1"
                    << name << outPoint.hash << outPoint.n << nNextHeight;
    for (auto&& row: query) {
        row >> nValidAtHeight;
        return true;
    }
    return false;
}

template<typename Target, typename... AttrTypes>
struct vector_builder : public std::vector<Target> {
    using std::vector<Target>::vector; // use constructors

    void operator()(AttrTypes... args) {
        this->emplace_back(std::forward<AttrTypes&&>(args)...);
    };
};

bool CClaimTrieCacheBase::deleteNodeIfPossible(const std::string& name, std::string& parent, std::vector<std::string>& claims) {
    if (name.empty()) return false;
    // to remove a node it must have one or less children and no claims
    vector_builder<std::string, std::string> claimsBuilder;
    db << "SELECT name FROM claims WHERE nodeName = ? AND validHeight < ? AND expirationHeight >= ? "
              << name << nNextHeight << nNextHeight >> claimsBuilder;
    claims = std::move(claimsBuilder);
    if (!claims.empty()) return false; // still has claims
    // we now know it has no claims, but we need to check its children
    int64_t count;
    std::string childName;
    // this line assumes that we've set the parent on child nodes already,
    // which means we are len(name) desc in our parent method
    // alternately: SELECT COUNT(DISTINCT nodeName) FROM claims WHERE SUBSTR(nodeName, 1, len(?)) == ? AND LENGTH(nodeName) > len(?)
    db << "SELECT COUNT(*),MAX(name) FROM nodes WHERE parent = ?" << name >> std::tie(count, childName);
    if (count > 1) return false; // still has multiple children
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

void CClaimTrieCacheBase::ensureTreeStructureIsUpToDate() {
    if (!transacting) return;

    // your children are your nodes that match your key but go at least one longer,
    // and have no trailing prefix in common with the other nodes in that set -- a hard query w/o parent field

    // when we get into this method, we have some claims that have been added, removed, and updated
    // those each have a corresponding node in the list with a null hash
    // some of our nodes will go away, some new ones will be added, some will be reparented


    // the plan: update all the claim hashes first
    vector_builder<std::string, std::string> names;
    db << "SELECT name FROM nodes WHERE hash IS NULL ORDER BY LENGTH(name) DESC, name DESC" >> names;
    if (names.empty()) return; // nothing to do

    // there's an assumption that all nodes with claims are here; we do that as claims are inserted

    // assume parents are not set correctly here:
    auto parentQuery = db << "SELECT name FROM nodes WHERE "
                              "name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
                              "SELECT SUBSTR(p, 1, LENGTH(p)-1) FROM prefix WHERE p != '') SELECT p FROM prefix) "
                              "ORDER BY LENGTH(name) DESC LIMIT 1";

    for (auto& name: names) {
        std::vector<std::string> claims;
        std::string parent;
        if (deleteNodeIfPossible(name, parent, claims)) {
            std::string grandparent;
            deleteNodeIfPossible(parent, grandparent, claims);
            continue;
        }
        if (name.empty() || claims.empty())
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
        vector_builder<std::string, std::string> siblings;
        db << "SELECT name FROM nodes WHERE parent = ?" << parent >> siblings;
        std::size_t splitPos = 0;
        for (auto& sibling: siblings) {
            if (sibling[parent.size()] == name[parent.size()]) {
                splitPos = parent.size() + 1;
                while(splitPos < sibling.size() && splitPos < name.size() && sibling[splitPos] == name[splitPos])
                    ++splitPos;
                auto newNodeName = name.substr(0, splitPos);
                // notify new node's parent:
                db << "UPDATE nodes SET hash = NULL WHERE name = ?" << parent;
                // and his sibling:
                db << "UPDATE nodes SET parent = ? WHERE name = ?" << newNodeName << sibling;
                if (splitPos == name.size()) {
                    // our new node is the same as the one we wanted to insert
                    break;
                }
                // insert the split node:
                db << "INSERT INTO nodes(name, parent, hash) VALUES(?, ?, NULL) "
                       "ON CONFLICT(name) DO UPDATE SET parent = excluded.parent, hash = NULL"
                    << newNodeName << parent;
                parent = std::move(newNodeName);
                break;
            }
        }

        db << "INSERT INTO nodes(name, parent, hash) VALUES(?, ?, NULL) "
               "ON CONFLICT(name) DO UPDATE SET parent = excluded.parent, hash = NULL"
            << name << parent;
        if (splitPos == 0)
            db << "UPDATE nodes SET hash = NULL WHERE name = ?" << parent;
    }

    // now we need to percolate the nulls up the tree
    // parents should all be set right
    db << "UPDATE nodes SET hash = NULL WHERE name IN (WITH RECURSIVE prefix(p) AS "
          "(SELECT parent FROM nodes WHERE hash IS NULL UNION SELECT parent FROM prefix, nodes "
          "WHERE name = prefix.p AND prefix.p IS NOT NULL ORDER BY parent DESC) SELECT p FROM prefix)";
}

std::size_t CClaimTrieCacheBase::getTotalNamesInTrie() const
{
    // you could do this select from the nodes table, but you would have to ensure it is not dirty first
    std::size_t ret;
    db << "SELECT COUNT(DISTINCT nodeName) FROM claims WHERE validHeight < ? AND expirationHeight >= ?"
              << nNextHeight << nNextHeight >> ret;
    return ret;
}

std::size_t CClaimTrieCacheBase::getTotalClaimsInTrie() const
{
    std::size_t ret;
    db << "SELECT COUNT(*) FROM claims WHERE validHeight < ? AND expirationHeight >= ?"
              << nNextHeight << nNextHeight >> ret;
    return ret;
}

CAmount CClaimTrieCacheBase::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    CAmount ret = 0;
    std::string query("SELECT (SELECT TOTAL(s.amount)+c.amount FROM supports s "
                      "WHERE s.supportedClaimID = c.claimID AND s.validHeight < ? AND s.expirationHeight >= ?) "
                      "FROM claims c WHERE c.validHeight < ? AND s.expirationHeight >= ?");
    if (fControllingOnly)
        throw std::runtime_error("not implemented yet"); // TODO: finish this
    db << query << nNextHeight << nNextHeight << nNextHeight << nNextHeight >> ret;
    return ret;
}

bool CClaimTrieCacheBase::getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset) const
{
    auto nextHeight = nNextHeight + heightOffset;
    auto query = db << "SELECT c.claimID, c.txID, c.txN, c.blockHeight, c.validHeight, c.amount, "
                              "(SELECT TOTAL(s.amount)+c.amount FROM supports s WHERE s.supportedClaimID = c.claimID AND s.validHeight < ? AND s.expirationHeight >= ?) as effectiveAmount "
                              "FROM claims c WHERE c.nodeName = ? AND c.validHeight < ? AND c.expirationHeight >= ? "
                              "ORDER BY effectiveAmount DESC, c.blockHeight, c.txID, c.txN LIMIT 1"
                    << nextHeight << nextHeight << name << nextHeight << nextHeight;
    for (auto&& row: query) {
        row >> claim.claimId >> claim.outPoint.hash >> claim.outPoint.n
            >> claim.nHeight >> claim.nValidAtHeight >> claim.nAmount >> claim.nEffectiveAmount;
        return true;
    }
    return false;
}

CClaimSupportToName CClaimTrieCacheBase::getClaimsForName(const std::string& name) const
{
    int nLastTakeoverHeight = 0;
    db << "SELECT IFNULL(takeoverHeight,0) FROM nodes WHERE name = ?" << name >> nLastTakeoverHeight;

    auto supports = getSupportsForName(name);

    auto query = db << "SELECT claimID, txID, txN, blockHeight, validHeight, amount "
                              "FROM claims WHERE nodeName = ? AND expirationHeight >= ?"
                    << name << nNextHeight;
    claimEntryType claims;
    for (auto&& row: query) {
        CClaimValue claim;
        row >> claim.claimId >> claim.outPoint.hash >> claim.outPoint.n
            >> claim.nHeight >> claim.nValidAtHeight >> claim.nAmount;
        claims.push_back(claim);
    }

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
            ic->supports.emplace_back(*it);
        }
        ic->claim.nEffectiveAmount = ic->effectiveAmount;
    }
    std::sort(claimsNsupports.begin(), claimsNsupports.end());
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

uint256 CClaimTrieCacheBase::recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly)
{
    std::vector<uint8_t> vchToHash;
    const auto pos = name.size();
    auto query = db << "SELECT name, hash, IFNULL(takeoverHeight,0) FROM nodes WHERE parent = ? ORDER BY name" << name;
    for (auto&& row : query) {
        std::string key;
        int childTakeoverHeight;
        std::unique_ptr<uint256> hash;
        row >> key >> hash >> childTakeoverHeight;
        if (hash == nullptr) hash = std::make_unique<uint256>();
        if (hash->IsNull()) {
            *hash = recursiveComputeMerkleHash(key, childTakeoverHeight, checkOnly);
        }
        completeHash(*hash, key, pos);
        vchToHash.push_back(key[pos]);
        vchToHash.insert(vchToHash.end(), hash->begin(), hash->end());
    }

    CClaimValue claim;
    if (getInfoForName(name, claim)) {
        uint256 valueHash = getValueHash(claim.outPoint, takeoverHeight);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    auto computedHash = vchToHash.empty() ? one : Hash(vchToHash.begin(), vchToHash.end());
    if (!checkOnly)
        db << "UPDATE nodes SET hash = ? WHERE name = ?" << computedHash << name;
    return computedHash;
}

bool CClaimTrieCacheBase::checkConsistency()
{
    // verify that all claims hash to the values on the nodes

    auto query = db << "SELECT name, hash, IFNULL(takeoverHeight, 0) FROM nodes";
    for (auto&& row: query) {
        std::string name;
        uint256 hash;
        int takeoverHeight;
        row >> name >> hash >> takeoverHeight;
        auto computedHash = recursiveComputeMerkleHash(name, takeoverHeight, true);
        if (computedHash != hash)
            return false;
    }
    return true;
}

bool CClaimTrieCacheBase::flush()
{
    if (transacting) {
        getMerkleHash();
        try {
            db << "commit";
        }
        catch (const std::exception& e) {
            LogPrintf("ERROR in ClaimTrieCache flush: %s\n", e.what());
            return false;
        }
        transacting = false;
    }
    base->nNextHeight = nNextHeight;
    return true;
}

bool CClaimTrieCacheBase::ValidateTipMatches(const CBlockIndex* tip)
{
    base->nNextHeight = nNextHeight = tip ? tip->nHeight + 1 : 0;

    LogPrintf("Checking claim trie consistency... ");
    if (checkConsistency()) {
        LogPrintf("consistent\n");
        if (tip && tip->hashClaimTrie != getMerkleHash()) {
            // suppose we leave the old LevelDB data there: any harm done? It's ~10GB
            // eh, we better blow it away; it's their job to back up the folder first
            // well, only do it if we're empty on the sqlite side -- aka, we haven't trie to sync first
            std::size_t count;
            db << "SELECT COUNT(*) FROM nodes" >> count;
            if (!count) {
                auto oldDataPath = GetDataDir() / "claimtrie";
                boost::system::error_code ec;
                boost::filesystem::remove_all(oldDataPath, ec);
            }

            return error("%s(): the block's root claim hash doesn't match the persisted claim root hash.", __func__);
        }
        return true;
    }
    LogPrintf("inconsistent!\n");

    return false;
}

CClaimTrieCacheBase::CClaimTrieCacheBase(CClaimTrie* base)
    : base(base), db(base->dbPath, sharedConfig), transacting(false)
{
    assert(base);
    nNextHeight = base->nNextHeight;

    db << "PRAGMA cache_size=-" + std::to_string(200 * 1024); // in -KB
    db << "PRAGMA synchronous=NORMAL"; // don't disk sync after transaction commit
    db << "PRAGMA journal_mode=MEMORY";
    db << "PRAGMA temp_store=MEMORY";
    db << "PRAGMA case_sensitive_like=true";
}

int CClaimTrieCacheBase::expirationTime() const
{
    return Params().GetConsensus().nOriginalClaimExpirationTime;
}

uint256 CClaimTrieCacheBase::getMerkleHash()
{
    ensureTreeStructureIsUpToDate();
    std::unique_ptr<uint256> hash;
    int takeoverHeight;
    db << "SELECT hash, IFNULL(takeoverHeight, 0) FROM nodes WHERE name = ''" >> std::tie(hash, takeoverHeight);
    if (hash == nullptr || hash->IsNull()) {
        assert(transacting); // no data changed but we didn't have the root hash there already?
        return recursiveComputeMerkleHash("", takeoverHeight, false);
    }
    return *hash;
}

bool CClaimTrieCacheBase::getLastTakeoverForName(const std::string& name, uint160& claimId, int& takeoverHeight) const
{
    auto query = db << "SELECT takeoverHeight, takeoverID FROM nodes WHERE name = ? AND takeoverID IS NOT NULL" << name;
    auto it = query.begin();
    if (it == query.end())
        return false;
    *it >> takeoverHeight >> claimId;
    return !claimId.IsNull();
}

bool CClaimTrieCacheBase::addClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId,
        CAmount nAmount, int nHeight, int nValidHeight, const std::vector<unsigned char>& metadata)
{
    if (!transacting) { transacting = true; db << "begin"; }

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

    db << "INSERT INTO claims(claimID, name, nodeName, txID, txN, amount, blockHeight, validHeight, expirationHeight, metadata) "
          "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)" << claimId << name << nodeName
           << outPoint.hash << outPoint.n << nAmount << nHeight << nValidHeight << expires << metadata;
    db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << nodeName;

    return true;
}

bool CClaimTrieCacheBase::addSupport(const std::string& name, const COutPoint& outPoint, CAmount nAmount,
        const uint160& supportedClaimId, int nHeight, int nValidHeight, const std::vector<unsigned char>& metadata)
{
    if (!transacting) { transacting = true; db << "begin"; }

    if (nValidHeight < 0)
        nValidHeight = nHeight + getDelayForName(name, supportedClaimId);
    auto nodeName = adjustNameForValidHeight(name, nValidHeight);
    auto expires = expirationTime() + nHeight;
    db << "INSERT INTO supports(supportedClaimID, name, nodeName, txID, txN, amount, blockHeight, validHeight, expirationHeight, metadata) "
                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)" << supportedClaimId << name << nodeName
                 << outPoint.hash << outPoint.n << nAmount << nHeight << nValidHeight << expires << metadata;

    db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << nodeName;
    return true;
}

bool CClaimTrieCacheBase::removeClaim(const uint160& claimId, const COutPoint& outPoint, std::string& nodeName, int& validHeight)
{
    if (!transacting) { transacting = true; db << "begin"; }

    // this gets tricky in that we may be removing an update
    // when going forward we spend a claim (aka, call removeClaim) before updating it (aka, call addClaim)
    // when going backwards we first remove the update by calling removeClaim
    // we then undo the spend of the previous one by calling addClaim with the original data
    // in order to maintain the proper takeover height the udpater will need to use our height returned here

    auto query = db << "SELECT nodeName, validHeight FROM claims WHERE claimID = ? and txID = ? and txN = ?"
                    << claimId << outPoint.hash << outPoint.n;
    auto it = query.begin();
    if (it == query.end()) return false;
    *it >> nodeName >> validHeight;
    db << "DELETE FROM claims WHERE claimID = ? AND txID = ? and txN = ?" << claimId << outPoint.hash << outPoint.n;
    db << "UPDATE nodes SET hash = NULL WHERE name = ?" << nodeName;
    return true;
}

bool CClaimTrieCacheBase::removeSupport(const COutPoint& outPoint, std::string& nodeName, int& validHeight)
{
    if (!transacting) { transacting = true; db << "begin"; }

    auto query = db << "SELECT nodeName, validHeight FROM supports WHERE txID = ? AND txN = ?"
                    << outPoint.hash << outPoint.n;
    auto it = query.begin();
    if (it == query.end()) return false;
    *it >> nodeName >> validHeight;
    db << "DELETE FROM supports WHERE txID = ? AND txN = ?" << outPoint.hash << outPoint.n;
    db << "UPDATE nodes SET hash = NULL WHERE name = ?" << nodeName;
    return true;
}

static const boost::container::flat_map<std::pair<int, std::string>, int> takeoverWorkarounds = {
        {{ 496856, "HunterxHunterAMV" }, 496835},
        {{ 542978, "namethattune1" }, 542429},
        {{ 543508, "namethattune-5" }, 543306},
        {{ 546780, "forecasts" }, 546624},
        {{ 548730, "forecasts" }, 546780},
        {{ 551540, "forecasts" }, 548730},
        {{ 552380, "chicthinkingofyou" }, 550804},
        {{ 560363, "takephotowithlbryteam" }, 559962},
        {{ 563710, "test-img" }, 563700},
        {{ 566750, "itila" }, 543261},
        {{ 567082, "malabarismo-com-bolas-de-futebol-vs-chap" }, 563592},
        {{ 596860, "180mphpullsthrougheurope" }, 596757},
        {{ 617743, "vaccines" }, 572756},
        {{ 619609, "copface-slamshandcuffedteengirlintoconcrete" }, 539940},
        {{ 620392, "banker-exposes-satanic-elite" }, 597788},
        {{ 624997, "direttiva-sulle-armi-ue-in-svizzera-di" }, 567908},
        {{ 624997, "best-of-apex" }, 585580},
        {{ 629970, "cannot-ignore-my-veins" }, 629914},
        {{ 633058, "bio-waste-we-programmed-your-brain" }, 617185},
        {{ 633601, "macrolauncher-overview-first-look" }, 633058},
        {{ 640186, "its-up-to-you-and-i-2019" }, 639116},
        {{ 640241, "tor-eas-3-20" }, 592645},
        {{ 640522, "seadoxdark" }, 619531},
        {{ 640617, "lbry-przewodnik-1-instalacja" }, 451186},
        {{ 640623, "avxchange-2019-the-next-netflix-spotify" }, 606790},
        {{ 640684, "algebra-introduction" }, 624152},
        {{ 640684, "a-high-school-math-teacher-does-a" }, 600885},
        {{ 640684, "another-random-life-update" }, 600884},
        {{ 640684, "who-is-the-taylor-series-for" }, 600882},
        {{ 640684, "tedx-talk-released" }, 612303},
        {{ 640730, "e-mental" }, 615375},
        {{ 641143, "amiga-1200-bespoke-virgin-cinema" }, 623542},
        {{ 641161, "dreamscape-432-omega" }, 618894},
        {{ 641162, "2019-topstone-carbon-force-etap-axs-bike" }, 639107},
        {{ 641186, "arin-sings-big-floppy-penis-live-jazz-2" }, 638904},
        {{ 641421, "edward-snowden-on-bitcoin-and-privacy" }, 522729},
        {{ 641421, "what-is-libra-facebook-s-new" }, 598236},
        {{ 641421, "what-are-stablecoins-counter-party-risk" }, 583508},
        {{ 641421, "anthony-pomp-pompliano-discusses-crypto" }, 564416},
        {{ 641421, "tim-draper-crypto-invest-summit-2019" }, 550329},
        {{ 641421, "mass-adoption-and-what-will-it-take-to" }, 549781},
        {{ 641421, "dragonwolftech-youtube-channel-trailer" }, 567128},
        {{ 641421, "naomi-brockwell-s-weekly-crypto-recap" }, 540006},
        {{ 641421, "blockchain-based-youtube-twitter" }, 580809},
        {{ 641421, "andreas-antonopoulos-on-privacy-privacy" }, 533522},
        {{ 641817, "mexico-submits-and-big-tech-worsens" }, 582977},
        {{ 641817, "why-we-need-travel-bans" }, 581354},
        {{ 641880, "censored-by-patreon-bitchute-shares" }, 482460},
        {{ 641880, "crypto-wonderland" }, 485218},
        {{ 642168, "1-diabolo-julio-cezar-16-cbmcp-freestyle" }, 374999},
        {{ 642314, "tough-students" }, 615780},
        {{ 642697, "gamercauldronep2" }, 642153},
        {{ 643406, "the-most-fun-i-ve-had-in-a-long-time" }, 616506},
        {{ 643893, "spitshine69-and-uk-freedom-audits" }, 616876},
        {{ 644480, "my-mum-getting-attacked-a-duck" }, 567624},
        {{ 644486, "the-cryptocurrency-experiment" }, 569189},
        {{ 644486, "tag-you-re-it" }, 558316},
        {{ 644486, "orange-county-mineral-society-rock-and" }, 397138},
        {{ 644486, "sampling-with-the-gold-rush-nugget" }, 527960},
        {{ 644562, "september-15-21-a-new-way-of-doing" }, 634792},
        {{ 644562, "july-week-3-collective-frequency-general" }, 607942},
        {{ 644562, "september-8-14-growing-up-general" }, 630977},
        {{ 644562, "august-4-10-collective-frequency-general" }, 612307},
        {{ 644562, "august-11-17-collective-frequency" }, 617279},
        {{ 644562, "september-1-7-gentle-wake-up-call" }, 627104},
        {{ 644607, "no-more-lol" }, 643497},
        {{ 644607, "minion-masters-who-knew" }, 641313},
        {{ 645236, "danganronpa-3-the-end-of-hope-s-peak" }, 644153},
        {{ 645348, "captchabot-a-discord-bot-to-protect-your" }, 592810},
        {{ 645701, "the-xero-hour-saint-greta-of-thunberg" }, 644081},
        {{ 645701, "batman-v-superman-theological-notions" }, 590189},
        {{ 645918, "emacs-is-great-ep-0-init-el-from-org" }, 575666},
        {{ 645918, "emacs-is-great-ep-1-packages" }, 575666},
        {{ 645918, "emacs-is-great-ep-40-pt-2-hebrew" }, 575668},
        {{ 645923, "nasal-snuff-review-osp-batch-2" }, 575658},
        {{ 645923, "why-bit-coin" }, 575658},
        {{ 645929, "begin-quest" }, 598822},
        {{ 645929, "filthy-foe" }, 588386},
        {{ 645929, "unsanitary-snow" }, 588386},
        {{ 645929, "famispam-1-music-box" }, 588386},
        {{ 645929, "running-away" }, 598822},
        {{ 645931, "my-beloved-chris-madsen" }, 589114},
        {{ 645931, "space-is-consciousness-chris-madsen" }, 589116},
        {{ 645947, "gasifier-rocket-stove-secondary-burn" }, 590595},
        {{ 645949, "mouse-razer-abyssus-v2-e-mousepad" }, 591139},
        {{ 645949, "pr-temporada-2018-league-of-legends" }, 591138},
        {{ 645949, "windows-10-build-9901-pt-br" }, 591137},
        {{ 645949, "abrindo-pacotes-do-festival-lunar-2018" }, 591139},
        {{ 645949, "unboxing-camisetas-personalizadas-play-e" }, 591138},
        {{ 645949, "abrindo-envelopes-do-festival-lunar-2017" }, 591138},
        {{ 645951, "grub-my-grub-played-guruku-tersayang" }, 618033},
        {{ 645951, "ismeeltimepiece" }, 618038},
        {{ 645951, "thoughts-on-doom" }, 596485},
        {{ 645951, "thoughts-on-god-of-war-about-as-deep-as" }, 596485},
        {{ 645956, "linux-lite-3-6-see-what-s-new" }, 645195},
        {{ 646191, "kahlil-gibran-the-prophet-part-1" }, 597637},
        {{ 646551, "crypto-market-crash-should-you-sell-your" }, 442613},
        {{ 646551, "live-crypto-trading-and-market-analysis" }, 442615},
        {{ 646551, "5-reasons-trading-is-always-better-than" }, 500850},
        {{ 646551, "digitex-futures-dump-panic-selling-or" }, 568065},
        {{ 646552, "how-to-install-polarr-on-kali-linux-bynp" }, 466235},
        {{ 646586, "electoral-college-kids-civics-lesson" }, 430818},
        {{ 646602, "grapes-full-90-minute-watercolour" }, 537108},
        {{ 646602, "meizu-mx4-the-second-ubuntu-phone" }, 537109},
        {{ 646609, "how-to-set-up-the-ledger-nano-x" }, 569992},
        {{ 646609, "how-to-buy-ethereum" }, 482354},
        {{ 646609, "how-to-install-setup-the-exodus-multi" }, 482356},
        {{ 646609, "how-to-manage-your-passwords-using" }, 531987},
        {{ 646609, "cryptodad-s-live-q-a-friday-may-3rd-2019" }, 562303},
        {{ 646638, "resident-evil-ada-chapter-5-final" }, 605612},
        {{ 646639, "taurus-june-2019-career-love-tarot" }, 586910},
        {{ 646652, "digital-bullpen-ep-5-building-a-digital" }, 589274},
        {{ 646661, "sunlight" }, 591076},
        {{ 646661, "grasp-lab-nasa-open-mct-series" }, 589414},
        {{ 646663, "bunnula-s-creepers-tim-pool-s-beanie-a" }, 599669},
        {{ 646663, "bunnula-music-hey-ya-by-outkast" }, 605685},
        {{ 646663, "bunnula-tv-s-music-television-eunoia" }, 644437},
        {{ 646663, "the-pussy-centipede-40-sneakers-and" }, 587265},
        {{ 646663, "bunnula-reacts-ashton-titty-whitty" }, 596988},
        {{ 646677, "filip-reviews-jeromes-dream-cataracts-so" }, 589751},
        {{ 646691, "fascism-and-its-mobilizing-passions" }, 464342},
        {{ 646692, "hsb-color-layers-action-for-adobe" }, 586533},
        {{ 646692, "master-colorist-action-pack-extracting" }, 631830},
        {{ 646693, "how-to-protect-your-garden-from-animals" }, 588476},
        {{ 646693, "gardening-for-the-apocalypse-epic" }, 588472},
        {{ 646693, "my-first-bee-hive-foundationless-natural" }, 588469},
        {{ 646693, "dragon-fruit-and-passion-fruit-planting" }, 588470},
        {{ 646693, "installing-my-first-foundationless" }, 588469},
        {{ 646705, "first-naza-fpv" }, 590411},
        {{ 646717, "first-burning-man-2019-detour-034" }, 630247},
        {{ 646717, "why-bob-marley-was-an-idiot-test-driving" }, 477558},
        {{ 646717, "we-are-addicted-to-gambling-ufc-207-w" }, 481398},
        {{ 646717, "ghetto-swap-meet-selling-storage-lockers" }, 498291},
        {{ 646738, "1-kings-chapter-7-summary-and-what-god" }, 586599},
        {{ 646814, "brand-spanking-new-junior-high-school" }, 592378},
        {{ 646814, "lupe-fiasco-freestyle-at-end-of-the-weak" }, 639535},
        {{ 646824, "how-to-one-stroke-painting-doodles-mixed" }, 592404},
        {{ 646824, "acrylic-pouring-landscape-with-a-tree" }, 592404},
        {{ 646824, "how-to-make-a-diy-concrete-paste-planter" }, 595976},
        {{ 646824, "how-to-make-a-rustic-sand-planter-sand" }, 592404},
        {{ 646833, "3-day-festival-at-the-galilee-lake-and" }, 592842},
        {{ 646833, "rainbow-circle-around-the-noon-sun-above" }, 592842},
        {{ 646833, "energetic-self-control-demonstration" }, 623811},
        {{ 646833, "bees-congregating" }, 592842},
        {{ 646856, "formula-offroad-honefoss-sunday-track2" }, 592872},
        {{ 646862, "h3video1-dc-vs-mb-1" }, 593237},
        {{ 646862, "h3video1-iwasgoingto-load-up-gmod-but" }, 593237},
        {{ 646883, "watch-this-game-developer-make-a-video" }, 592593},
        {{ 646883, "how-to-write-secure-javascript" }, 592593},
        {{ 646883, "blockchain-technology-explained-2-hour" }, 592593},
        {{ 646888, "fl-studio-bits" }, 608155},
        {{ 646914, "andy-s-shed-live-s03e02-the-longest" }, 592200},
        {{ 646914, "gpo-telephone-776-phone-restoration" }, 592201},
        {{ 646916, "toxic-studios-co-stream-pubg" }, 597126},
        {{ 646916, "hyperlapse-of-prague-praha-from-inside" }, 597109},
        {{ 646933, "videobits-1" }, 597378},
        {{ 646933, "clouds-developing-daytime-8" }, 597378},
        {{ 646933, "slechtvalk-in-watertoren-bodegraven" }, 597378},
        {{ 646933, "timelapse-maansverduistering-16-juli" }, 605880},
        {{ 646933, "startrails-27" }, 597378},
        {{ 646933, "passing-clouds-daytime-3" }, 597378},
        {{ 646940, "nerdgasm-unboxing-massive-playing-cards" }, 597421},
        {{ 646946, "debunking-cops-volume-3-the-murder-of" }, 630570},
        {{ 646961, "kingsong-ks16x-electric-unicycle-250km" }, 636725},
        {{ 646968, "wild-mountain-goats-amazing-rock" }, 621940},
        {{ 646968, "no-shelter-backcountry-camping-in" }, 621940},
        {{ 646968, "can-i-live-in-this-through-winter-lets" }, 645750},
        {{ 646968, "why-i-wear-a-chest-rig-backcountry-or" }, 621940},
        {{ 646989, "marc-ivan-o-gorman-promo-producer-editor" }, 645656},
        {{ 647045, "@moraltis" }, 646367},
        {{ 647045, "moraltis-twitch-highlights-first-edit" }, 646368},
        {{ 647075, "the-3-massive-tinder-convo-mistakes" }, 629464},
        {{ 647075, "how-to-get-friend-zoned-via-text" }, 592298},
        {{ 647075, "don-t-do-this-on-tinder" }, 624591},
        {{ 647322, "world-of-tanks-7-kills" }, 609905},
        {{ 647322, "the-tier-6-auto-loading-swedish-meatball" }, 591338},
        {{ 647416, "hypnotic-soundscapes-garden-of-the" }, 596923},
        {{ 647416, "hypnotic-soundscapes-the-cauldron-sacred" }, 596928},
        {{ 647416, "schumann-resonance-to-theta-sweep" }, 596920},
        {{ 647416, "conversational-indirect-hypnosis-why" }, 596913},
        {{ 647493, "mimirs-brunnr" }, 590498},
        {{ 648143, "live-ita-completiamo-the-evil-within-2" }, 646568},
        {{ 648203, "why-we-love-people-that-hurt-us" }, 591128},
        {{ 648203, "i-didn-t-like-my-baby-and-considered" }, 591128},
        {{ 648220, "trade-talk-001-i-m-a-vlogger-now-fielder" }, 597303},
        {{ 648220, "vise-restoration-record-no-6-vise" }, 597303},
        {{ 648540, "amv-reign" }, 571863},
        {{ 648540, "amv-virus" }, 571863},
        {{ 648588, "audial-drift-(a-journey-into-sound)" }, 630217},
        {{ 648616, "quick-zbrush-tip-transpose-master-scale" }, 463205},
        {{ 648616, "how-to-create-3d-horns-maya-to-zbrush-2" }, 463205},
        {{ 648815, "arduino-based-cartridge-game-handheld" }, 593252},
        {{ 648815, "a-maze-update-3-new-game-modes-amazing" }, 593252},
        {{ 649209, "denmark-trip" }, 591428},
        {{ 649209, "stunning-4k-drone-footage" }, 591428},
        {{ 649215, "how-to-create-a-channel-and-publish-a" }, 414908},
        {{ 649215, "lbryclass-11-how-to-get-your-deposit" }, 632420},
        {{ 649543, "spring-break-madness-at-universal" }, 599698},
        {{ 649921, "navegador-brave-navegador-da-web-seguro" }, 649261},
        {{ 650191, "stream-intro" }, 591301},
        {{ 650946, "platelet-chan-fan-art" }, 584601},
        {{ 650946, "aqua-fanart" }, 584601},
        {{ 650946, "virginmedia-stores-password-in-plain" }, 619537},
        {{ 650946, "running-linux-on-android-teaser" }, 604441},
        {{ 650946, "hatsune-miku-ievan-polka" }, 600126},
        {{ 650946, "digital-security-and-privacy-2-and-a-new" }, 600135},
        {{ 650993, "my-editorial-comment-on-recent-youtube" }, 590305},
        {{ 650993, "drive-7-18-2018" }, 590305},
        {{ 651011, "old-world-put-on-realm-realms-gg" }, 591899},
        {{ 651011, "make-your-own-soundboard-with-autohotkey" }, 591899},
        {{ 651011, "ark-survival-https-discord-gg-ad26xa" }, 637680},
        {{ 651011, "minecraft-featuring-seus-8-just-came-4" }, 596488},
        {{ 651057, "found-footage-bikinis-at-the-beach-with" }, 593586},
        {{ 651057, "found-footage-sexy-mom-a-mink-stole" }, 593586},
        {{ 651067, "who-are-the-gentiles-gomer" }, 597094},
        {{ 651067, "take-back-the-kingdom-ep-2-450-million" }, 597094},
        {{ 651067, "mmxtac-implemented-footstep-sounds-and" }, 597094},
        {{ 651067, "dynasoul-s-blender-to-unreal-animated" }, 597094},
        {{ 651103, "calling-a-scammer-syntax-error" }, 612532},
        {{ 651103, "quick-highlight-of-my-day" }, 647651},
        {{ 651103, "calling-scammers-and-singing-christmas" }, 612531},
        {{ 651109, "@livingtzm" }, 637322},
        {{ 651109, "living-tzm-juuso-from-finland-september" }, 643412},
        {{ 651373, "se-voc-rir-ou-sorrir-reinicie-o-v-deo" }, 649302},
        {{ 651476, "what-is-pagan-online-polished-new-arpg" }, 592157},
        {{ 651476, "must-have-elder-scrolls-online-addons" }, 592156},
        {{ 651476, "who-should-play-albion-online" }, 592156},
        {{ 651730, "person-detection-with-keras-tensorflow" }, 621276},
        {{ 651730, "youtube-censorship-take-two" }, 587249},
        {{ 651730, "new-red-tail-shark-and-two-silver-sharks" }, 587251},
        {{ 651730, "around-auckland" }, 587250},
        {{ 651730, "humanism-in-islam" }, 587250},
        {{ 651730, "tigers-at-auckland-zoo" }, 587250},
        {{ 651730, "gravity-demonstration" }, 587250},
        {{ 651730, "copyright-question" }, 587249},
        {{ 651730, "uberg33k-the-ultimate-software-developer" }, 599522},
        {{ 651730, "chl-e-swarbrick-auckland-mayoral" }, 587250},
        {{ 651730, "code-reviews" }, 587249},
        {{ 651730, "raising-robots" }, 587251},
        {{ 651730, "teaching-python" }, 587250},
        {{ 651730, "kelly-tarlton-2016" }, 587250},
        {{ 652172, "where-is-everything" }, 589491},
        {{ 652172, "some-guy-and-his-camera" }, 617062},
        {{ 652172, "practical-information-pt-1" }, 589491},
        {{ 652172, "latent-vibrations" }, 589491},
        {{ 652172, "maldek-compilation" }, 589491},
        {{ 652444, "thank-you-etika-thank-you-desmond" }, 652121},
        {{ 652611, "plants-vs-zombies-gw2-20190827183609" }, 624339},
        {{ 652611, "wolfenstein-the-new-order-playthrough-6" }, 650299},
        {{ 652887, "a-codeigniter-cms-open-source-download" }, 652737},
        {{ 652966, "@pokesadventures" }, 632391},
        {{ 653009, "flat-earth-uk-convention-is-a-bust" }, 585786},
        {{ 653009, "flat-earth-reset-flat-earth-money-tree" }, 585786},
        {{ 653011, "veil-of-thorns-dispirit-brutal-leech-3" }, 652475},
        {{ 653069, "being-born-after-9-11" }, 632218},
        {{ 653069, "8-years-on-youtube-what-it-has-done-for" }, 637130},
        {{ 653069, "answering-questions-how-original" }, 521447},
        {{ 653069, "talking-about-my-first-comedy-stand-up" }, 583450},
        {{ 653069, "doing-push-ups-in-public" }, 650920},
        {{ 653069, "vlog-extra" }, 465997},
        {{ 653069, "crying-myself" }, 465997},
        {{ 653069, "xbox-rejection" }, 465992},
        {{ 653354, "msps-how-to-find-a-linux-job-where-no" }, 642537},
        {{ 653354, "windows-is-better-than-linux-vlog-it-and" }, 646306},
        {{ 653354, "luke-smith-is-wrong-about-everything" }, 507717},
        {{ 653354, "advice-for-those-starting-out-in-tech" }, 612452},
        {{ 653354, "treating-yourself-to-make-studying-more" }, 623561},
        {{ 653354, "lpi-linux-essential-dns-tools-vlog-what" }, 559464},
        {{ 653354, "is-learning-linux-worth-it-in-2019-vlog" }, 570886},
        {{ 653354, "huawei-linux-and-cellphones-in-2019-vlog" }, 578501},
        {{ 653354, "how-to-use-webmin-to-manage-linux" }, 511507},
        {{ 653354, "latency-concurrency-and-the-best-value" }, 596857},
        {{ 653354, "how-to-use-the-pomodoro-method-in-it" }, 506632},
        {{ 653354, "negotiating-compensation-vlog-it-and" }, 542317},
        {{ 653354, "procedural-goals-vs-outcome-goals-vlog" }, 626785},
        {{ 653354, "intro-to-raid-understanding-how-raid" }, 529341},
        {{ 653354, "smokeping" }, 574693},
        {{ 653354, "richard-stallman-should-not-be-fired" }, 634928},
        {{ 653354, "unusual-or-specialty-certifications-vlog" }, 620146},
        {{ 653354, "gratitude-and-small-projects-vlog-it" }, 564900},
        {{ 653354, "why-linux-on-the-smartphone-is-important" }, 649543},
        {{ 653354, "opportunity-costs-vlog-it-devops-career" }, 549708},
        {{ 653354, "double-giveaway-lpi-class-dates-and" }, 608129},
        {{ 653354, "linux-on-the-smartphone-in-2019-librem" }, 530426},
        {{ 653524, "celtic-folk-music-full-live-concert-mps" }, 589762},
};

bool CClaimTrieCacheBase::incrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo,
                                         insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo,
                                         takeoverUndoType& takeoverUndo)
{
    // the plan:
    // for every claim and support that becomes active this block set its node hash to null (aka, dirty)
    // for every claim and support that expires this block set its node hash to null and add it to the expire(Support)Undo
    // for all dirty nodes look for new takeovers
    if (!transacting) { transacting = true; db << "begin"; }

    db << "UPDATE nodes SET hash = NULL WHERE name IN "
                 "(SELECT nodeName FROM claims WHERE validHeight = ?)" << nNextHeight;
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
                 "(SELECT nodeName FROM supports WHERE validHeight = ?)" << nNextHeight;

    assert(expireUndo.empty());
    {
        auto becomingExpired = db << "SELECT txID, txN, nodeName, claimID, validHeight, blockHeight, amount "
                                            "FROM claims WHERE expirationHeight = ?" << nNextHeight;
        for (auto &&row: becomingExpired) {
            CClaimValue value;
            std::string name;
            row >> value.outPoint.hash >> value.outPoint.n >> name
                >> value.claimId >> value.nValidAtHeight >> value.nHeight >> value.nAmount;
            expireUndo.emplace_back(name, value);
        }
    }
    db << "UPDATE nodes SET hash = NULL WHERE name IN (SELECT nodeName FROM claims WHERE expirationHeight = ?)"
              << nNextHeight;

    assert(expireSupportUndo.empty());
    {
        auto becomingExpired = db << "SELECT txID, txN, nodeName, supportedClaimID, validHeight, blockHeight, amount "
                                            "FROM supports WHERE expirationHeight = ?" << nNextHeight;
        for (auto &&row: becomingExpired) {
            CSupportValue value;
            std::string name;
            row >> value.outPoint.hash >> value.outPoint.n >> name
                >> value.supportedClaimId >> value.nValidAtHeight >> value.nHeight >> value.nAmount;
            expireSupportUndo.emplace_back(name, value);
        }
    }
    db << "UPDATE nodes SET hash = NULL WHERE name IN (SELECT nodeName FROM supports WHERE expirationHeight = ?)"
              << nNextHeight;

    // takeover handling:
    vector_builder<std::string, std::string> takeovers;
    db << "SELECT name FROM nodes WHERE hash IS NULL" >> takeovers;

    for (const auto& nameWithTakeover : takeovers) {
        auto needsActivate = false;
        if (nNextHeight >= 496856 && nNextHeight <= 653524) {
            auto wit = takeoverWorkarounds.find(std::make_pair(nNextHeight, nameWithTakeover));
            needsActivate = wit != takeoverWorkarounds.end();
        }

        // if somebody activates on this block and they are the new best, then everybody activates on this block
        CClaimValue candidateValue;
        auto hasCandidate = getInfoForName(nameWithTakeover, candidateValue, 1);
        // now that they're all in get the winner:
        int existingHeight;
        std::unique_ptr<uint160> existingID;
        db << "SELECT IFNULL(takeoverHeight, 0), takeoverID FROM nodes WHERE name = ?"
              << nameWithTakeover >> std::tie(existingHeight, existingID);

        auto hasBeenSetBefore = existingID != nullptr && !existingID->IsNull();
        auto takeoverHappening = needsActivate || !hasCandidate || (hasBeenSetBefore && *existingID != candidateValue.claimId);
        if (takeoverHappening && activateAllFor(insertUndo, insertSupportUndo, nameWithTakeover))
            hasCandidate = getInfoForName(nameWithTakeover, candidateValue, 1);

        if (takeoverHappening || !hasBeenSetBefore) {
            takeoverUndo.emplace_back(nameWithTakeover, std::make_pair(existingHeight, hasBeenSetBefore ? *existingID : uint160()));
            if (hasCandidate)
                db << "UPDATE nodes SET takeoverHeight = ?, takeoverID = ? WHERE name = ?"
                      << nNextHeight << candidateValue.claimId << nameWithTakeover;
            else
                db << "UPDATE nodes SET takeoverHeight = NULL, takeoverID = NULL WHERE name = ?" << nameWithTakeover;
            assert(db.rows_modified());
        }
    }

    nNextHeight++;
    return true;
}

bool CClaimTrieCacheBase::activateAllFor(insertUndoType& insertUndo, insertUndoType& insertSupportUndo,
                                         const std::string& name) {
    // now that we know a takeover is happening, we bring everybody in:
    auto ret = false;
    {
        auto query = db << "SELECT txID, txN, validHeight FROM claims WHERE nodeName = ? AND validHeight > ? AND expirationHeight > ?"
                        << name << nNextHeight << nNextHeight;
        for (auto &&row: query) {
            uint256 hash;
            uint32_t n;
            int oldValidHeight;
            row >> hash >> n >> oldValidHeight;
            insertUndo.emplace_back(name, COutPoint(hash, n), oldValidHeight);
        }
    }
    // and then update them all to activate now:
    db << "UPDATE claims SET validHeight = ? WHERE nodeName = ? AND validHeight > ? AND expirationHeight > ?"
              << nNextHeight << name << nNextHeight << nNextHeight;
    ret |= db.rows_modified() > 0;

    // then do the same for supports:
    {
        auto query = db << "SELECT txID, txN, validHeight FROM supports WHERE nodeName = ? AND validHeight > ? AND expirationHeight > ?"
                        << name << nNextHeight << nNextHeight;
        for (auto &&row: query) {
            uint256 hash;
            uint32_t n;
            int oldValidHeight;
            row >> hash >> n >> oldValidHeight;
            insertSupportUndo.emplace_back(name, COutPoint(hash, n), oldValidHeight);
        }
    }
    // and then update them all to activate now:
    db << "UPDATE supports SET validHeight = ? WHERE nodeName = ? AND validHeight > ? AND expirationHeight > ?"
              << nNextHeight << name << nNextHeight << nNextHeight;
    ret |= db.rows_modified() > 0;
    return ret;
}

bool CClaimTrieCacheBase::decrementBlock(insertUndoType& insertUndo, claimUndoType& expireUndo,
                                         insertUndoType& insertSupportUndo, supportUndoType& expireSupportUndo)
{
    if (!transacting) { transacting = true; db << "begin"; }

    nNextHeight--;

    // to actually delete the expired items and then restore them here I would have to look up the metadata in the block
    // that doesn't sound very fun so we modified the other queries to exclude expired items
    for (auto it = expireSupportUndo.crbegin(); it != expireSupportUndo.crend(); ++it) {
        db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << it->first;
    }

    for (auto it = expireUndo.crbegin(); it != expireUndo.crend(); ++it) {
        db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << it->first;
    }

    for (auto it = insertSupportUndo.crbegin(); it != insertSupportUndo.crend(); ++it) {
        LogPrint(BCLog::CLAIMS, "Resetting support valid height to %d for %s\n", it->nValidHeight, it->name);
        db << "UPDATE supports SET validHeight = ? WHERE txID = ? AND txN = ?"
           << it->nValidHeight << it->outPoint.hash << it->outPoint.n;
        db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << it->name;
    }

    for (auto it = insertUndo.crbegin(); it != insertUndo.crend(); ++it) {
        LogPrint(BCLog::CLAIMS, "Resetting valid height to %d for %s\n", it->nValidHeight, it->name);
        db << "UPDATE claims SET validHeight = ? WHERE nodeName = ? AND txID = ? AND txN = ?"
           << it->nValidHeight << it->name << it->outPoint.hash << it->outPoint.n;
        db << "INSERT INTO nodes(name) VALUES(?) ON CONFLICT(name) DO UPDATE SET hash = NULL" << it->name;
    }

    return true;
}

bool CClaimTrieCacheBase::finalizeDecrement(takeoverUndoType& takeoverUndo)
{
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
          "(SELECT nodeName FROM claims WHERE validHeight = ?)" << nNextHeight;
    db << "UPDATE nodes SET hash = NULL WHERE name IN "
          "(SELECT nodeName FROM supports WHERE validHeight = ?)" << nNextHeight;

    for (auto it = takeoverUndo.crbegin(); it != takeoverUndo.crend(); ++it) {
        if (it->second.second.IsNull())
            db << "UPDATE nodes SET takeoverHeight = NULL, takeoverID = NULL, hash = NULL WHERE name = ?" << it->first;
        else
            db << "UPDATE nodes SET takeoverHeight = ?, takeoverID = ?, hash = NULL WHERE name = ?"
                  << it->second.first << it->second.second << it->first;
    }
    return true;
}

static const boost::container::flat_set<std::pair<int, std::string>> ownershipWorkaround = {
        { 297706, "firstvideo" },
        { 300045, "whatislbry" },
        { 305742, "mrrobots01e01" },
        { 350299, "gaz" },
        { 426898, "travtest01" },
        { 466336, "network" },
        { 481633, "11111111111111111111" },
        { 538169, "cmd" },
        { 539068, "who13" },
        { 552082, "right" },
        { 557322, "pixelboard" },
        { 562221, "stats331" },
        { 583305, "gauntlet-invade-the-darkness-lvl-1-of" },
        { 584733, "0000001" },
        { 584733, "0000003" },
        { 584733, "0000002" },
        { 588287, "livestream-project-reality-arma-3" },
        { 588308, "fr-let-s-play-software-inc-jay" },
        { 588308, "fr-motorsport-manager-jay-s-racing-5" },
        { 588308, "fr-motorsport-manager-jay-s-racing" },
        { 588318, "fr-hoi-iv-the-great-war-l-empire-2" },
        { 588318, "fr-stellaris-distant-stars-la-pr" },
        { 588318, "fr-stellaris-distant-stars-la-pr-2" },
        { 588318, "fr-crusader-kings-2-la-dynastie-6" },
        { 588318, "fr-jurassic-world-evolution-let-s-play" },
        { 588322, "fr-cold-waters-campagne-asie-2000-2" },
        { 588683, "calling-tech-support-scammers-live-3" },
        { 589013, "lets-play-jackbox-games-5" },
        { 589013, "let-s-play-jackbox-games" },
        { 589534, "let-s-play-the-nightmare-before" },
        { 589538, "kabutothesnake-s-live-ps4-broadcast" },
        { 589538, "back-with-fortnite" },
        { 589554, "no-eas-strong-thunderstorm-advisory" },
        { 589606, "new-super-mario-bros-wii-walkthrough" },
        { 589606, "samurai-warrior-chronicles-hero-rise" },
        { 589630, "ullash" },
        { 589640, "today-s-professionals-2018-winter-3" },
        { 589640, "let-s-run-a-mall-series-6-1-18-no-more" },
        { 589640, "today-s-professionals-2018-winter-4" },
        { 589641, "today-s-professionals-big-brother-6-14" },
        { 589641, "today-s-professionals-2018-winter-14" },
        { 589641, "today-s-professionals-big-brother-6-13" },
        { 589641, "today-s-professionals-big-brother-6-28" },
        { 589641, "today-s-professionals-2018-winter-6" },
        { 589641, "today-s-professionals-big-brother-6-26" },
        { 589641, "today-s-professionals-big-brother-6-27" },
        { 589641, "today-s-professionals-2018-winter-10" },
        { 589641, "today-s-professionals-2018-winter-7" },
        { 589641, "today-s-professionals-big-brother-6-29" },
        { 589760, "bobby-blades" },
        { 589831, "fifa-14-android-astrodude44-vs" },
        { 589849, "gaming-and-drawing-videos-live-stream" },
        { 589849, "gaming-with-silverwolf-live-stream-2" },
        { 589849, "gaming-with-silverwolf-live-stream-3" },
        { 589849, "gaming-with-silverwolf-videos-live" },
        { 589849, "gaming-with-silverwolf-live-stream-4" },
        { 589849, "gaming-with-silverwolf-live-stream-5" },
        { 589851, "gaming-with-silverwolf-live-stream-7" },
        { 589851, "gaming-with-silverwolf-live-stream-6" },
        { 589870, "classic-sonic-games" },
        { 589926, "j-dog7973-s-fortnite-squad" },
        { 589967, "wow-warlords-of-draenor-horde-side" },
        { 590020, "come-chill-with-rekzzey-2" },
        { 590033, "gothsnake-black-ops-ii-game-clip" },
        { 590074, "a-new-stream" },
        { 590075, "a-new-stream" },
        { 590082, "a-new-stream" },
        { 590116, "a-new-stream" },
        { 590178, "father-vs-son-stickfight-stickfight" },
        { 590178, "little-t-playing-subnautica-livestream" },
        { 590179, "my-family-trip-with-my-mom-and-sister" },
        { 590206, "pomskies" },
        { 590223, "dark-souls-iii-soul-level-1-challenge-2" },
        { 590223, "dark-souls-iii-soul-level-1-challenge" },
        { 590223, "dark-souls-iii-soul-level-1-challenge-3" },
        { 590225, "skyrim-special-edition-ps4-platinum-3" },
        { 590225, "skyrim-special-edition-ps4-platinum-4" },
        { 590225, "let-s-play-sniper-elite-4-authentic-2" },
        { 590226, "let-s-play-final-fantasy-the-zodiac-2" },
        { 590226, "let-s-play-final-fantasy-the-zodiac-3" },
        { 590401, "ls-h-ppchen-halloween-stream-vom-31-10" },
        { 591982, "destiny-the-taken-king-gameplay" },
        { 591984, "ghost-recon-wildlands-100-complete-4" },
        { 591986, "uncharted-the-lost-legacy-100-complete" },
        { 593535, "3-smg4-reactions-in-1-gabrieloreacts" },
        { 593550, "speed-runs-community-versus-video" },
        { 593551, "rayman-legends-challenges-app-murphy-s" },
        { 593551, "rayman-legends-challenges-app-the" },
        { 593726, "dmt-psychedelics-death-and-rebirth" },
        { 593726, "flat-earth-and-other-shill-potatoes" },
        { 593726, "why-everyone-s-leaving-youtube" },
        { 595537, "memory-techniques-1-000-people-system" },
        { 595556, "qik-mobile-video-by-paul-clifford" },
        { 595818, "ohare12345-s-live-ps4-broadcast" },
        { 595838, "super-smash-bros-u-3-minute-smash-as" },
        { 595838, "super-smash-bros-u-multi-man-smash-3" },
        { 595838, "super-smash-bros-u-target-blast-3" },
        { 595838, "super-smash-bros-u-donkey-kong-tourney" },
        { 595839, "super-smash-bros-u-super-mario-u-smash" },
        { 595841, "super-smash-bros-u-zelda-smash-series" },
        { 595841, "super-smash-bros-u-tournament-series" },
        { 595841, "super-smash-bros-u-link-tourney-mode-a" },
        { 595841, "super-smash-bros-u-brawl-co-op-event" },
        { 595841, "super-smash-bros-u-link-tourney-mode-b" },
        { 595842, "super-smash-bros-u-3-newcomers-the" },
        { 595844, "super-smash-bros-u-home-run-contest-2" },
        { 596829, "gramy-minecraft" },
        { 596829, "gramy-minecraft-jasmc-pl" },
        { 597635, "5-new-technology-innovations-in-5" },
        { 597658, "borderlands-2-tiny-tina-s-assault-on" },
        { 597658, "let-s-play-borderlands-the-pre-sequel" },
        { 597660, "caveman-world-mountains-of-unga-boonga" },
        { 597787, "playing-hypixel-webcam-and-mic" },
        { 597789, "if-herobrine-played-yandere-simulator" },
        { 597794, "user-registration-system-in-php-mysql" },
        { 597796, "let-s-play-mario-party-luigi-s-engine" },
        { 597796, "let-s-play-mario-party-dk-s-jungle" },
        { 597803, "asphalt-8-gameplay" },
        { 597817, "roblox-phantom-forces-no-audio-just" },
        { 597824, "best-funny-clip" },
        { 597825, "let-s-play-fallout-2-restoration-3" },
        { 597826, "saturday-night-baseball-with-3" },
        { 597826, "saturday-night-baseball-with-6" },
        { 597829, "payeer" },
        { 597831, "dreamtowards" },
        { 597833, "20000" },
        { 597833, "remme" },
        { 597834, "hycon" },
        { 597834, "hearthstone-heroes-of-warcraft-3" },
        { 597837, "15-curiosidades-que-probablemente-ya" },
        { 597893, "elder-scrolls-online-road-to-level-20" },
        { 597894, "elder-scrolls-legends-beta-gameplay" },
        { 597900, "fallout-4-walkthrough" },
        { 597901, "wwe-2k18-with-that-guy-and-tricky" },
        { 597902, "dead-space-walkthroug-wcommentary-part" },
        { 597903, "how-it-feels-to-chew-5-gum-funny" },
        { 597909, "president-obama-apec-press-conference" },
        { 597909, "100-5" },
        { 597910, "eat-the-street" },
        { 597923, "rocket-league-giveaway-3" },
        { 597923, "mortal-kombat-xl-livestream" },
        { 597930, "lets-play-spore-with" },
        { 597931, "lets-play-minecraft-with" },
        { 597932, "for-honor-4" },
        { 597933, "memorize-english-words-scientifically" },
        { 597933, "true-mov" },
        { 597935, "jugando-pokemon-esmeralda-gba" },
        { 597936, "gta-5-livestream-85-92" },
        { 597937, "battlefield-hardline-9-19" },
        { 598070, "mechwarrior-2-soundtrack-clan-jade" },
        { 606424, "@apostrophe" },
        { 615725, "amazonianhunter" },
        { 615726, "amazonianhunter" },
        { 630930, "cli" },
        { 638876, "@ordinary" },
        { 638878, "@ordinary" },
        { 644575, "ratio" },
        { 646584, "calling-tech-support-scammers-live-3" },
};

int CClaimTrieCacheBase::getDelayForName(const std::string& name, const uint160& claimId) const
{
    uint160 winningClaimId;
    int winningTakeoverHeight;
    auto found = getLastTakeoverForName(name, winningClaimId, winningTakeoverHeight);
    if (found && winningClaimId == claimId) {
        assert(winningTakeoverHeight <= nNextHeight);
        return 0;
    }

    if (nNextHeight <= 646584 && ownershipWorkaround.find(std::make_pair(nNextHeight, name)) != ownershipWorkaround.end())
        return 0;

    return found ? std::min((nNextHeight - winningTakeoverHeight) / base->nProportionalDelayFactor, 4032) : 0;
}

std::string CClaimTrieCacheBase::adjustNameForValidHeight(const std::string& name, int validHeight) const
{
    return name;
}

bool CClaimTrieCacheBase::getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof)
{
    // cache the parent nodes
    getMerkleHash();
    proof = CClaimTrieProof();
    auto nodeQuery = db << "SELECT name, IFNULL(takeoverHeight, 0) FROM nodes WHERE "
                                    "name IN (WITH RECURSIVE prefix(p) AS (VALUES(?) UNION ALL "
                                    "SELECT SUBSTR(p, 1, LENGTH(p)-1) FROM prefix WHERE p != '') SELECT p FROM prefix) "
                                    "ORDER BY LENGTH(name)" << name;
    for (auto&& row: nodeQuery) {
        CClaimValue claim;
        std::string key;
        int takeoverHeight;
        row >> key >> takeoverHeight;
        bool fNodeHasValue = getInfoForName(key, claim);
        uint256 valueHash;
        if (fNodeHasValue)
            valueHash = getValueHash(claim.outPoint, takeoverHeight);

        const auto pos = key.size();
        std::vector<std::pair<unsigned char, uint256>> children;
        auto childQuery = db << "SELECT name, hash FROM nodes WHERE parent = ?" << key;
        for (auto&& child : childQuery) {
            std::string childKey;
            uint256 hash;
            child >> childKey >> hash;
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
            completeHash(hash, childKey, pos);
            children.emplace_back(childKey[pos], hash);
        }
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

bool CClaimTrieCacheBase::findNameForClaim(const std::vector<unsigned char>& claim, CClaimValue& value, std::string& name) {
    auto query = db << "SELECT nodeName, claimId, txID, txN, amount, validHeight, blockHeight "
                              "FROM claims WHERE SUBSTR(claimID, 1, ?) = ? AND validHeight < ? AND expirationHeight >= ?"
                    << claim.size() << claim << nNextHeight << nNextHeight;
    auto hit = false;
    for (auto&& row: query) {
        if (hit) return false;
        row >> name >> value.claimId >> value.outPoint.hash >> value.outPoint.n
            >> value.nAmount >> value.nValidAtHeight >> value.nHeight;
        hit = true;
    }
    return hit;
}

void CClaimTrieCacheBase::getNamesInTrie(std::function<void(const std::string&)> callback)
{
    auto query = db << "SELECT DISTINCT nodeName FROM claims WHERE validHeight < ? AND expirationHeight >= ?"
                    << nNextHeight << nNextHeight;
    for (auto&& row: query) {
        std::string name;
        row >> name;
        callback(name);
    }
}
