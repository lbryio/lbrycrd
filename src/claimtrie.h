#ifndef BITCOIN_CLAIMTRIE_H
#define BITCOIN_CLAIMTRIE_H

#include "amount.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "dbwrapper.h"
#include "primitives/transaction.h"

#include <string>
#include <vector>

// leveldb keys
#define HASH_BLOCK 'h'
#define CURRENT_HEIGHT 't'
#define TRIE_NODE 'n'
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

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
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
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
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

typedef std::pair<std::string, CClaimTrieNode> namedNodeType;

class CClaimTrieNode
{
public:
    CClaimTrieNode() : nHeightOfLastTakeover(0) {}
    CClaimTrieNode(uint256 hash) : hash(hash), nHeightOfLastTakeover(0) {}
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
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
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
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
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
    : name(name), outPoint(outPoint), nHeight(nHeight) {}
   
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
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
    : name(name), outPoint(outPoint) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(name);
        READWRITE(outPoint);
    }
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

struct claimsForNameType
{
    std::vector<CClaimValue> claims;
    std::vector<CSupportValue> supports;
    int nLastTakeoverHeight;

    claimsForNameType(std::vector<CClaimValue> claims, std::vector<CSupportValue> supports, int nLastTakeoverHeight)
    : claims(claims), supports(supports), nLastTakeoverHeight(nLastTakeoverHeight) {}
    
    //return effective amount form claim, retuns 0 if claim is not found
    CAmount getEffectiveAmountForClaim(uint160 claimId, int currentHeight)
    {
        CAmount effectiveAmount = 0;
        bool claim_found = false; 
        for (std::vector<CClaimValue>::iterator it=claims.begin(); it!=claims.end(); ++it)
        {
            if (it->claimId == claimId && it->nValidAtHeight < currentHeight)
                effectiveAmount += it->nAmount;       
                claim_found = true;  
                break;
        }
        if (!claim_found)
            return effectiveAmount; 
        
        for (std::vector<CSupportValue>::iterator it=supports.begin(); it!=supports.end(); ++it)
        {
            if (it->supportedClaimId == claimId && it->nValidAtHeight < currentHeight)
                effectiveAmount += it->nAmount; 
        }
        return effectiveAmount; 

    }
};

class CClaimTrieCache;

class CClaimTrie
{
public:
    CClaimTrie(bool fMemory = false, bool fWipe = false, int nProportionalDelayFactor = 32)
               : db(GetDataDir() / "claimtrie", 100, fMemory, fWipe, false)
               , nCurrentHeight(0), nExpirationTime(262974)
               , nProportionalDelayFactor(nProportionalDelayFactor)
               , root(uint256S("0000000000000000000000000000000000000000000000000000000000000001"))
    {}
    
    uint256 getMerkleHash();
    
    bool empty() const;
    void clear();
    
    bool checkConsistency() const;
    
    bool WriteToDisk();
    bool ReadFromDisk(bool check = false);
    
    std::vector<namedNodeType> flattenTrie() const;
    bool getInfoForName(const std::string& name, CClaimValue& claim) const;
    bool getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const;

    claimsForNameType getClaimsForName(const std::string& name) const;
    
    bool queueEmpty() const;
    bool supportEmpty() const;
    bool supportQueueEmpty() const;
    bool expirationQueueEmpty() const;
    bool supportExpirationQueueEmpty() const;
    
    void setExpirationTime(int t);
    
    bool getQueueRow(int nHeight, claimQueueRowType& row) const;
    bool getQueueNameRow(const std::string& name, queueNameRowType& row) const;
    bool getExpirationQueueRow(int nHeight, expirationQueueRowType& row) const;
    bool getSupportNode(std::string name, supportMapEntryType& node) const;
    bool getSupportQueueRow(int nHeight, supportQueueRowType& row) const;
    bool getSupportQueueNameRow(const std::string& name, queueNameRowType& row) const;
    bool getSupportExpirationQueueRow(int nHeight, expirationQueueRowType& row) const;
    
    bool haveClaim(const std::string& name, const COutPoint& outPoint) const;
    bool haveClaimInQueue(const std::string& name, const COutPoint& outPoint,
                          int& nValidAtHeight) const;
    
    bool haveSupport(const std::string& name, const COutPoint& outPoint) const;
    bool haveSupportInQueue(const std::string& name, const COutPoint& outPoint,
                            int& nValidAtHeight) const;
    
    unsigned int getTotalNamesInTrie() const;
    unsigned int getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;

    friend class CClaimTrieCache;
    
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
    bool recursiveNullify(CClaimTrieNode* node, std::string& name);
    
    bool recursiveCheckConsistency(const CClaimTrieNode* node) const;
    
    bool InsertFromDisk(const std::string& name, CClaimTrieNode* node);
    
    unsigned int getTotalNamesRecursive(const CClaimTrieNode* current) const;
    unsigned int getTotalClaimsRecursive(const CClaimTrieNode* current) const;
    CAmount getTotalValueOfClaimsRecursive(const CClaimTrieNode* current,
                                           bool fControllingOnly) const;
    bool recursiveFlattenTrie(const std::string& name,
                              const CClaimTrieNode* current,
                              std::vector<namedNodeType>& nodes) const;
    
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
    void BatchEraseNode(CDBBatch& batch, const std::string& nome) const;
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
        : children(children), hasValue(hasValue), valHash(valHash)
        {};
    std::vector<std::pair<unsigned char, uint256> > children;
    bool hasValue;
    uint256 valHash;
};

class CClaimTrieProof
{
public:
    CClaimTrieProof() {};
    CClaimTrieProof(std::vector<CClaimTrieProofNode> nodes, bool hasValue, COutPoint outPoint, int nHeightOfLastTakeover) : nodes(nodes), hasValue(hasValue), outPoint(outPoint), nHeightOfLastTakeover(nHeightOfLastTakeover) {}
    std::vector<CClaimTrieProofNode> nodes;
    bool hasValue;
    COutPoint outPoint;
    int nHeightOfLastTakeover;
};

class CClaimTrieCache
{
public:
    CClaimTrieCache(CClaimTrie* base, bool fRequireTakeoverHeights = true)
                    : base(base),
                      fRequireTakeoverHeights(fRequireTakeoverHeights)
    {
        assert(base);
        nCurrentHeight = base->nCurrentHeight;
    }
    
    uint256 getMerkleHash() const;
    
    bool empty() const;
    bool flush();
    bool dirty() const { return !dirtyHashes.empty(); }
    
    bool addClaim(const std::string& name, const COutPoint& outPoint,
                  uint160 claimId, CAmount nAmount, int nHeight) const;
    bool undoAddClaim(const std::string& name, const COutPoint& outPoint,
                      int nHeight) const;
    bool spendClaim(const std::string& name, const COutPoint& outPoint,
                    int nHeight, int& nValidAtHeight) const;
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

    bool incrementBlock(insertUndoType& insertUndo,
                        claimQueueRowType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportQueueRowType& expireSupportUndo,
                        std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const;
    bool decrementBlock(insertUndoType& insertUndo,
                        claimQueueRowType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportQueueRowType& expireSupportUndo,
                        std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const;
    
    ~CClaimTrieCache() { clear(); }
    
    bool insertClaimIntoTrie(const std::string& name, CClaimValue claim,
                             bool fCheckTakeover = false) const;
    bool removeClaimFromTrie(const std::string& name, const COutPoint& outPoint,
                             CClaimValue& claim,
                             bool fCheckTakeover = false) const;
    CClaimTrieProof getProofForName(const std::string& name) const;

    bool finalizeDecrement() const;
private:
    CClaimTrie* base;

    bool fRequireTakeoverHeights;

    mutable nodeCacheType cache;
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
    mutable int nCurrentHeight; // Height of the block that is being worked on, which is
                                // one greater than the height of the chain's tip
    
    uint256 hashBlock;
    
    uint256 computeHash() const;
    
    bool reorderTrieNode(const std::string& name, bool fCheckTakeover) const;
    bool recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent,
                                    std::string sPos) const;
    bool recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos,
                            std::string sName,
                            bool* pfNullified = NULL) const;
    
    bool clear() const;
    
    bool removeClaim(const std::string& name, const COutPoint& outPoint,
                     int nHeight, int& nValidAtHeight, bool fCheckTakeover) const;
    
    bool addClaimToQueues(const std::string& name, CClaimValue& claim) const;
    bool removeClaimFromQueue(const std::string& name, const COutPoint& outPoint,
                              CClaimValue& claim) const;
    void addToExpirationQueue(int nExpirationHeight, nameOutPointType& entry) const;
    void removeFromExpirationQueue(const std::string& name, const COutPoint& outPoint,
                                   int nHeight) const;
    
    claimQueueType::iterator getQueueCacheRow(int nHeight,
                                              bool createIfNotExists) const;
    queueNameType::iterator getQueueCacheNameRow(const std::string& name,
                                                 bool createIfNotExists) const;
    expirationQueueType::iterator getExpirationQueueCacheRow(int nHeight,
                                                             bool createIfNotExists) const;
    
    bool removeSupport(const std::string& name, const COutPoint& outPoint,
                       int nHeight, int& nValidAtHeight,
                       bool fCheckTakeover) const;
    bool removeSupportFromMap(const std::string& name, const COutPoint& outPoint,
                              CSupportValue& support,
                              bool fCheckTakeover) const;
    
    bool insertSupportIntoMap(const std::string& name,
                              CSupportValue support,
                              bool fCheckTakeover) const;
    
    supportQueueType::iterator getSupportQueueCacheRow(int nHeight,
                                                       bool createIfNotExists) const;
    queueNameType::iterator getSupportQueueCacheNameRow(const std::string& name,
                                                                 bool createIfNotExists) const;
    expirationQueueType::iterator getSupportExpirationQueueCacheRow(int nHeight,
                                                                     bool createIfNotExists) const;

    bool addSupportToQueues(const std::string& name, CSupportValue& support) const;
    bool removeSupportFromQueue(const std::string& name, const COutPoint& outPoint,
                                CSupportValue& support) const;

    void addSupportToExpirationQueue(int nExpirationHeight,
                                     nameOutPointType& entry) const;
    void removeSupportFromExpirationQueue(const std::string& name,
                                          const COutPoint& outPoint,
                                          int nHeight) const;
    
    bool getSupportsForName(const std::string& name,
                            supportMapEntryType& node) const;

    bool getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const;
    
    int getDelayForName(const std::string& name) const;

    uint256 getLeafHashForProof(const std::string& currentPosition, unsigned char nodeChar,
                                const CClaimTrieNode* currentNode) const;

    CClaimTrieNode* addNodeToCache(const std::string& position, CClaimTrieNode* original) const;

    bool getOriginalInfoForName(const std::string& name, CClaimValue& claim) const;

    int getNumBlocksOfContinuousOwnership(const std::string& name) const;
};

#endif // BITCOIN_CLAIMTRIE_H
