#ifndef BITCOIN_NCCTRIE_H
#define BITCOIN_NCCTRIE_H

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

uint256 getOutPointHash(uint256 txhash, uint32_t nOut);

class CNodeValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    CAmount nAmount;
    int nHeight;
    int nValidAtHeight;
    CNodeValue() {};
    CNodeValue(uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) : txhash(txhash), nOut(nOut), nAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight) {}
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
        if (nAmount < other.nAmount)
            return true;
        else if (nAmount == other.nAmount)
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

class CNCCTrieNode;
class CNCCTrie;

typedef std::map<unsigned char, CNCCTrieNode*> nodeMapType;

typedef std::pair<std::string, CNCCTrieNode> namedNodeType;

class CNCCTrieNode
{
public:
    CNCCTrieNode() {}
    CNCCTrieNode(uint256 hash) : hash(hash) {}
    uint256 hash;
    uint256 bestBlock;
    nodeMapType children;
    std::vector<CNodeValue> values;
    bool insertValue(CNodeValue val, bool * fChanged = NULL);
    bool removeValue(uint256& txhash, uint32_t nOut, CNodeValue& val, bool * fChanged = NULL);
    bool getBestValue(CNodeValue& val) const;
    bool empty() const {return children.empty() && values.empty();}
    bool haveValue(const uint256& txhash, uint32_t nOut) const;
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(values);
    }
    
    bool operator==(const CNCCTrieNode& other) const
    {
        return hash == other.hash && values == other.values;
    }

    bool operator!=(const CNCCTrieNode& other) const
    {
        return !(*this == other);
    }
private:
    bool getValue(uint256& txhash, uint32_t nOut, CNodeValue& val) const; 
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

typedef std::map<int, std::vector<CValueQueueEntry> > valueQueueType;
typedef std::vector<CValueQueueEntry> CNCCTrieQueueUndo;

typedef std::map<std::string, CNCCTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

class CNCCTrieCache;

class CNCCTrie
{
public:
    CNCCTrie(bool fMemory = false, bool fWipe = false) : db(GetDataDir() / "ncctrie", 100, fMemory, fWipe), nCurrentHeight(0), nExpirationTime(262974), root(uint256S("0000000000000000000000000000000000000000000000000000000000000001")) {}
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
    bool expirationQueueEmpty() const;
    void setExpirationTime(int t);
    bool getQueueRow(int nHeight, std::vector<CValueQueueEntry>& row) const;
    bool getExpirationQueueRow(int nHeight, std::vector<CValueQueueEntry>& row);
    bool haveClaim(const std::string& name, const uint256& txhash, uint32_t nOut) const;
    bool haveClaimInQueue(const std::string& name, const uint256& txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool haveClaimInQueueRow(const std::string& name, const uint256& txhash, uint32_t nOut, int nHeight, const std::vector<CValueQueueEntry>& row) const;
    unsigned int getTotalNamesInTrie() const;
    unsigned int getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;
    friend class CNCCTrieCache;
    int nExpirationTime;
private:
    void clear(CNCCTrieNode* current);
    bool update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlock, valueQueueType& queueCache, valueQueueType& expirationQueueCache, int nNewHeight);
    bool updateName(const std::string& name, CNCCTrieNode* updatedNode);
    bool updateHash(const std::string& name, uint256& hash);
    bool recursiveNullify(CNCCTrieNode* node, std::string& name);
    bool recursiveCheckConsistency(CNCCTrieNode* node);
    bool InsertFromDisk(const std::string& name, CNCCTrieNode* node);
    unsigned int getTotalNamesRecursive(const CNCCTrieNode* current) const;
    unsigned int getTotalClaimsRecursive(const CNCCTrieNode* current) const;
    CAmount getTotalValueOfClaimsRecursive(const CNCCTrieNode* current, bool fControllingOnly) const;
    bool recursiveFlattenTrie(const std::string& name, const CNCCTrieNode* current, std::vector<namedNodeType>& nodes) const;
    CNCCTrieNode root;
    uint256 hashBlock;
    valueQueueType dirtyQueueRows;
    valueQueueType dirtyExpirationQueueRows;
    
    nodeCacheType dirtyNodes;
    void markNodeDirty(const std::string& name, CNCCTrieNode* node);
    void updateQueueRow(int nHeight, std::vector<CValueQueueEntry>& row);
    void updateExpirationRow(int nHeight, std::vector<CValueQueueEntry>& row);
    void BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CNCCTrieNode* pNode) const;
    void BatchEraseNode(CLevelDBBatch& batch, const std::string& nome) const;
    void BatchWriteQueueRows(CLevelDBBatch& batch);
    void BatchWriteExpirationQueueRows(CLevelDBBatch& batch);
};

class CNCCTrieProofNode
{
public:
    CNCCTrieProofNode() {};
    CNCCTrieProofNode(std::vector<unsigned char> children, bool hasValue, uint256 valHash, uint256 nodeHash) : children(children), hasValue(hasValue), valHash(valHash), nodeHash(nodeHash) {};
    std::vector<unsigned char> children;
    bool hasValue;
    uint256 valHash;
    uint256 nodeHash;
};

class CNCCTrieProofLeafNode
{
public:
    CNCCTrieProofLeafNode() {};
    CNCCTrieProofLeafNode(uint256 nodeHash) : nodeHash(nodeHash) {};
    uint256 nodeHash;
};

class CNCCTrieProof
{
public:
    CNCCTrieProof() {};
    CNCCTrieProof(std::map<std::string, CNCCTrieProofNode> nodes, std::map<std::string, CNCCTrieProofLeafNode> leafNodes, bool hasValue, uint256 txhash, uint32_t nOut) : nodes(nodes), leafNodes(leafNodes), hasValue(hasValue), txhash(txhash), nOut(nOut) {}
    std::map<std::string, CNCCTrieProofNode> nodes;
    std::map<std::string, CNCCTrieProofLeafNode> leafNodes;
    bool hasValue;
    uint256 txhash;
    uint32_t nOut;
};

class CNCCTrieCache
{
public:
    CNCCTrieCache(CNCCTrie* base): base(base) {assert(base); nCurrentHeight = base->nCurrentHeight;}
    uint256 getMerkleHash() const;
    bool empty() const;
    bool flush();
    bool dirty() const { return !dirtyHashes.empty(); }
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) const;
    bool addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, uint256 prevTxhash, uint32_t nPrevOut) const;
    bool undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const;
    bool spendClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const;
    uint256 getBestBlock();
    void setBestBlock(const uint256& hashBlock);
    bool incrementBlock(CNCCTrieQueueUndo& insertUndo, CNCCTrieQueueUndo& expireUndo) const;
    bool decrementBlock(CNCCTrieQueueUndo& insertUndo, CNCCTrieQueueUndo& expireUndo) const;
    ~CNCCTrieCache() { clear(); }
    bool insertClaimIntoTrie(const std::string name, CNodeValue val) const;
    bool removeClaimFromTrie(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight) const;
    CNCCTrieProof getProofForName(const std::string& name) const;
private:
    CNCCTrie* base;
    mutable nodeCacheType cache;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    mutable valueQueueType valueQueueCache;
    mutable valueQueueType expirationQueueCache;
    mutable int nCurrentHeight; // Height of the block that is being worked on, which is
                                // one greater than the height of the chain's tip
    uint256 computeHash() const;
    bool recursiveComputeMerkleHash(CNCCTrieNode* tnCurrent, std::string sPos) const;
    bool recursivePruneName(CNCCTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified = NULL) const;
    bool clear() const;
    bool removeClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const;
    bool addClaimToQueues(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const;
    bool removeClaimFromQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeightToCheck, int& nValidAtHeight) const;
    void addToExpirationQueue(CValueQueueEntry& entry) const;
    void removeFromExpirationQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const;
    valueQueueType::iterator getQueueCacheRow(int nHeight, bool createIfNotExists) const;
    valueQueueType::iterator getExpirationQueueCacheRow(int nHeight, bool createIfNotExists) const;
    std::pair<std::string, CNCCTrieProofLeafNode> getLeafNodeForProof(const std::string& currentPosition, unsigned char nodeChar, const CNCCTrieNode* currentNode) const;
    uint256 hashBlock;
};

#endif // BITCOIN_NCCTRIE_H
