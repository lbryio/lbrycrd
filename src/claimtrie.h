#ifndef BITCOIN_CLAIMTRIE_H
#define BITCOIN_CLAIMTRIE_H

#include <amount.h>
#include <serialize.h>
#include <uint256.h>
#include <util.h>
#include <dbwrapper.h>
#include <chainparams.h>
#include <primitives/transaction.h>

#include <string>
#include <vector>
#include <map>

// leveldb keys
#define HASH_BLOCK 'h'
#define CURRENT_HEIGHT 't'
#define TRIE_NODE 'n'
#define CLAIM_BY_ID 'i'
#define CLAIM_QUEUE_ROW 'r'
#define CLAIM_QUEUE_NAME_ROW 'm'
#define EXP_QUEUE_ROW 'e'
#define SUPPORT 's'
#define SUPPORT_QUEUE_ROW 'u'
#define SUPPORT_QUEUE_NAME_ROW 'p'
#define SUPPORT_EXP_QUEUE_ROW 'x'

uint256 getValueHash(COutPoint outPoint, int nHeightOfLastTakeover);

class CClaimValue
{
public:
    COutPoint outPoint;
    uint160 claimId;
    CAmount nAmount;
    CAmount nEffectiveAmount;
    int nHeight;
    int nValidAtHeight;

    CClaimValue() {};

    CClaimValue(COutPoint outPoint, uint160 claimId, CAmount nAmount, int nHeight,
                int nValidAtHeight)
                : outPoint(outPoint), claimId(claimId)
                , nAmount(nAmount), nEffectiveAmount(nAmount)
                , nHeight(nHeight), nValidAtHeight(nValidAtHeight)
    {}

    CClaimValue(CClaimValue&&) = default;
    CClaimValue(const CClaimValue&) = default;
    CClaimValue& operator=(CClaimValue&&) = default;
    CClaimValue& operator=(const CClaimValue&) = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(outPoint);
        READWRITE(claimId);
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(nValidAtHeight);
    }

    bool operator<(const CClaimValue& other) const
    {
        if (nEffectiveAmount < other.nEffectiveAmount)
            return true;
        else if (nEffectiveAmount == other.nEffectiveAmount)
        {
            if (nHeight > other.nHeight)
                return true;
            else if (nHeight == other.nHeight)
            {
                if (outPoint != other.outPoint && !(outPoint < other.outPoint))
                    return true;
            }
        }
        return false;
    }

    bool operator==(const CClaimValue& other) const
    {
        return outPoint == other.outPoint && claimId == other.claimId && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
    }

    bool operator!=(const CClaimValue& other) const
    {
        return !(*this == other);
    }
};

class CSupportValue
{
public:
    COutPoint outPoint;
    uint160 supportedClaimId;
    CAmount nAmount;
    int nHeight;
    int nValidAtHeight;

    CSupportValue() {};
    CSupportValue(COutPoint outPoint, uint160 supportedClaimId,
                  CAmount nAmount, int nHeight, int nValidAtHeight)
                  : outPoint(outPoint), supportedClaimId(supportedClaimId)
                  , nAmount(nAmount), nHeight(nHeight)
                  , nValidAtHeight(nValidAtHeight)
    {}

    CSupportValue(CSupportValue&&) = default;
    CSupportValue(const CSupportValue&) = default;
    CSupportValue& operator=(CSupportValue&&) = default;
    CSupportValue& operator=(const CSupportValue&) = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(outPoint);
        READWRITE(supportedClaimId);
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(nValidAtHeight);
    }

    bool operator==(const CSupportValue& other) const
    {
        return outPoint == other.outPoint && supportedClaimId == other.supportedClaimId && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
    }

    bool operator!=(const CSupportValue& other) const
    {
        return !(*this == other);
    }
};

class CClaimTrieNode;
class CClaimTrie;

typedef std::vector<CSupportValue> supportMapEntryType;
typedef std::map<unsigned char, CClaimTrieNode*> nodeMapType;

class CClaimTrieNode
{
public:
    CClaimTrieNode() : nHeightOfLastTakeover(0) {}
    CClaimTrieNode(uint256 hash) : hash(hash), nHeightOfLastTakeover(0) {}
    CClaimTrieNode(const CClaimTrieNode&) = default;
    CClaimTrieNode(CClaimTrieNode&& other)
    {
        hash = std::move(other.hash);
        claims = std::move(other.claims);
        children = std::move(other.children);
        nHeightOfLastTakeover = other.nHeightOfLastTakeover;
    }

    CClaimTrieNode& operator=(const CClaimTrieNode&) = default;
    CClaimTrieNode& operator=(CClaimTrieNode&& other)
    {
        if (this != &other)
        {
            hash = std::move(other.hash);
            claims = std::move(other.claims);
            children = std::move(other.children);
            nHeightOfLastTakeover = other.nHeightOfLastTakeover;
        }
        return *this;
    }

    uint256 hash;
    nodeMapType children;
    int nHeightOfLastTakeover;
    std::vector<CClaimValue> claims;

    bool insertClaim(CClaimValue claim);
    bool removeClaim(const COutPoint& outPoint, CClaimValue& claim);
    bool getBestClaim(CClaimValue& claim) const;
    bool empty() const {return children.empty() && claims.empty();}
    bool haveClaim(const COutPoint& outPoint) const;
    void reorderClaims(supportMapEntryType& supports);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(claims);
        READWRITE(nHeightOfLastTakeover);
    }

    bool operator==(const CClaimTrieNode& other) const
    {
        return hash == other.hash && claims == other.claims;
    }

    bool operator!=(const CClaimTrieNode& other) const
    {
        return !(*this == other);
    }
};

struct nodenamecompare
{
    bool operator() (const std::string& i, const std::string& j) const
    {
        if (i.size() == j.size())
            return i < j;
        return i.size() < j.size();
    }
};

struct outPointHeightType
{
    COutPoint outPoint;
    int nHeight;

    outPointHeightType() {}

    outPointHeightType(COutPoint outPoint, int nHeight)
    : outPoint(outPoint), nHeight(nHeight) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(outPoint);
        READWRITE(nHeight);
    }

};

struct nameOutPointHeightType
{
    std::string name;
    COutPoint outPoint;
    int nHeight;

    nameOutPointHeightType() {}

    nameOutPointHeightType(std::string name, COutPoint outPoint, int nHeight)
    : name(std::move(name)), outPoint(outPoint), nHeight(nHeight) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(outPoint);
        READWRITE(nHeight);
    }
};

struct nameOutPointType
{
    std::string name;
    COutPoint outPoint;

    nameOutPointType() {}

    nameOutPointType(std::string name, COutPoint outPoint)
    : name(std::move(name)), outPoint(outPoint) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(outPoint);
    }
};

class CClaimIndexElement
{
  public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(name);
        READWRITE(claim);
    }

    std::string name;
    CClaimValue claim;
};

typedef std::pair<std::string, CClaimValue> claimQueueEntryType;

typedef std::pair<std::string, CSupportValue> supportQueueEntryType;

typedef std::map<std::string, supportMapEntryType> supportMapType;

typedef std::vector<outPointHeightType> queueNameRowType;
typedef std::map<std::string, queueNameRowType> queueNameType;

typedef std::vector<nameOutPointHeightType> insertUndoType;

typedef std::vector<nameOutPointType> expirationQueueRowType;
typedef std::map<int, expirationQueueRowType> expirationQueueType;

typedef std::vector<claimQueueEntryType> claimQueueRowType;
typedef std::map<int, claimQueueRowType> claimQueueType;

typedef std::vector<supportQueueEntryType> supportQueueRowType;
typedef std::map<int, supportQueueRowType> supportQueueType;

typedef std::map<std::string, CClaimTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

typedef std::set<CClaimValue> claimIndexClaimListType;
typedef std::vector<CClaimIndexElement> claimIndexElementListType;

struct claimsForNameType
{
    std::vector<CClaimValue> claims;
    std::vector<CSupportValue> supports;
    int nLastTakeoverHeight;
    std::string name;

    claimsForNameType(std::vector<CClaimValue> claims, std::vector<CSupportValue> supports,
                      int nLastTakeoverHeight, const std::string& name)
    : claims(std::move(claims)), supports(std::move(supports)),
        nLastTakeoverHeight(nLastTakeoverHeight), name(name) {}

    claimsForNameType(const claimsForNameType&) = default;
    claimsForNameType(claimsForNameType&& other)
    {
        claims = std::move(other.claims);
        supports = std::move(other.supports);
        name = std::move(other.name);
        nLastTakeoverHeight = other.nLastTakeoverHeight;
    }

    claimsForNameType& operator=(const claimsForNameType&) = default;
    claimsForNameType& operator=(claimsForNameType&& other)
    {
        if (this != &other)
        {
            claims = std::move(other.claims);
            supports = std::move(other.supports);
            name = std::move(other.name);
            nLastTakeoverHeight = other.nLastTakeoverHeight;
        }
        return *this;
    }

    virtual ~claimsForNameType() {}
};

class CClaimTrieCacheBase;
class CClaimTrieCacheExpirationFork;

class CClaimTrie
{
public:
    CClaimTrie(bool fMemory = false, bool fWipe = false, int nProportionalDelayFactor = 32)
               : db(GetDataDir() / "claimtrie", 20 * 1024 * 1024, fMemory, fWipe, false)
               , nCurrentHeight(0), nExpirationTime(Params().GetConsensus().nOriginalClaimExpirationTime)
               , nProportionalDelayFactor(nProportionalDelayFactor)
               , root(uint256S("0000000000000000000000000000000000000000000000000000000000000001"))
    {}

    uint256 getMerkleHash();

    bool empty() const;
    void clear();

    bool checkConsistency() const;

    bool WriteToDisk();
    bool ReadFromDisk(bool check = false);

    bool getInfoForName(const std::string& name, CClaimValue& claim) const;
    bool getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const;

    std::vector<CClaimValue> getClaimsForName(const std::string& name) const;

    bool queueEmpty() const;
    bool supportEmpty() const;
    bool supportQueueEmpty() const;
    bool expirationQueueEmpty() const;

    void setExpirationTime(int t);

    void addToClaimIndex(const std::string& name, const CClaimValue& claim);
    void removeFromClaimIndex(const CClaimValue& claim);

    bool getClaimById(const uint160 claimId, std::string& name, CClaimValue& claim) const;

    bool haveClaim(const std::string& name, const COutPoint& outPoint) const;
    bool haveClaimInQueue(const std::string& name, const COutPoint& outPoint,
                          int& nValidAtHeight) const;

    bool haveSupport(const std::string& name, const COutPoint& outPoint) const;
    bool haveSupportInQueue(const std::string& name, const COutPoint& outPoint,
                            int& nValidAtHeight) const;

    unsigned int getTotalNamesInTrie() const;
    unsigned int getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;

    friend class CClaimTrieCacheBase;
    friend class CClaimTrieCacheExpirationFork;

    CDBWrapper db;
    int nCurrentHeight;
    int nExpirationTime;
    int nProportionalDelayFactor;

private:
    void clear(CClaimTrieNode* current);

    const CClaimTrieNode* getNodeForName(const std::string& name) const;

    bool update(nodeCacheType& cache, hashMapType& hashes,
                std::map<std::string, int>& takeoverHeights,
                const uint256& hashBlock, claimQueueType& queueCache,
                queueNameType& queueNameCache,
                expirationQueueType& expirationQueueCache, int nNewHeight,
                supportMapType& supportCache,
                supportQueueType& supportQueueCache,
                queueNameType& supportQueueNameCache,
                expirationQueueType& supportExpirationQueueCache);
    bool updateName(const std::string& name, CClaimTrieNode* updatedNode);
    bool updateHash(const std::string& name, uint256& hash);
    bool updateTakeoverHeight(const std::string& name, int nTakeoverHeight);
    bool recursiveNullify(CClaimTrieNode* node, const std::string& name);

    bool recursiveCheckConsistency(const CClaimTrieNode* node) const;

    bool InsertFromDisk(const std::string& name, CClaimTrieNode* node);

    unsigned int getTotalNamesRecursive(const CClaimTrieNode* current) const;
    unsigned int getTotalClaimsRecursive(const CClaimTrieNode* current) const;
    CAmount getTotalValueOfClaimsRecursive(const CClaimTrieNode* current,
                                           bool fControllingOnly) const;

    bool getQueueRow(int nHeight, claimQueueRowType& row) const;
    bool getQueueNameRow(const std::string& name, queueNameRowType& row) const;
    bool getExpirationQueueRow(int nHeight, expirationQueueRowType& row) const;
    bool getSupportNode(std::string name, supportMapEntryType& node) const;
    bool getSupportQueueRow(int nHeight, supportQueueRowType& row) const;
    bool getSupportQueueNameRow(const std::string& name, queueNameRowType& row) const;
    bool getSupportExpirationQueueRow(int nHeight, expirationQueueRowType& row) const;

    void markNodeDirty(const std::string& name, CClaimTrieNode* node);
    void updateQueueRow(int nHeight, claimQueueRowType& row);
    void updateQueueNameRow(const std::string& name,
                            queueNameRowType& row);
    void updateExpirationRow(int nHeight, expirationQueueRowType& row);
    void updateSupportMap(const std::string& name, supportMapEntryType& node);
    void updateSupportQueue(int nHeight, supportQueueRowType& row);
    void updateSupportNameQueue(const std::string& name,
                                queueNameRowType& row);
    void updateSupportExpirationQueue(int nHeight, expirationQueueRowType& row);

    void BatchWriteNode(CDBBatch& batch, const std::string& name,
                        const CClaimTrieNode* pNode) const;
    void BatchWriteQueueRows(CDBBatch& batch);
    void BatchWriteQueueNameRows(CDBBatch& batch);
    void BatchWriteExpirationQueueRows(CDBBatch& batch);
    void BatchWriteSupportNodes(CDBBatch& batch);
    void BatchWriteSupportQueueRows(CDBBatch& batch);
    void BatchWriteSupportQueueNameRows(CDBBatch& batch);
    void BatchWriteSupportExpirationQueueRows(CDBBatch& batch);
    template<typename K> bool keyTypeEmpty(char key, K& dummy) const;

    CClaimTrieNode root;
    uint256 hashBlock;

    claimQueueType dirtyQueueRows;
    queueNameType dirtyQueueNameRows;
    expirationQueueType dirtyExpirationQueueRows;

    supportQueueType dirtySupportQueueRows;
    queueNameType dirtySupportQueueNameRows;
    expirationQueueType dirtySupportExpirationQueueRows;

    nodeCacheType dirtyNodes;
    supportMapType dirtySupportNodes;
};

class CClaimTrieProofNode
{
public:
    CClaimTrieProofNode() {};
    CClaimTrieProofNode(std::vector<std::pair<unsigned char, uint256> > children,
                        bool hasValue, uint256 valHash)
        : children(std::move(children)), hasValue(hasValue), valHash(std::move(valHash))
        {};
    CClaimTrieProofNode(const CClaimTrieProofNode&) = default;
    CClaimTrieProofNode(CClaimTrieProofNode&& other)
    {
        hasValue = other.hasValue;
        valHash = std::move(other.valHash);
        children = std::move(other.children);
    }
    CClaimTrieProofNode& operator=(const CClaimTrieProofNode&) = default;
    CClaimTrieProofNode& operator=(CClaimTrieProofNode&& other)
    {
        if (this != &other)
        {
            hasValue = other.hasValue;
            valHash = std::move(other.valHash);
            children = std::move(other.children);
        }
        return *this;
    }

    std::vector<std::pair<unsigned char, uint256> > children;
    bool hasValue;
    uint256 valHash;
};

class CClaimTrieProof
{
public:
    CClaimTrieProof() {};
    CClaimTrieProof(std::vector<CClaimTrieProofNode> nodes, bool hasValue, COutPoint outPoint, int nHeightOfLastTakeover) : nodes(std::move(nodes)), hasValue(hasValue), outPoint(outPoint), nHeightOfLastTakeover(nHeightOfLastTakeover) {}
    CClaimTrieProof(const CClaimTrieProof&) = default;
    CClaimTrieProof(CClaimTrieProof&& other)
    {
        hasValue = other.hasValue;
        outPoint = std::move(other.outPoint);
        nodes = std::move(other.nodes);
        nHeightOfLastTakeover = other.nHeightOfLastTakeover;
    }

    CClaimTrieProof& operator=(const CClaimTrieProof&) = default;
    CClaimTrieProof& operator=(CClaimTrieProof&& other)
    {
        if (this != &other)
        {
            hasValue = other.hasValue;
            outPoint = std::move(other.outPoint);
            nodes = std::move(other.nodes);
            nHeightOfLastTakeover = other.nHeightOfLastTakeover;
        }
        return *this;
    }

    std::vector<CClaimTrieProofNode> nodes;
    bool hasValue;
    COutPoint outPoint;
    int nHeightOfLastTakeover;
};

struct CNodeCallback {
    struct CRecursionInterruptionException : public std::exception {
        const bool success;
        explicit CRecursionInterruptionException(bool success) : success(success) {}
    };

    virtual ~CNodeCallback()
    {
    }

    /**
     * Callback to be called on every trie node
     * @param[in] name      full name of the node
     * @param[in] node      pointer to node itself
     *
     * To breakout early throw an exception.
     * Throwing CRecursionInterruptionException will allow you to set the return value of iterateTrie.
     */
    virtual void visit(const std::string& name, const CClaimTrieNode* node) = 0;
};

class CClaimTrieCacheBase
{
public:
    CClaimTrieCacheBase(CClaimTrie* base, bool fRequireTakeoverHeights = true)
      : base(base), fRequireTakeoverHeights(fRequireTakeoverHeights)
    {
        assert(base);
        nCurrentHeight = base->nCurrentHeight;
    }

    uint256 getMerkleHash(bool forceCompute = false) const;

    bool empty() const;
    bool flush();
    bool dirty() const { return !dirtyHashes.empty(); }

    CClaimTrieNode* getRoot() const
    {
        const auto iter = cache.find("");
        return iter == cache.end() ? &(base->root) : iter->second;
    }

    bool addClaim(const std::string& name, const COutPoint& outPoint,
                  uint160 claimId, CAmount nAmount, int nHeight) const;
    bool undoAddClaim(const std::string& name, const COutPoint& outPoint, int nHeight) const;
    bool spendClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight) const;
    bool undoSpendClaim(const std::string& name, const COutPoint& outPoint,
                        uint160 claimId, CAmount nAmount, int nHeight,
                        int nValidAtHeight) const;

    bool addSupport(const std::string& name, const COutPoint& outPoint,
                    CAmount nAmount, uint160 supportedClaimId,
                    int nHeight) const;
    bool undoAddSupport(const std::string& name, const COutPoint& outPoint,
                        int nHeight) const;
    bool spendSupport(const std::string& name, const COutPoint& outPoint,
                      int nHeight, int& nValidAtHeight) const;
    bool undoSpendSupport(const std::string& name, const COutPoint& outPoint,
                          uint160 supportedClaimId, CAmount nAmount,
                          int nHeight, int nValidAtHeight) const;

    uint256 getBestBlock();
    void setBestBlock(const uint256& hashBlock);

    virtual bool incrementBlock(insertUndoType& insertUndo,
                                claimQueueRowType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportQueueRowType& expireSupportUndo,
                                std::vector<std::pair<std::string, int> >& takeoverHeightUndo);
    virtual bool decrementBlock(insertUndoType& insertUndo,
                                claimQueueRowType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportQueueRowType& expireSupportUndo,
                                std::vector<std::pair<std::string, int> >& takeoverHeightUndo);

    virtual ~CClaimTrieCacheBase() { clear(); }

    virtual bool getProofForName(const std::string& name, CClaimTrieProof& proof) const;
    virtual bool getInfoForName(const std::string& name, CClaimValue& claim) const;

    bool finalizeDecrement() const;

    bool iterateTrie(CNodeCallback& callback) const;

    virtual claimsForNameType getClaimsForName(const std::string& name) const;

    CAmount getEffectiveAmountForClaim(const std::string& name, const uint160& claimId,
                                       std::vector<CSupportValue>* supports = nullptr) const;
    CAmount getEffectiveAmountForClaim(const claimsForNameType& claims, const uint160& claimId,
                                       std::vector<CSupportValue>* supports = nullptr) const;

protected:
    // Should be private: Do not use unless you know what you're doing.
    CClaimTrieNode* addNodeToCache(const std::string& position, CClaimTrieNode* original) const;
    bool recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent,
                                    const std::string& sPos,
                                    bool forceCompute = false) const;
    bool recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos, const std::string& sName, bool* pfNullified = nullptr) const;
    void checkNamesForTakeover(insertUndoType& insertUndo, insertUndoType& insertSupportUndo,
                               std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const;

    virtual bool insertClaimIntoTrie(const std::string& name, CClaimValue claim,
                                     bool fCheckTakeover = false) const;
    virtual bool removeClaimFromTrie(const std::string& name, const COutPoint& outPoint,
                                     CClaimValue& claim,
                                     bool fCheckTakeover = false) const;

    virtual bool insertSupportIntoMap(const std::string& name,
                                      CSupportValue support,
                                      bool fCheckTakeover) const;
    virtual bool removeSupportFromMap(const std::string& name, const COutPoint& outPoint,
                                      CSupportValue& support,
                                      bool fCheckTakeover) const;

    virtual void addClaimToQueues(const std::string& name, CClaimValue& claim) const;
    virtual bool addSupportToQueues(const std::string& name, CSupportValue& support) const;
    virtual std::string adjustNameForValidHeight(const std::string& name, int validHeight) const;

    void addToExpirationQueue(int nExpirationHeight, nameOutPointType& entry) const;
    void removeFromExpirationQueue(const std::string& name, const COutPoint& outPoint,
                                   int nHeight) const;

    void addSupportToExpirationQueue(int nExpirationHeight,
                                     nameOutPointType& entry) const;
    void removeSupportFromExpirationQueue(const std::string& name,
                                          const COutPoint& outPoint,
                                          int nHeight) const;

    bool getSupportsForName(const std::string& name,
                            supportMapEntryType& supports) const;

    virtual int getDelayForName(const std::string& name, const uint160& claimId) const;

    mutable nodeCacheType cache;
    CClaimTrie* base;
    mutable int nCurrentHeight; // Height of the block that is being worked on, which is
    // one greater than the height of the chain's tip

private:

    bool fRequireTakeoverHeights;

    mutable nodeCacheType block_originals;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    mutable claimQueueType claimQueueCache;
    mutable queueNameType claimQueueNameCache;
    mutable expirationQueueType expirationQueueCache;
    mutable supportMapType supportCache;
    mutable supportQueueType supportQueueCache;
    mutable queueNameType supportQueueNameCache;
    mutable expirationQueueType supportExpirationQueueCache;
    mutable std::set<std::string> namesToCheckForTakeover;
    mutable std::map<std::string, int> cacheTakeoverHeights;
    mutable claimIndexElementListType claimsToAdd;
    mutable claimIndexClaimListType claimsToDelete;

    uint256 hashBlock;

    bool reorderTrieNode(const std::string& name, bool fCheckTakeover) const;

    bool clear() const;

    // generally the opposite of addClaimToQueues, but they aren't perfectly symmetrical:
    bool removeClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover) const;
    bool removeClaimFromQueue(const std::string& name, const COutPoint& outPoint,
                              CClaimValue& claim) const;

    claimQueueType::iterator getQueueCacheRow(int nHeight,
                                              bool createIfNotExists) const;
    queueNameType::iterator getQueueCacheNameRow(const std::string& name,
                                                 bool createIfNotExists) const;
    expirationQueueType::iterator getExpirationQueueCacheRow(int nHeight,
                                                             bool createIfNotExists) const;

    bool removeSupport(const std::string& name, const COutPoint& outPoint,
                       int nHeight, int& nValidAtHeight,
                       bool fCheckTakeover) const;

    supportQueueType::iterator getSupportQueueCacheRow(int nHeight,
                                                       bool createIfNotExists) const;
    queueNameType::iterator getSupportQueueCacheNameRow(const std::string& name,
                                                        bool createIfNotExists) const;
    expirationQueueType::iterator getSupportExpirationQueueCacheRow(int nHeight,
                                                                    bool createIfNotExists) const;

    bool removeSupportFromQueue(const std::string& name, const COutPoint& outPoint,
                                CSupportValue& support) const;

    bool getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const;

    int getDelayForName(const std::string& name) const;

    uint256 getLeafHashForProof(const std::string& currentPosition, const CClaimTrieNode* currentNode) const;

    bool getOriginalInfoForName(const std::string& name, CClaimValue& claim) const;

    int getNumBlocksOfContinuousOwnership(const std::string& name) const;

    void recursiveIterateTrie(std::string& name, const CClaimTrieNode* current, CNodeCallback& callback) const;

    const CClaimTrieNode* getNodeForName(const std::string& name) const;
};

class CClaimTrieCacheExpirationFork: public CClaimTrieCacheBase {
  public:
    CClaimTrieCacheExpirationFork(CClaimTrie* base, bool fRequireTakeoverHeights = true)
        : CClaimTrieCacheBase(base, fRequireTakeoverHeights) {}

    virtual ~CClaimTrieCacheExpirationFork() {}

    bool forkForExpirationChange(bool increment) const;

    // TODO: move the expiration fork code from main.cpp to overrides of increment/decrement block

private:
    void removeAndAddSupportToExpirationQueue(expirationQueueRowType &row, int height, bool increment) const;
    void removeAndAddToExpirationQueue(expirationQueueRowType &row, int height, bool increment) const;
};

class CClaimTrieCacheNormalizationFork: public CClaimTrieCacheExpirationFork {
  public:
    CClaimTrieCacheNormalizationFork(CClaimTrie* base, bool fRequireTakeoverHeights = true)
        : CClaimTrieCacheExpirationFork(base, fRequireTakeoverHeights),
        overrideInsertNormalization(false), overrideRemoveNormalization(false) {}

    virtual ~CClaimTrieCacheNormalizationFork() {}

    bool shouldNormalize() const;

    // lower-case and normalize any input string name
    // see: https://unicode.org/reports/tr15/#Norm_Forms
    std::string normalizeClaimName(const std::string& name, bool force = false) const; // public only for validating name field on update op

    virtual bool incrementBlock(insertUndoType& insertUndo,
                                claimQueueRowType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportQueueRowType& expireSupportUndo,
                                std::vector<std::pair<std::string, int> >& takeoverHeightUndo);
    virtual bool decrementBlock(insertUndoType& insertUndo,
                                claimQueueRowType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportQueueRowType& expireSupportUndo,
                                std::vector<std::pair<std::string, int> >& takeoverHeightUndo);

    virtual bool getProofForName(const std::string& name, CClaimTrieProof& proof) const;
    virtual bool getInfoForName(const std::string& name, CClaimValue& claim) const;
    virtual claimsForNameType getClaimsForName(const std::string& name) const;

protected:
    virtual bool insertClaimIntoTrie(const std::string& name, CClaimValue claim, bool fCheckTakeover = false) const;
    virtual bool removeClaimFromTrie(const std::string& name, const COutPoint& outPoint,
                                     CClaimValue& claim, bool fCheckTakeover = false) const;

    virtual bool insertSupportIntoMap(const std::string& name, CSupportValue support, bool fCheckTakeover) const;
    virtual bool removeSupportFromMap(const std::string& name, const COutPoint& outPoint,
                                      CSupportValue& support, bool fCheckTakeover) const;
    virtual int getDelayForName(const std::string& name, const uint160& claimId) const;

    virtual void addClaimToQueues(const std::string& name, CClaimValue& claim) const;
    virtual bool addSupportToQueues(const std::string& name, CSupportValue& support) const;
    virtual std::string adjustNameForValidHeight(const std::string& name, int validHeight) const;

private:
    bool overrideInsertNormalization, overrideRemoveNormalization;
    bool normalizeAllNamesInTrieIfNecessary(insertUndoType& insertUndo, claimQueueRowType& removeUndo,
                                            insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo,
                                            std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const;
};

typedef CClaimTrieCacheNormalizationFork CClaimTrieCache;

#endif // BITCOIN_CLAIMTRIE_H
