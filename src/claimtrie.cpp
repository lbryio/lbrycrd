
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
queueNameRowType* CClaimTrieCacheBase::getQueueCacheNameRow<CClaimValue>(const std::string& name, bool createIfNotExists)
{
    return getQueue(*(base->db), CLAIM_QUEUE_NAME_ROW, name, claimQueueNameCache, createIfNotExists);
}

template <>
queueNameRowType* CClaimTrieCacheBase::getQueueCacheNameRow<CSupportValue>(const std::string& name, bool createIfNotExists)
{
    return getQueue(*(base->db), SUPPORT_QUEUE_NAME_ROW, name, supportQueueNameCache, createIfNotExists);
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
expirationQueueRowType* CClaimTrieCacheBase::getExpirationQueueCacheRow<CClaimValue>(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), CLAIM_EXP_QUEUE_ROW, nHeight, expirationQueueCache, createIfNotExists);
}

template <>
expirationQueueRowType* CClaimTrieCacheBase::getExpirationQueueCacheRow<CSupportValue>(int nHeight, bool createIfNotExists)
{
    return getQueue(*(base->db), SUPPORT_EXP_QUEUE_ROW, nHeight, supportExpirationQueueCache, createIfNotExists);
}

template <typename T>
expirationQueueRowType* CClaimTrieCacheBase::getExpirationQueueCacheRow(int, bool)
{
    supportedType<T>();
    return nullptr;
}

bool CClaimTrieCacheBase::haveClaim(const std::string& name, const COutPoint& outPoint) const
{
    auto it = nodesToAddOrUpdate.find(name);
    if (it && it->haveClaim(outPoint))
        return true;
    if (it || nodesToDelete.count(name))
        return false;
    CClaimTrieData data;
    return base->find(name, data) && data.haveClaim(outPoint);
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

void CClaimTrie::recurseNodes(const std::string& name, const CClaimTrieDataNode& current, std::function<recurseNodesCB> function) const {
    CClaimTrieData data;
    find(name, data);

    data.hash = current.hash;
    data.flags |= current.children.empty() ? 0 : CClaimTrieDataFlags::POTENTIAL_CHILDREN;
    function(name, data, current.children);

    for (auto& child: current.children) {
        CClaimTrieDataNode node;
        auto childName = name + child;
        if (find(childName, node))
            recurseNodes(childName, node, function);
    }
}

std::size_t CClaimTrie::getTotalNamesInTrie() const
{
    std::size_t count = 0;
    CClaimTrieDataNode node;
    if (find({}, node))
        recurseNodes({}, node, [&count](const std::string &name, const CClaimTrieData &data, const std::vector<std::string>& children) {
            count += !data.empty();
        });
    return count;
}

std::size_t CClaimTrie::getTotalClaimsInTrie() const
{
    std::size_t count = 0;
    CClaimTrieDataNode node;
    if (find({}, node))
        recurseNodes({}, node, [&count]
        (const std::string &name, const CClaimTrieData &data, const std::vector<std::string>& children) {
            count += data.claims.size();
        });
    return count;
}

CAmount CClaimTrie::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    CAmount value_in_subtrie = 0;
    CClaimTrieDataNode node;
    if (find({}, node))
        recurseNodes({}, node, [&value_in_subtrie, fControllingOnly]
        (const std::string &name, const CClaimTrieData &data, const std::vector<std::string>& children) {
            for (const auto &claim : data.claims) {
                value_in_subtrie += claim.nAmount;
                if (fControllingOnly)
                    break;
            }
        });
    return value_in_subtrie;
}

bool CClaimTrieCacheBase::getInfoForName(const std::string& name, CClaimValue& claim) const
{
    auto it = nodesToAddOrUpdate.find(name);
    if (it && it->getBestClaim(claim))
        return true;
    if (it || nodesToDelete.count(name))
        return false;
    CClaimTrieData claims;
    return base->find(name, claims) && claims.getBestClaim(claim);
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

    if (auto it = nodesToAddOrUpdate.find(name)) {
        claims = it->claims;
        nLastTakeoverHeight = it->nHeightOfLastTakeover;
    } else if (!nodesToDelete.count(name)) {
        CClaimTrieData data;
        if (base->find(name, data)) {
            claims = data.claims;
            nLastTakeoverHeight = data.nHeightOfLastTakeover;
        }
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

bool CClaimTrie::checkConsistency(const uint256& rootHash) const
{
    CClaimTrieDataNode node;
    if (!find({}, node) || node.hash != rootHash) {
        if (rootHash == one)
            return true;

        return error("Mismatched root claim trie hashes. This may happen when there is not a clean process shutdown. Please run with -reindex.");
    }

    bool success = true;
    recurseNodes({}, node, [&success, this](const std::string &name, const CClaimTrieData &data, const std::vector<std::string>& children) {
        if (!success) return;

        std::vector<uint8_t> vchToHash;
        const auto pos = name.size();
        for (auto &child : children) {
            auto key = name + child;
            CClaimTrieDataNode node;
            success &= find(key, node);
            auto hash = node.hash;
            completeHash(hash, key, pos);
            vchToHash.push_back(key[pos]);
            vchToHash.insert(vchToHash.end(), hash.begin(), hash.end());
        }

        CClaimValue claim;
        if (data.getBestClaim(claim)) {
            uint256 valueHash = getValueHash(claim.outPoint, data.nHeightOfLastTakeover);
            vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
        } else {
            success &= !children.empty(); // we disallow leaf nodes without claims
        }
        success &= data.hash == Hash(vchToHash.begin(), vchToHash.end());
    });

    return success;
}

std::vector<std::pair<std::string, CClaimTrieDataNode>> CClaimTrie::nodes(const std::string &key) const {
    std::vector<std::pair<std::string, CClaimTrieDataNode>> ret;
    CClaimTrieDataNode node;

    if (!find({}, node))
        return ret;

    ret.emplace_back(std::string{}, node);

    std::string partialKey = key;

    while (!node.children.empty()) {
        // auto it = node.children.lower_bound(partialKey); // for using a std::map
        auto it = std::lower_bound(node.children.begin(), node.children.end(), partialKey);
        if (it != node.children.end() && *it == partialKey) {
            // we're completely done
            if (find(key, node))
                ret.emplace_back(key, node);
            break;
        }
        if (it != node.children.begin()) --it;
        const auto count = match(partialKey, *it);

        if (count != it->size()) break;
        if (count == partialKey.size()) break;
        partialKey = partialKey.substr(count);
        auto frontKey = key.substr(0, key.size() - partialKey.size());
        if (find(frontKey, node))
            ret.emplace_back(frontKey, node);
        else break;
    }

    return ret;
}

bool CClaimTrie::contains(const std::string &key) const {
    return db->Exists(std::make_pair(TRIE_NODE_CHILDREN, key));
}

bool CClaimTrie::empty() const {
    return !contains({});
}

bool CClaimTrie::find(const std::string& key, CClaimTrieDataNode &node) const {
    return db->Read(std::make_pair(TRIE_NODE_CHILDREN, key), node);
}

bool CClaimTrie::find(const std::string& key, CClaimTrieData &data) const {
    return db->Read(std::make_pair(TRIE_NODE_CLAIMS, key), data);
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

    for (auto it = nodesToAddOrUpdate.begin(); it != nodesToAddOrUpdate.end(); ++it) {
        bool removed = forDeleteFromBase.erase(it.key());
        if (it->flags & CClaimTrieDataFlags::HASH_DIRTY) {
            CClaimTrieDataNode node;
            node.hash = it->hash;
            for (auto &child: it.children()) // ordering here is important
                node.children.push_back(child.key().substr(it.key().size()));

            batch.Write(std::make_pair(TRIE_NODE_CHILDREN, it.key()), node);

            if (removed || (it->flags & CClaimTrieDataFlags::CLAIMS_DIRTY))
                batch.Write(std::make_pair(TRIE_NODE_CLAIMS, it.key()), it.data());
        }
    }

    for (auto& name: forDeleteFromBase) {
        batch.Erase(std::make_pair(TRIE_NODE_CHILDREN, name));
        batch.Erase(std::make_pair(TRIE_NODE_CLAIMS, name));
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

bool CClaimTrieCacheBase::validateTrieConsistency(const CBlockIndex* tip)
{
    if (!tip || tip->nHeight < 1)
        return true;

    LogPrintf("Checking claim trie consistency... ");
    if (base->checkConsistency(tip->hashClaimTrie)) {
        LogPrintf("consistent\n");
        return true;
    }
    LogPrintf("inconsistent!\n");
    return false;
}

bool CClaimTrieCacheBase::ReadFromDisk(const CBlockIndex* tip)
{
    base->nNextHeight = nNextHeight = tip ? tip->nHeight + 1 : 0;
    clear();

    if (tip && base->db->Exists(std::make_pair(TRIE_NODE, std::string()))) {
        LogPrintf("The claim trie database contains deprecated data and will need to be rebuilt\n");
        return false;
    }
    return validateTrieConsistency(tip);
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

uint256 CClaimTrieCacheBase::recursiveComputeMerkleHash(CClaimPrefixTrie::iterator& it)
{
    if (!it->hash.IsNull())
        return it->hash;

    std::vector<uint8_t> vchToHash;
    const auto pos = it.key().size();
    for (auto& child : it.children()) {
        auto hash = recursiveComputeMerkleHash(child);
        auto& key = child.key();
        completeHash(hash, key, pos);
        vchToHash.push_back(key[pos]);
        vchToHash.insert(vchToHash.end(), hash.begin(), hash.end());
    }

    CClaimValue claim;
    if (it->getBestClaim(claim)) {
        uint256 valueHash = getValueHash(claim.outPoint, it->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    return it->hash = Hash(vchToHash.begin(), vchToHash.end());
}

uint256 CClaimTrieCacheBase::getMerkleHash()
{
    if (auto it = nodesToAddOrUpdate.begin())
        return recursiveComputeMerkleHash(it);
    if (nodesToDelete.empty() && nodesAlreadyCached.empty()) {
        CClaimTrieDataNode node;
        if (base->find({}, node))
            return node.hash; // it may be valuable to have base cache its current root hash
    }
    return one; // we have no data or we deleted everything
}

CClaimPrefixTrie::const_iterator CClaimTrieCacheBase::begin() const
{
    return nodesToAddOrUpdate.begin();
}

CClaimPrefixTrie::const_iterator CClaimTrieCacheBase::end() const
{
    return nodesToAddOrUpdate.end();
}

bool CClaimTrieCacheBase::empty() const
{
    return nodesToAddOrUpdate.empty();
}

CClaimPrefixTrie::iterator CClaimTrieCacheBase::cacheData(const std::string& name, bool create)
{
    // we need all parent nodes and their one level deep children
    // to calculate merkle hash
    auto nodes = base->nodes(name);
    for (auto& node: nodes) {
        if (nodesAlreadyCached.insert(node.first).second) {
            // do not insert nodes that are already present
            CClaimTrieData data;
            base->find(node.first, data);
            data.hash = node.second.hash;
            data.flags = node.second.children.empty() ? 0 : CClaimTrieDataFlags::POTENTIAL_CHILDREN;
            nodesToAddOrUpdate.insert(node.first, data);
        }
        for (auto& child : node.second.children) {
            auto childKey = node.first + child;
            if (nodesAlreadyCached.insert(childKey).second) {
                CClaimTrieData childData;
                if (!base->find(childKey, childData))
                    childData = {};
                CClaimTrieDataNode childNode;
                if (base->find(childKey, childNode)) {
                    childData.hash = childNode.hash;
                    childData.flags = childNode.children.empty() ? 0 : CClaimTrieDataFlags::POTENTIAL_CHILDREN;
                }
                nodesToAddOrUpdate.insert(childKey, childData);
            }
        }
    }

    auto it = nodesToAddOrUpdate.find(name);
    if (!it && create) {
        it = nodesToAddOrUpdate.insert(name, CClaimTrieData{});
        // if (it.hasChildren()) any children should be in the trie (not base alone)
        //    it->flags |= CClaimTrieDataFlags::POTENTIAL_CHILDREN;
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
    CClaimTrieData data;
    if (base->find(name, data)) {
        takeoverHeight = data.nHeightOfLastTakeover;
        CClaimValue claim;
        if (data.getBestClaim(claim)) {
            claimId = claim.claimId;
            return true;
        }
    }
    return false;
}

void CClaimTrieCacheBase::markAsDirty(const std::string& name, bool fCheckTakeover)
{
    for (auto& node : nodesToAddOrUpdate.nodes(name)) {
        node->flags |= CClaimTrieDataFlags::HASH_DIRTY;
        node->hash.SetNull();
        if (node.key() == name)
            node->flags |= CClaimTrieDataFlags::CLAIMS_DIRTY;
    }

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

        for (auto& node : nodesToAddOrUpdate.nodes(name))
            forDeleteFromBase.emplace(node.key());

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

void CClaimTrieCacheBase::dumpToLog(CClaimPrefixTrie::const_iterator it, bool diffFromBase) const
{
    if (!it) return;

    if (diffFromBase) {
        CClaimTrieDataNode node;
        if (base->find(it.key(), node) && node.hash == it->hash)
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
    if (auto it = nodesToAddOrUpdate.find(name))
        return it->empty() ? 0 : nNextHeight - it->nHeightOfLastTakeover;
    CClaimTrieData data;
    if (base->find(name, data) && !data.empty())
        return nNextHeight - data.nHeightOfLastTakeover;
    return 0;
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
    forDeleteFromBase.clear();
    nodesToAddOrUpdate.clear();
    claimsToAddToByIdIndex.clear();
    supportCache.clear();
    nodesToDelete.clear();
    claimsToDeleteFromByIdIndex.clear();
    takeoverCache.clear();
    claimQueueCache.clear();
    supportQueueCache.clear();
    nodesAlreadyCached.clear();
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
    // cache the parent nodes
    cacheData(name, false);
    getMerkleHash();
    proof = CClaimTrieProof();
    for (auto& it : static_cast<const CClaimPrefixTrie&>(nodesToAddOrUpdate).nodes(name)) {
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
                    children.clear(); // move promises to leave it in a valid state only
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

void CClaimTrieCacheBase::recurseNodes(const std::string &name,
        std::function<void(const std::string &, const CClaimTrieData &)> function) const {

    std::function<CClaimTrie::recurseNodesCB> baseFunction = [this, &function]
            (const std::string& name, const CClaimTrieData& data, const std::vector<std::string>&) {
        if (nodesToDelete.find(name) == nodesToDelete.end())
            function(name, data);
    };

    if (empty()) {
        CClaimTrieDataNode node;
        if (base->find(name, node))
            base->recurseNodes(name, node, baseFunction);
    }
    else {
        for (auto it = begin(); it != end(); ++it) {
            function(it.key(), it.data());
            if ((it->flags & CClaimTrieDataFlags::POTENTIAL_CHILDREN) && !it.hasChildren()) {
                CClaimTrieDataNode node;
                if (base->find(it.key(), node))
                    for (auto& partialKey: node.children) {
                        auto childKey = it.key() + partialKey;
                        CClaimTrieDataNode childNode;
                        if (base->find(childKey, childNode))
                            base->recurseNodes(childKey, childNode, baseFunction);
                    }
            }
        }
    }
}
