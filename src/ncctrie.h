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
#include "json/json_spirit_value.h"

class CNodeValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    CAmount nAmount;
    int nHeight;
    CNodeValue() {};
    CNodeValue(uint256 txhash, uint32_t nOut) : txhash(txhash), nOut(nOut), nAmount(0), nHeight(0) {}
    CNodeValue(uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) : txhash(txhash), nOut(nOut), nAmount(nAmount), nHeight(nHeight) {}
    std::string ToString();
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(nOut);
        READWRITE(nAmount);
        READWRITE(nHeight);
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
        return txhash == other.txhash && nOut == other.nOut;
    }
    bool operator!=(const CNodeValue& other) const
    {
        return !(*this == other);
    }
};

class CNCCTrieNode;
class CNCCTrie;

typedef std::map<unsigned char, CNCCTrieNode*> nodeMapType;

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
    bool removeValue(CNodeValue val, bool * fChanged = NULL);
    bool getValue(CNodeValue& val) const;
    bool empty() const {return children.empty() && values.empty();}

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

typedef std::map<std::string, CNCCTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;
class CNCCTrieCache;

class CNCCTrie
{
public:
    CNCCTrie() : db(GetDataDir() / "ncctrie", 100, false, false), root(uint256S("0000000000000000000000000000000000000000000000000000000000000001")) {}
    uint256 getMerkleHash();
    CLevelDBWrapper db;
    bool empty() const;
    bool checkConsistency();
    bool ReadFromDisk(bool check = false);
    json_spirit::Array dumpToJSON() const;
    bool getInfoForName(const std::string& name, CNodeValue& val) const;
    friend class CNCCTrieCache;
private:
    bool update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlock);
    bool updateName(const std::string& name, CNCCTrieNode* updatedNode, std::vector<std::string>& deletedNames);
    bool updateHash(const std::string& name, uint256& hash, CNCCTrieNode** pNodeRet);
    bool recursiveNullify(CNCCTrieNode* node, std::string& name, std::vector<std::string>& deletedNames);
    bool recursiveCheckConsistency(CNCCTrieNode* node);
    bool BatchWrite(nodeCacheType& changedNodes, std::vector<std::string>& deletedNames, const uint256& hashBlock);
    bool InsertFromDisk(const std::string& name, CNCCTrieNode* node);
    bool recursiveDumpToJSON(const std::string& name, const CNCCTrieNode* current, json_spirit::Array& ret) const;
    CNCCTrieNode root;
    uint256 hashBlock;
};

class CNCCTrieCache
{
public:
    CNCCTrieCache(CNCCTrie* base): base(base) {assert(base);}
    uint256 getMerkleHash() const;
    bool empty() const;
    bool flush();
    bool dirty() const { return !dirtyHashes.empty(); }
    bool insertName (const std::string name, uint256 txhash, int nOut, CAmount nAmount, int nDepth) const;
    bool removeName (const std::string name, uint256 txhash, int nOut) const;
    uint256 getBestBlock();
    void setBestBlock(const uint256& hashBlock);
    ~CNCCTrieCache() { clear(); }
private:
    CNCCTrie* base;
    mutable nodeCacheType cache;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    uint256 computeHash() const;
    bool recursiveComputeMerkleHash(CNCCTrieNode* tnCurrent, std::string sPos) const;
    bool recursivePruneName(CNCCTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified = NULL) const;
    bool clear() const;
    uint256 hashBlock;
};

#endif // BITCOIN_NCCTRIE_H
