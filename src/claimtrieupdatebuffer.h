#ifndef BITCOIN_CLAIMTRIEUPDATEBUFFER_H
#define BITCOIN_CLAIMTRIEUPDATEBUFFER_H

#include "amount.h"
#include "claimtrie.h"

#include <string>
#include <vector>
#include <map>
#include <set>

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

class CClaimTrieUpdateBuffer
{
public:
    CClaimTrieUpdateBuffer(CClaimTrie* base, bool fRequireTakeoverHeights = true);

    ~CClaimTrieUpdateBuffer();

    uint256 getMerkleHash() const;

    bool empty() const;
    bool flush();
    bool dirty() const;

    bool addClaim(const std::string& name, const COutPoint& outPoint,
                  uint160 claimId, CAmount nAmount, int nHeight);
    bool undoAddClaim(const std::string& name, const COutPoint& outPoint,
                      int nHeight);
    bool spendClaim(const std::string& name, const COutPoint& outPoint,
                    int nHeight, int& nValidAtHeight);
    bool undoSpendClaim(const std::string& name, const COutPoint& outPoint,
                        uint160 claimId, CAmount nAmount, int nHeight,
                        int nValidAtHeight);

    bool addSupport(const std::string& name, const COutPoint& outPoint,
                    CAmount nAmount, uint160 supportedClaimId,
                    int nHeight);
    bool undoAddSupport(const std::string& name, const COutPoint& outPoint,
                        int nHeight);
    bool spendSupport(const std::string& name, const COutPoint& outPoint,
                      int nHeight, int& nValidAtHeight);
    bool undoSpendSupport(const std::string& name, const COutPoint& outPoint,
                          uint160 supportedClaimId, CAmount nAmount,
                          int nHeight, int nValidAtHeight);

    uint256 getBestBlock() const;
    void setBestBlock(const uint256& hashBlock);

    bool incrementBlock(insertUndoType& insertUndo,
                        claimQueueRowType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportQueueRowType& expireSupportUndo,
                        std::vector<std::pair<std::string, int> >& takeoverHeightUndo);
    bool decrementBlock(insertUndoType& insertUndo,
                        claimQueueRowType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportQueueRowType& expireSupportUndo,
                        std::vector<std::pair<std::string, int> >& takeoverHeightUndo);

    bool insertClaimIntoTrie(const std::string& name, CClaimValue claim,
                             bool fCheckTakeover = false);
    bool removeClaimFromTrie(const std::string& name, const COutPoint& outPoint,
                             CClaimValue& claim,
                             bool fCheckTakeover = false);
    CClaimTrieProof getProofForName(const std::string& name) const;

    bool finalizeDecrement();

    void removeAndAddSupportToExpirationQueue(supportExpirationQueueRowType &row, int height, bool increment);
    void removeAndAddToExpirationQueue(expirationQueueRowType &row, int height, bool increment);

    bool forkForExpirationChange(bool increment);

protected:
    CClaimTrie* base;

    bool fRequireTakeoverHeights;

    nodeCacheType cache;
    nodeCacheType block_originals;
    claimQueueType claimQueueCache;
    queueNameType claimQueueNameCache;
    expirationQueueType expirationQueueCache;
    supportMapType supportCache;
    supportQueueType supportQueueCache;
    supportQueueNameType supportQueueNameCache;
    supportExpirationQueueType supportExpirationQueueCache;
    std::set<std::string> namesToCheckForTakeover;
    std::map<std::string, int> cacheTakeoverHeights;
    int nCurrentHeight; // Height of the block that is being worked on, which is
                        // one greater than the height of the chain's tip

    // used in merkle hash computing
    mutable hashMapType cacheHashes;
    mutable std::set<std::string> dirtyHashes;

    uint256 hashBlock;

    uint256 computeHash() const;

    bool reorderTrieNode(const std::string& name, bool fCheckTakeover);
    bool recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent,
                                    std::string sPos) const;
    bool recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos,
                            std::string sName,
                            bool* pfNullified = NULL);

    bool clear();

    bool removeClaim(const std::string& name, const COutPoint& outPoint,
                     int nHeight, int& nValidAtHeight, bool fCheckTakeover);

    bool addClaimToQueues(const std::string& name, CClaimValue& claim);
    bool removeClaimFromQueue(const std::string& name, const COutPoint& outPoint,
                              CClaimValue& claim);

    void addToExpirationQueue(int nExpirationHeight, nameOutPointType& entry);
    void removeFromExpirationQueue(const std::string& name, const COutPoint& outPoint,
                                   int nHeight);

    bool removeSupport(const std::string& name, const COutPoint& outPoint,
                       int nHeight, int& nValidAtHeight,
                       bool fCheckTakeover);
    bool removeSupportFromMap(const std::string& name, const COutPoint& outPoint,
                              CSupportValue& support,
                              bool fCheckTakeover);

    bool insertSupportIntoMap(const std::string& name,
                              CSupportValue support,
                              bool fCheckTakeover);

    bool addSupportToQueues(const std::string& name, CSupportValue& support);
    bool removeSupportFromQueue(const std::string& name, const COutPoint& outPoint,
                                CSupportValue& support);

    void addSupportToExpirationQueue(int nExpirationHeight,
                                     nameOutPointType& entry);
    void removeSupportFromExpirationQueue(const std::string& name,
                                          const COutPoint& outPoint,
                                          int nHeight);

    bool getSupportsForName(const std::string& name,
                            supportMapEntryType& node) const;

    bool getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const;

    int getDelayForName(const std::string& name) const;

    uint256 getLeafHashForProof(const std::string& currentPosition, unsigned char nodeChar,
                                const CClaimTrieNode* currentNode) const;

    CClaimTrieNode* addNodeToCache(const std::string& position, CClaimTrieNode* original);

    bool getOriginalInfoForName(const std::string& name, CClaimValue& claim) const;

    int getNumBlocksOfContinuousOwnership(const std::string& name) const;

    template <typename K, typename V> void update(std::map<K, V> &map);
    template <typename K, typename V> typename std::map<K, V>::iterator getQueueCacheRow(const K &key, std::map<K, V> &map, bool createIfNotExists);
};

#endif // BITCOIN_CLAIMTRIEUPDATEBUFFER_H
