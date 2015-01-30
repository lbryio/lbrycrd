#ifndef BITCOIN_NCC_H
#define BITCOIN_NCC_H

#include "script/script.h"
#include "amount.h"
#include "primitives/transaction.h"
#include "coins.h"
#include "hash.h"
#include "uint256.h"
#include "util.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>

bool DecodeNCCScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams);
bool DecodeNCCScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc);
CScript StripNCCScriptPrefix(const CScript& scriptIn);


class CNodeValue
{
public:
    uint256 txhash;
    uint32_t nOut;
    CAmount nAmount;
    int nHeight;
    CNodeValue() {};
    CNodeValue(uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) : txhash(txhash), nOut(nOut), nAmount(nAmount), nHeight(nHeight) {}
    std::string ToString();
    bool operator<(const CNodeValue& other)
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
    bool operator==(const CNodeValue& other)
    {
        return txhash == other.txhash && nOut == other.nOut && nAmount == other.nAmount && nHeight == other.nHeight;
    }
    bool operator!=(const CNodeValue& other)
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
    bool insertValue(CNodeValue val, bool * fChanged);
    bool removeValue(CNodeValue val, bool * fChanged);
    bool getValue(CNodeValue& val);
    bool empty() const {return children.empty() && values.empty();}
    friend class CNCCTrie;
private:
    std::vector<CNodeValue> values;

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
    CNCCTrie(CCoinsViewCache* coins) : coins(coins), root(uint256S("0000000000000000000000000000000000000000000000000000000000000001")) {}
    uint256 getMerkleHash();
    bool empty() const;
    bool checkConsistency();
    friend class CNCCTrieCache;
private:
    CCoinsViewCache* coins;
    bool update(nodeCacheType& cache, hashMapType& hashes);
    bool updateName(const std::string& name, CNCCTrieNode* updatedNode);
    bool updateHash(const std::string& name, uint256& hash);
    bool recursiveNullify(CNCCTrieNode* node);
    bool recursiveCheckConsistency(CNCCTrieNode* node);
    CNCCTrieNode root;
};

class CNCCTrieCache
{
public:
    CNCCTrieCache(CNCCTrie* base): base(base) {}
    uint256 getMerkleHash() const;
    bool empty() const;
    bool Flush();
    bool isDirty() const { return !dirtyHashes.empty(); }
    bool insertName (const std::string name, uint256 txhash, int nOut, CAmount nAmount, int nDepth) const;
    bool removeName (const std::string name, uint256 txhash, int nOut, CAmount nAmount, int nDepth) const;
private:
    CNCCTrie* base;
    mutable nodeCacheType cache;
    mutable std::set<std::string> dirtyHashes;
    mutable hashMapType cacheHashes;
    uint256 computeHash() const;
    bool recursiveComputeMerkleHash(CNCCTrieNode* tnCurrent, std::string sPos) const;
    bool recursivePruneName(CNCCTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified = NULL) const;
};

#endif // BITCOIN_NCC_H
