#ifndef BITCOIN_ClaimTRIE_H
#define BITCOIN_ClaimTRIE_H

#include "amount.h"
#include "serialize.h"
#include "coins.h"
#include "hash.h"
#include "uint256.h"
#include "util.h"
#include "leveldbwrapper.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <vector>

#define DEFAULT_DELAY 100

// leveldb keys
#define HASH_BLOCK 'h'
#define CURRENT_HEIGHT 't'
#define TRIE_NODE 'n'
#define QUEUE_ROW 'r'
#define EXP_QUEUE_ROW 'e'
#define SUPPORT 's'
#define SUPPORT_QUEUE 'u' 

class CNodeValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    CAmount nAmount;
    CAmount nEffectiveAmount;
    int nHeight;
    int nValidAtHeight;
    CNodeValue() {};
    CNodeValue(uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) : txhash(txhash), nOut(nOut), nAmount(nAmount), nEffectiveAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight) {}
    uint256 GetHash() const;
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(nOut);
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(nValidAtHeight);
    }
    
    bool operator<(const CNodeValue& other) const
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
    bool operator==(const CNodeValue& other) const
    {
        return txhash == other.txhash && nOut == other.nOut && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
    }
    bool operator!=(const CNodeValue& other) const
    {
        return !(*this == other);
    }
};

class CSupportNodeValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    uint256 supportTxhash;
    int supportnOut;
    CAmount nAmount;
    int nHeight;
    int nValidAtHeight;
    CSupportNodeValue() {};
    CSupportNodeValue(uint256 txhash, uint32_t nOut, uint256 supportTxhash, uint32_t supportnOut, CAmount nAmount, int nHeight, int nValidAtHeight) : txhash(txhash), nOut(nOut), supportTxhash(supportTxhash), supportnOut(supportnOut), nAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight) {}
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(nOut);
        READWRITE(supportTxhash);
        READWRITE(supportnOut);
        READWRITE(nAmount);
        READWRITE(nHeight);
        READWRITE(nValidAtHeight);
    }

    bool operator==(const CSupportNodeValue& other) const
    {
        return txhash == other.txhash && nOut == other.nOut && supportTxhash == other.supportTxhash && supportnOut == other.supportnOut && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
    }
    bool operator!=(const CSupportNodeValue& other) const
    {
        return !(*this == other);
    }
};

class CClaimTrieNode;
class CClaimTrie;

typedef std::vector<CSupportNodeValue> supportMapNodeType;

typedef std::map<unsigned char, CClaimTrieNode*> nodeMapType;

typedef std::pair<std::string, CClaimTrieNode> namedNodeType;

class CClaimTrieNode
{
public:
    CClaimTrieNode() {}
    CClaimTrieNode(uint256 hash) : hash(hash) {}
    uint256 hash;
    nodeMapType children;
    std::vector<CNodeValue> values;
    bool insertValue(CNodeValue val);//, bool * fChanged = NULL);
    bool removeValue(uint256& txhash, uint32_t nOut, CNodeValue& val);//, bool * fChanged = NULL);
    bool getBestValue(CNodeValue& val) const;
    bool empty() const {return children.empty() && values.empty();}
    bool haveValue(const uint256& txhash, uint32_t nOut) const;
    void reorderValues(supportMapNodeType& supports);
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(values);
    }
    
    bool operator==(const CClaimTrieNode& other) const
    {
        return hash == other.hash && values == other.values;
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

class CValueQueueEntry
{
public:
    CValueQueueEntry() {}
    CValueQueueEntry(std::string name, CNodeValue val) : name(name), val(val) {}
    std::string name;
    CNodeValue val;
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(name);
        READWRITE(val);
    }
};

class CSupportValueQueueEntry
{
public:
    CSupportValueQueueEntry() {}
    CSupportValueQueueEntry(std::string name, CSupportNodeValue val) : name(name), val(val) {}
    std::string name;
    CSupportNodeValue val;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action, int nType, int nVersion) {
        READWRITE(name);
        READWRITE(val);
    }
};

typedef std::map<std::string, supportMapNodeType> supportMapType;

typedef std::map<int, std::vector<CValueQueueEntry> > valueQueueType;
typedef std::vector<CValueQueueEntry> CClaimTrieQueueUndo;

typedef std::map<int, std::vector<CSupportValueQueueEntry> > supportValueQueueType;
typedef std::vector<CSupportValueQueueEntry> CSupportValueQueueUndo;

typedef std::map<std::string, CClaimTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

class CClaimTrieCache;

class CClaimTrie
{
public:
    CClaimTrie(bool fMemory = false, bool fWipe = false) : db(GetDataDir() / "claimtrie", 100, fMemory, fWipe), nCurrentHeight(0), nExpirationTime(262974), root(uint256S("0000000000000000000000000000000000000000000000000000000000000001")) {}
    uint256 getMerkleHash();
    CLevelDBWrapper db;
    bool empty() const;
    void clear();
    bool checkConsistency();
    bool WriteToDisk();
    bool ReadFromDisk(bool check = false);
    std::vector<namedNodeType> flattenTrie() const;
    bool getInfoForName(const std::string& name, CNodeValue& val) const;
    int nCurrentHeight;
    bool queueEmpty() const;
    bool supportEmpty() const;
    bool supportQueueEmpty() const;
    bool expirationQueueEmpty() const;
    void setExpirationTime(int t);
    bool getQueueRow(int nHeight, std::vector<CValueQueueEntry>& row);
    bool getExpirationQueueRow(int nHeight, std::vector<CValueQueueEntry>& row);
    bool getSupportNode(std::string name, supportMapNodeType& node);
    bool getSupportQueueRow(int nHeight, std::vector<CSupportValueQueueEntry>& row);
    bool haveClaim(const std::string& name, const uint256& txhash, uint32_t nOut) const;
    unsigned int getTotalNamesInTrie() const;
    unsigned int getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;
    friend class CClaimTrieCache;
    int nExpirationTime;
private:
    void clear(CClaimTrieNode* current);
    bool update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlock, valueQueueType& queueCache, valueQueueType& expirationQueueCache, int nNewHeight, supportMapType& supportCache, supportValueQueueType& supportQueueCache);
    bool updateName(const std::string& name, CClaimTrieNode* updatedNode);
    bool updateHash(const std::string& name, uint256& hash);
    bool recursiveNullify(CClaimTrieNode* node, std::string& name);
    bool recursiveCheckConsistency(CClaimTrieNode* node);
    bool InsertFromDisk(const std::string& name, CClaimTrieNode* node);
    unsigned int getTotalNamesRecursive(const CClaimTrieNode* current) const;
    unsigned int getTotalClaimsRecursive(const CClaimTrieNode* current) const;
    CAmount getTotalValueOfClaimsRecursive(const CClaimTrieNode* current, bool fControllingOnly) const;
    bool recursiveFlattenTrie(const std::string& name, const CClaimTrieNode* current, std::vector<namedNodeType>& nodes) const;
    CClaimTrieNode root;
    //supportMapType support;
    uint256 hashBlock;
    valueQueueType dirtyQueueRows;
    valueQueueType dirtyExpirationQueueRows;
    supportValueQueueType dirtySupportQueueRows;
    
    nodeCacheType dirtyNodes;
    supportMapType dirtySupportNodes;
    void markNodeDirty(const std::string& name, CClaimTrieNode* node);
    void updateQueueRow(int nHeight, std::vector<CValueQueueEntry>& row);
    void updateExpirationRow(int nHeight, std::vector<CValueQueueEntry>& row);
    void updateSupportMap(const std::string& name, supportMapNodeType& node);
    void updateSupportQueue(int nHeight, std::vector<CSupportValueQueueEntry>& row);
    void BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CClaimTrieNode* pNode) const;
    void BatchEraseNode(CLevelDBBatch& batch, const std::string& nome) const;
    void BatchWriteQueueRows(CLevelDBBatch& batch);
    void BatchWriteExpirationQueueRows(CLevelDBBatch& batch);
    void BatchWriteSupportNodes(CLevelDBBatch& batch);
    void BatchWriteSupportQueueRows(CLevelDBBatch& batch);
    bool keyTypeEmpty(char key) const;
};

class CClaimTrieCache
{
public:
    CClaimTrieCache(CClaimTrie* base): base(base) {assert(base); nCurrentHeight = base->nCurrentHeight;}
    uint256 getMerkleHash() const;
    bool empty() const;
    bool flush();
    bool dirty() const { return !dirtyHashes.empty(); }
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) const;
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, uint256 prevTxhash, uint32_t nPrevOut) const;
    bool undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const;
    bool spendClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const;
    bool addSupport(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, int supportednOut, int nHeight) const;
    bool undoAddSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, int supportednOut, int nHeight) const;
    bool spendSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, int supportednOut, int nHeight, int& nValidAtHeight) const;
    bool undoSpendSupport(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, int supportednOut, int nHeight, int nValidAtHeight) const;
    uint256 getBestBlock();
    void setBestBlock(const uint256& hashBlock);
    bool incrementBlock(CClaimTrieQueueUndo& insertUndo, CClaimTrieQueueUndo& expireUndo, CSupportValueQueueUndo& insertSupportUndo) const;
    bool decrementBlock(CClaimTrieQueueUndo& insertUndo, CClaimTrieQueueUndo& expireUndo, CSupportValueQueueUndo& insertSupportUndo) const;
    ~CClaimTrieCache() { clear(); }
    bool insertClaimIntoTrie(const std::string name, CNodeValue val) const;
    bool removeClaimFromTrie(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight) const;
private:
    CClaimTrie* base;
    mutable nodeCacheType cache;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    mutable valueQueueType valueQueueCache;
    mutable valueQueueType expirationQueueCache;
    mutable supportMapType supportCache;
    mutable supportValueQueueType supportQueueCache; 
    mutable int nCurrentHeight; // Height of the block that is being worked on, which is
                                // one greater than the height of the chain's tip
    uint256 computeHash() const;
    bool reorderTrieNode(const std::string name) const;
    bool recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent, std::string sPos) const;
    bool recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified = NULL) const;
    bool clear() const;
    bool removeClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool addClaimToQueues(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const;
    bool removeClaimFromQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeightToCheck, int& nValidAtHeight) const;
    void addToExpirationQueue(CValueQueueEntry& entry) const;
    void removeFromExpirationQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const;
    valueQueueType::iterator getQueueCacheRow(int nHeight, bool createIfNotExists) const;
    valueQueueType::iterator getExpirationQueueCacheRow(int nHeight, bool createIfNotExists) const;
    bool removeSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, int supportednOut, int nHeight, int& nValidAtHeight) const;
    bool removeSupportFromMap(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, int supportednOut, int nHeight, int& nValidAtHeight) const;
    bool insertSupportIntoMap(const std::string name, CSupportNodeValue val) const;
    supportValueQueueType::iterator getSupportQueueCacheRow(int nHeight, bool createIfNotExists) const;
    bool addSupportToQueue(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, int supportednOut, int nHeight, int nValidAtHeight) const;
    bool removeSupportFromQueue(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, int supportednOut, int nHeightToCheck, int& nValidAtHeight) const;
    uint256 hashBlock;
    bool getSupportsForName(const std::string name, supportMapNodeType& node) const;
};

#endif // BITCOIN_ClaimTRIE_H
