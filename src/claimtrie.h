#ifndef BITCOIN_CLAIMTRIE_H
#define BITCOIN_CLAIMTRIE_H

#include "amount.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "leveldbwrapper.h"

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

class CClaimValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    uint160 claimId;
    CAmount nAmount;
    CAmount nEffectiveAmount;
    int nHeight;
    int nValidAtHeight;

    CClaimValue() {};

    CClaimValue(uint256 txhash, uint32_t nOut, uint160 claimId, CAmount nAmount, int nHeight,
                int nValidAtHeight)
                : txhash(txhash), nOut(nOut), claimId(claimId)
                , nAmount(nAmount), nEffectiveAmount(nAmount)
                , nHeight(nHeight), nValidAtHeight(nValidAtHeight)
    {}
    
    uint256 GetHash() const;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(nOut);
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
                if (txhash.GetHex() > other.txhash.GetHex())
                    return true;
                else if (txhash == other.txhash)
                    if (nOut > other.nOut)
                        return true;
            }
        }
        return false;
    }
    
    bool operator==(const CClaimValue& other) const
    {
        return txhash == other.txhash && nOut == other.nOut && claimId == other.claimId && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
    }
    
    bool operator!=(const CClaimValue& other) const
    {
        return !(*this == other);
    }
};

class CSupportValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    uint160 supportedClaimId;
    CAmount nAmount;
    int nHeight;
    int nValidAtHeight;
    
    CSupportValue() {};
    CSupportValue(uint256 txhash, uint32_t nOut, uint160 supportedClaimId,
                  CAmount nAmount, int nHeight, int nValidAtHeight)
                  : txhash(txhash), nOut(nOut)
                  , supportedClaimId(supportedClaimId), nAmount(nAmount)
                  , nHeight(nHeight), nValidAtHeight(nValidAtHeight)
    {}
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(nOut);
        READWRITE(supportedClaimId);
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(nValidAtHeight);
    }

    bool operator==(const CSupportValue& other) const
    {
        return txhash == other.txhash && nOut == other.nOut && supportedClaimId == other.supportedClaimId && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
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
    bool removeClaim(uint256& txhash, uint32_t nOut, CClaimValue& claim);
    bool getBestClaim(CClaimValue& claim) const;
    bool empty() const {return children.empty() && claims.empty();}
    bool haveClaim(const uint256& txhash, uint32_t nOut) const;
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

typedef std::pair<std::string, CClaimValue> claimQueueEntryType;

typedef std::pair<std::string, CSupportValue> supportQueueEntryType;

typedef std::map<std::string, supportMapEntryType> supportMapType;

typedef std::vector<claimQueueEntryType> claimQueueRowType;
typedef std::map<int, claimQueueRowType> claimQueueType;
typedef std::map<std::string, std::vector<CClaimValue> > claimQueueNamesType;

typedef std::vector<supportQueueEntryType> supportQueueRowType;
typedef std::map<int, supportQueueRowType> supportQueueType;
typedef std::map<std::string, std::vector<CSupportValue> > supportQueueNamesType;

typedef std::map<std::string, CClaimTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

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
    
    bool queueEmpty() const;
    bool supportEmpty() const;
    bool supportQueueEmpty() const;
    bool expirationQueueEmpty() const;
    
    void setExpirationTime(int t);
    
    bool getQueueRow(int nHeight, claimQueueRowType& row) const;
    bool getQueueNameRow(const std::string& name, std::vector<CClaimValue>& row) const;
    bool getExpirationQueueRow(int nHeight, claimQueueRowType& row) const;
    bool getSupportNode(std::string name, supportMapEntryType& node) const;
    bool getSupportQueueRow(int nHeight, supportQueueRowType& row) const;
    bool getSupportQueueNameRow(const std::string& name, std::vector<CSupportValue>& row) const;
    
    bool haveClaim(const std::string& name, const uint256& txhash,
                   uint32_t nOut) const;
    bool haveClaimInQueue(const std::string& name, const uint256& txhash,
                          uint32_t nOut, int& nValidAtHeight) const;
    
    bool haveSupport(const std::string& name, const uint256& txhash,
                     uint32_t nOut) const;
    bool haveSupportInQueue(const std::string& name, const uint256& txhash,
                            uint32_t nOut, int& nValidAtHeight) const;
    
    unsigned int getTotalNamesInTrie() const;
    unsigned int getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;
    
    friend class CClaimTrieCache;
    
    CLevelDBWrapper db;
    int nCurrentHeight;
    int nExpirationTime;
    int nProportionalDelayFactor;
private:
    void clear(CClaimTrieNode* current);

    const CClaimTrieNode* getNodeForName(const std::string& name) const;
    
    bool update(nodeCacheType& cache, hashMapType& hashes,
                std::map<std::string, int>& takeoverHeights,
                const uint256& hashBlock, claimQueueType& queueCache,
                claimQueueNamesType& queueNameCache,
                claimQueueType& expirationQueueCache, int nNewHeight,
                supportMapType& supportCache,
                supportQueueType& supportQueueCache,
                supportQueueNamesType& supportQueueNameCache);
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
                            std::vector<CClaimValue>& row); 
    void updateExpirationRow(int nHeight, claimQueueRowType& row);
    void updateSupportMap(const std::string& name, supportMapEntryType& node);
    void updateSupportQueue(int nHeight, supportQueueRowType& row);
    void updateSupportNameQueue(const std::string& name,
                                std::vector<CSupportValue>& row);
    
    void BatchWriteNode(CLevelDBBatch& batch, const std::string& name,
                        const CClaimTrieNode* pNode) const;
    void BatchEraseNode(CLevelDBBatch& batch, const std::string& nome) const;
    void BatchWriteQueueRows(CLevelDBBatch& batch);
    void BatchWriteQueueNameRows(CLevelDBBatch& batch);
    void BatchWriteExpirationQueueRows(CLevelDBBatch& batch);
    void BatchWriteSupportNodes(CLevelDBBatch& batch);
    void BatchWriteSupportQueueRows(CLevelDBBatch& batch);
    void BatchWriteSupportQueueNameRows(CLevelDBBatch& batch);
    template<typename K> bool keyTypeEmpty(char key, K& dummy) const;
    
    CClaimTrieNode root;
    uint256 hashBlock;
    
    claimQueueType dirtyQueueRows;
    claimQueueNamesType dirtyQueueNameRows;
    claimQueueType dirtyExpirationQueueRows;
    
    supportQueueType dirtySupportQueueRows;
    supportQueueNamesType dirtySupportQueueNameRows;
    
    nodeCacheType dirtyNodes;
    supportMapType dirtySupportNodes;
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
    
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut,
                  uint160 claimId, CAmount nAmount, int nHeight) const;
    bool undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut,
                      int nHeight) const;
    bool spendClaim(const std::string name, uint256 txhash, uint32_t nOut,
                    int nHeight, int& nValidAtHeight) const;
    bool undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut,
                        uint160 claimId, CAmount nAmount, int nHeight,
                        int nValidAtHeight) const;
    
    bool addSupport(const std::string name, uint256 txhash, uint32_t nOut,
                    CAmount nAmount, uint160 supportedClaimId,
                    int nHeight) const;
    bool undoAddSupport(const std::string name, uint256 txhash, uint32_t nOut,
                        int nHeight) const;
    bool spendSupport(const std::string name, uint256 txhash, uint32_t nOut,
                      int nHeight, int& nValidAtHeight) const;
    bool undoSpendSupport(const std::string name, uint256 txhash,
                          uint32_t nOut, uint160 supportedClaimId,
                          CAmount nAmount, int nHeight, int nValidAtHeight) const;
    
    uint256 getBestBlock();
    void setBestBlock(const uint256& hashBlock);

    bool incrementBlock(claimQueueRowType& insertUndo,
                        claimQueueRowType& expireUndo,
                        supportQueueRowType& insertSupportUndo,
                        std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const;
    bool decrementBlock(claimQueueRowType& insertUndo,
                        claimQueueRowType& expireUndo,
                        supportQueueRowType& insertSupportUndo,
                        std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const;
    
    ~CClaimTrieCache() { clear(); }
    
    bool insertClaimIntoTrie(const std::string name, CClaimValue claim,
                             bool fCheckTakeover = false) const;
    bool removeClaimFromTrie(const std::string name, uint256 txhash,
                             uint32_t nOut, int& nValidAtHeight,
                             bool fCheckTakeover = false) const;
private:
    CClaimTrie* base;

    bool fRequireTakeoverHeights;

    mutable nodeCacheType cache;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    mutable claimQueueType claimQueueCache;
    mutable claimQueueNamesType claimQueueNameCache;
    mutable claimQueueType expirationQueueCache;
    mutable supportMapType supportCache;
    mutable supportQueueType supportQueueCache;
    mutable supportQueueNamesType supportQueueNameCache;
    mutable std::set<std::string> namesToCheckForTakeover;
    mutable std::map<std::string, int> cacheTakeoverHeights; 
    mutable int nCurrentHeight; // Height of the block that is being worked on, which is
                                // one greater than the height of the chain's tip
    
    uint256 hashBlock;
    
    uint256 computeHash() const;
    
    bool reorderTrieNode(const std::string name, bool fCheckTakeover) const;
    bool recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent,
                                    std::string sPos) const;
    bool recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos,
                            std::string sName,
                            bool* pfNullified = NULL) const;
    
    bool clear() const;
    
    bool removeClaim(const std::string name, uint256 txhash, uint32_t nOut,
                     int nHeight, int& nValidAtHeight, bool fCheckTakeover) const;
    
    bool addClaimToQueues(const std::string name, CClaimValue& claim) const;
    bool removeClaimFromQueue(const std::string name, uint256 txhash,
                              uint32_t nOut, int& nValidAtHeight) const;
    void addToExpirationQueue(claimQueueEntryType& entry) const;
    void removeFromExpirationQueue(const std::string name, uint256 txhash,
                                   uint32_t nOut, int nHeight) const;
    
    claimQueueType::iterator getQueueCacheRow(int nHeight,
                                              bool createIfNotExists) const;
    claimQueueNamesType::iterator getQueueCacheNameRow(const std::string& name,
                                                       bool createIfNotExists) const;
    claimQueueType::iterator getExpirationQueueCacheRow(int nHeight,
                                                        bool createIfNotExists) const;
    
    bool removeSupport(const std::string name, uint256 txhash, uint32_t nOut,
                       int nHeight, int& nValidAtHeight,
                       bool fCheckTakeover) const;
    bool removeSupportFromMap(const std::string name, uint256 txhash,
                              uint32_t nOut, int nHeight, int& nValidAtHeight,
                              bool fCheckTakeover) const;
    
    bool insertSupportIntoMap(const std::string name,
                              CSupportValue support,
                              bool fCheckTakeover) const;
    
    supportQueueType::iterator getSupportQueueCacheRow(int nHeight,
                                                       bool createIfNotExists) const;
    supportQueueNamesType::iterator getSupportQueueCacheNameRow(const std::string& name,
                                                                 bool createIfNotExists) const;
    
    bool addSupportToQueue(const std::string name, uint256 txhash,
                           uint32_t nOut, CAmount nAmount,
                           uint160 supportedClaimId,
                           int nHeight, int nValidAtHeight) const;
    bool removeSupportFromQueue(const std::string name, uint256 txhash,
                                uint32_t nOut, int& nValidAtHeight) const;
    
    bool getSupportsForName(const std::string name,
                            supportMapEntryType& node) const;

    bool getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const;
    
    int getDelayForName(const std::string& name) const;
};

#endif // BITCOIN_CLAIMTRIE_H
