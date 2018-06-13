#ifndef BITCOIN_CLAIMTRIE_H
#define BITCOIN_CLAIMTRIE_H

#include "amount.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "claimtriedb.h"
#include "chainparams.h"
#include "primitives/transaction.h"

#include <string>
#include <vector>

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
    bool empty() const;
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

class CClaimIndexElement
{
  public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(name);
        READWRITE(claim);
    }

    bool empty() const
    {
        return name.empty() && claim.claimId.IsNull();
    }

    std::string name;
    CClaimValue claim;
};

// Helpers to separate queue types from each other
struct claimQueueEntryHelper;
struct supportQueueEntryHelper;

struct queueNameRowHelper;
struct supportQueueNameRowHelper;

struct expirationQueueRowHelper;
struct supportExpirationQueueRowHelper;

template <typename Base, typename Form>
struct Generic : public Base
{
    Generic() : Base() {}
    Generic(const Base &base) : Base(base) {}
    Generic(const Generic &generic) : Base(generic) {}
};

// Make std::swap to work with custom types
namespace std {
template <typename Base, typename Form> inline
void swap(Generic<Base, Form> &generic, Base &base)
{
    Base base1 = generic;
    generic = base;
    base = base1;
}

template <typename Base, typename Form> inline
void swap(Base &base, Generic<Base, Form> &generic)
{
    swap(generic, base);
}
}

// Each of these Generic types will be stored in the database
typedef Generic<CClaimValue, claimQueueEntryHelper> claimQueueValueType;
typedef Generic<CSupportValue, supportQueueEntryHelper> supportQueueValueType;

typedef Generic<outPointHeightType, queueNameRowHelper> queueNameRowValueType;
typedef Generic<outPointHeightType, supportQueueNameRowHelper> supportQueueNameRowValueType;

typedef Generic<nameOutPointType, expirationQueueRowHelper> expirationQueueRowValueType;
typedef Generic<nameOutPointType, supportExpirationQueueRowHelper> supportExpirationQueueRowValueType;

typedef std::pair<std::string, claimQueueValueType> claimQueueEntryType;

typedef std::pair<std::string, supportQueueValueType> supportQueueEntryType;

typedef std::map<std::string, supportMapEntryType> supportMapType;

typedef std::vector<queueNameRowValueType> queueNameRowType;
typedef std::map<std::string, queueNameRowType> queueNameType;

typedef std::vector<supportQueueNameRowValueType> supportQueueNameRowType;
typedef std::map<std::string, supportQueueNameRowType> supportQueueNameType;

typedef std::vector<nameOutPointHeightType> insertUndoType;

typedef std::vector<expirationQueueRowValueType> expirationQueueRowType;
typedef std::map<int, expirationQueueRowType> expirationQueueType;

typedef std::vector<supportExpirationQueueRowValueType> supportExpirationQueueRowType;
typedef std::map<int, supportExpirationQueueRowType> supportExpirationQueueType;

typedef std::vector<claimQueueEntryType> claimQueueRowType;
typedef std::map<int, claimQueueRowType> claimQueueType;

typedef std::vector<supportQueueEntryType> supportQueueRowType;
typedef std::map<int, supportQueueRowType> supportQueueType;

typedef std::map<std::string, CClaimTrieNode*, nodenamecompare> nodeCacheType;

typedef std::map<std::string, uint256> hashMapType;

HASH_VALUE(int, claimQueueRowType);
HASH_VALUE(int, supportQueueRowType);
HASH_VALUE(int, expirationQueueRowType);
HASH_VALUE(int, supportExpirationQueueRowType);
HASH_VALUE(uint160, CClaimIndexElement);
HASH_VALUE(std::string, CClaimTrieNode);
HASH_VALUE(std::string, queueNameRowType);
HASH_VALUE(std::string, supportMapEntryType);
HASH_VALUE(std::string, supportQueueNameRowType);

struct claimsForNameType
{
    std::vector<CClaimValue> claims;
    std::vector<CSupportValue> supports;
    int nLastTakeoverHeight;

    claimsForNameType(std::vector<CClaimValue> claims, std::vector<CSupportValue> supports, int nLastTakeoverHeight)
    : claims(claims), supports(supports), nLastTakeoverHeight(nLastTakeoverHeight) {}
};

class CClaimTrieCache;

class CClaimTrie
{
public:
    CClaimTrie(bool fMemory = false, bool fWipe = false, int nProportionalDelayFactor = 32);

    ~CClaimTrie();

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

    CAmount getEffectiveAmountForClaim(const std::string& name, uint160 claimId) const;
    CAmount getEffectiveAmountForClaimWithSupports(const std::string& name, uint160 claimId,
                                                   std::vector<CSupportValue>& supports) const;

    bool queueEmpty() const;
    bool supportEmpty() const;
    bool supportQueueEmpty() const;
    bool expirationQueueEmpty() const;
    bool supportExpirationQueueEmpty() const;

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

    friend class CClaimTrieCache;

    int nCurrentHeight;
    int nExpirationTime;
    int nProportionalDelayFactor;
private:
    CClaimTrieDb db;
    void clear(CClaimTrieNode* current);

    const CClaimTrieNode* getNodeForName(const std::string& name) const;

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

    CClaimTrieNode root;
    uint256 hashBlock;

    nodeCacheType dirtyNodes;
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
        hashBlock = base->hashBlock;
        nCurrentHeight = base->nCurrentHeight;
    }

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

    ~CClaimTrieCache() { clear(); }

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

#endif // BITCOIN_CLAIMTRIE_H
