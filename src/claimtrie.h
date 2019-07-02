#ifndef BITCOIN_CLAIMTRIE_H
#define BITCOIN_CLAIMTRIE_H

#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <dbwrapper.h>
#include <prefixtrie.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <util.h>

#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// leveldb keys
#define TRIE_NODE 'n'
#define CLAIM_BY_ID 'i'
#define CLAIM_QUEUE_ROW 'r'
#define CLAIM_QUEUE_NAME_ROW 'm'
#define EXP_QUEUE_ROW 'e'
#define SUPPORT 's'
#define SUPPORT_QUEUE_ROW 'u'
#define SUPPORT_QUEUE_NAME_ROW 'p'
#define SUPPORT_EXP_QUEUE_ROW 'x'

uint256 getValueHash(const COutPoint& outPoint, int nHeightOfLastTakeover);

struct CClaimValue
{
    COutPoint outPoint;
    uint160 claimId;
    CAmount nAmount;
    CAmount nEffectiveAmount;
    int nHeight;
    int nValidAtHeight;

    CClaimValue()
    {
    }

    CClaimValue(const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight, int nValidAtHeight)
        : outPoint(outPoint), claimId(claimId), nAmount(nAmount), nEffectiveAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight)
    {
    }

    CClaimValue(CClaimValue&&) = default;
    CClaimValue(const CClaimValue&) = default;
    CClaimValue& operator=(CClaimValue&&) = default;
    CClaimValue& operator=(const CClaimValue&) = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
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
        if (nEffectiveAmount != other.nEffectiveAmount)
            return false;
        if (nHeight > other.nHeight)
            return true;
        if (nHeight != other.nHeight)
            return false;
        return outPoint != other.outPoint && !(outPoint < other.outPoint);
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

struct CSupportValue
{
    COutPoint outPoint;
    uint160 supportedClaimId;
    CAmount nAmount;
    int nHeight;
    int nValidAtHeight;

    CSupportValue()
    {
    }

    CSupportValue(const COutPoint& outPoint, const uint160& supportedClaimId, CAmount nAmount, int nHeight, int nValidAtHeight)
        : outPoint(outPoint), supportedClaimId(supportedClaimId), nAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight)
    {
    }

    CSupportValue(CSupportValue&&) = default;
    CSupportValue(const CSupportValue&) = default;
    CSupportValue& operator=(CSupportValue&&) = default;
    CSupportValue& operator=(const CSupportValue&) = default;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
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

typedef std::vector<CClaimValue> claimEntryType;
typedef std::vector<CSupportValue> supportEntryType;

struct CClaimTrieData
{
    uint256 hash;
    claimEntryType claims;
    int nHeightOfLastTakeover = 0;

    CClaimTrieData() = default;
    CClaimTrieData(CClaimTrieData&&) = default;
    CClaimTrieData(const CClaimTrieData&) = default;
    CClaimTrieData& operator=(CClaimTrieData&&) = default;
    CClaimTrieData& operator=(const CClaimTrieData& d) = default;

    bool insertClaim(const CClaimValue& claim);
    bool removeClaim(const COutPoint& outPoint, CClaimValue& claim);
    bool getBestClaim(CClaimValue& claim) const;
    bool haveClaim(const COutPoint& outPoint) const;
    void reorderClaims(const supportEntryType& support);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(hash);

        if (ser_action.ForRead()) {
            if (s.eof()) {
                claims.clear();
                nHeightOfLastTakeover = 0;
                return;
            }
        }
        else if (claims.empty())
            return;

        READWRITE(claims);
        READWRITE(nHeightOfLastTakeover);
    }

    bool operator==(const CClaimTrieData& other) const
    {
        return hash == other.hash && nHeightOfLastTakeover == other.nHeightOfLastTakeover && claims == other.claims;
    }

    bool operator!=(const CClaimTrieData& other) const
    {
        return !(*this == other);
    }

    bool empty() const
    {
        return claims.empty();
    }
};

struct COutPointHeightType
{
    COutPoint outPoint;
    int nHeight;

    COutPointHeightType()
    {
    }

    COutPointHeightType(const COutPoint& outPoint, int nHeight)
        : outPoint(outPoint), nHeight(nHeight)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(outPoint);
        READWRITE(nHeight);
    }
};

struct CNameOutPointHeightType
{
    std::string name;
    COutPoint outPoint;
    int nHeight;

    CNameOutPointHeightType()
    {
    }

    CNameOutPointHeightType(std::string name, const COutPoint& outPoint, int nHeight)
        : name(std::move(name)), outPoint(outPoint), nHeight(nHeight)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(outPoint);
        READWRITE(nHeight);
    }
};

struct CNameOutPointType
{
    std::string name;
    COutPoint outPoint;

    CNameOutPointType()
    {
    }

    CNameOutPointType(std::string name, const COutPoint& outPoint)
        : name(std::move(name)), outPoint(outPoint)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(outPoint);
    }
};

struct CClaimIndexElement
{
    CClaimIndexElement()
    {
    }

    CClaimIndexElement(const std::string& name, const CClaimValue& claim)
        : name(name), claim(claim)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(claim);
    }

    std::string name;
    CClaimValue claim;
};

struct CClaimsForNameType
{
    claimEntryType claims;
    supportEntryType supports;
    int nLastTakeoverHeight;
    std::string name;

    CClaimsForNameType(claimEntryType claims, supportEntryType supports, int nLastTakeoverHeight, const std::string& name)
        : claims(std::move(claims)), supports(std::move(supports)), nLastTakeoverHeight(nLastTakeoverHeight), name(name)
    {
    }

    CClaimsForNameType(CClaimsForNameType&&) = default;
    CClaimsForNameType(const CClaimsForNameType&) = default;
    CClaimsForNameType& operator=(CClaimsForNameType&&) = default;
    CClaimsForNameType& operator=(const CClaimsForNameType&) = default;
};

class CClaimTrie : public CPrefixTrie<std::string, CClaimTrieData>
{
    int nNextHeight = 0;
    int nProportionalDelayFactor = 0;
    std::unique_ptr<CDBWrapper> db;

public:
    CClaimTrie() = default;
    CClaimTrie(CClaimTrie&&) = delete;
    CClaimTrie(const CClaimTrie&) = delete;
    CClaimTrie(bool fMemory, bool fWipe, int proportionalDelayFactor = 32);

    CClaimTrie& operator=(CClaimTrie&&) = delete;
    CClaimTrie& operator=(const CClaimTrie&) = delete;

    bool SyncToDisk();

    friend class CClaimTrieCacheBase;
    friend struct ClaimTrieChainFixture;
    friend class CClaimTrieCacheExpirationFork;
    friend class CClaimTrieCacheNormalizationFork;
};

struct CClaimTrieProofNode
{
    CClaimTrieProofNode(std::vector<std::pair<unsigned char, uint256>> children, bool hasValue, const uint256& valHash)
        : children(std::move(children)), hasValue(hasValue), valHash(valHash)
    {
    }

    CClaimTrieProofNode(CClaimTrieProofNode&&) = default;
    CClaimTrieProofNode(const CClaimTrieProofNode&) = default;
    CClaimTrieProofNode& operator=(CClaimTrieProofNode&&) = default;
    CClaimTrieProofNode& operator=(const CClaimTrieProofNode&) = default;

    std::vector<std::pair<unsigned char, uint256>> children;
    bool hasValue;
    uint256 valHash;
};

struct CClaimTrieProof
{
    CClaimTrieProof()
    {
    }

    CClaimTrieProof(std::vector<CClaimTrieProofNode> nodes, bool hasValue, const COutPoint& outPoint, int nHeightOfLastTakeover)
        : nodes(std::move(nodes)), hasValue(hasValue), outPoint(outPoint), nHeightOfLastTakeover(nHeightOfLastTakeover)
    {
    }

    CClaimTrieProof(CClaimTrieProof&&) = default;
    CClaimTrieProof(const CClaimTrieProof&) = default;
    CClaimTrieProof& operator=(CClaimTrieProof&&) = default;
    CClaimTrieProof& operator=(const CClaimTrieProof&) = default;

    std::vector<CClaimTrieProofNode> nodes;
    bool hasValue;
    COutPoint outPoint;
    int nHeightOfLastTakeover;
};

template <typename T>
using queueEntryType = std::pair<std::string, T>;

typedef std::vector<queueEntryType<CClaimValue>> claimQueueRowType;
typedef std::map<int, claimQueueRowType> claimQueueType;

typedef std::vector<queueEntryType<CSupportValue>> supportQueueRowType;
typedef std::map<int, supportQueueRowType> supportQueueType;

typedef std::vector<COutPointHeightType> queueNameRowType;
typedef std::map<std::string, queueNameRowType> queueNameType;

typedef std::vector<CNameOutPointHeightType> insertUndoType;

typedef std::vector<CNameOutPointType> expirationQueueRowType;
typedef std::map<int, expirationQueueRowType> expirationQueueType;

typedef std::set<CClaimValue> claimIndexClaimListType;
typedef std::vector<CClaimIndexElement> claimIndexElementListType;

class CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheBase(CClaimTrie* base, bool fRequireTakeoverHeights = true);
    virtual ~CClaimTrieCacheBase() = default;

    uint256 getMerkleHash();
    bool checkConsistency() const;

    bool getClaimById(const uint160& claimId, std::string& name, CClaimValue& claim) const;

    bool flush();
    bool empty() const;
    bool ReadFromDisk(const CBlockIndex* tip);

    bool haveClaim(const std::string& name, const COutPoint& outPoint) const;
    bool haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight);

    bool haveSupport(const std::string& name, const COutPoint& outPoint) const;
    bool haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight);

    std::size_t getTotalNamesInTrie() const;
    std::size_t getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;

    bool addClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight);
    bool undoAddClaim(const std::string& name, const COutPoint& outPoint, int nHeight);

    bool spendClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight);
    bool undoSpendClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight, int nValidAtHeight);

    bool addSupport(const std::string& name, const COutPoint& outPoint, CAmount nAmount, const uint160& supportedClaimId, int nHeight);
    bool undoAddSupport(const std::string& name, const COutPoint& outPoint, int nHeight);

    bool spendSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight);
    bool undoSpendSupport(const std::string& name, const COutPoint& outPoint, const uint160& supportedClaimId, CAmount nAmount, int nHeight, int nValidAtHeight);

    virtual bool incrementBlock(insertUndoType& insertUndo,
        claimQueueRowType& expireUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo,
        std::vector<std::pair<std::string, int>>& takeoverHeightUndo);

    virtual bool decrementBlock(insertUndoType& insertUndo,
        claimQueueRowType& expireUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo);

    virtual bool getProofForName(const std::string& name, CClaimTrieProof& proof);
    virtual bool getInfoForName(const std::string& name, CClaimValue& claim) const;

    virtual int expirationTime() const;

    bool finalizeDecrement(std::vector<std::pair<std::string, int>>& takeoverHeightUndo);

    virtual CClaimsForNameType getClaimsForName(const std::string& name) const;

    CAmount getEffectiveAmountForClaim(const std::string& name, const uint160& claimId, std::vector<CSupportValue>* supports = nullptr) const;
    CAmount getEffectiveAmountForClaim(const CClaimsForNameType& claims, const uint160& claimId, std::vector<CSupportValue>* supports = nullptr) const;

    CClaimTrie::const_iterator begin() const;
    CClaimTrie::const_iterator end() const;
    CClaimTrie::const_iterator find(const std::string& name) const;

    void dumpToLog(CClaimTrie::const_iterator it, bool diffFromBase = true) const;

protected:
    CClaimTrie* base;
    CClaimTrie cache;
    std::unordered_set<std::string> namesToCheckForTakeover;

    uint256 recursiveComputeMerkleHash(CClaimTrie::iterator& it);

    virtual bool insertClaimIntoTrie(const std::string& name, const CClaimValue& claim, bool fCheckTakeover);
    virtual bool removeClaimFromTrie(const std::string& name, const COutPoint& outPoint, CClaimValue& claim, bool fCheckTakeover);

    virtual bool insertSupportIntoMap(const std::string& name, const CSupportValue& support, bool fCheckTakeover);
    virtual bool removeSupportFromMap(const std::string& name, const COutPoint& outPoint, CSupportValue& support, bool fCheckTakeover);

    virtual bool removeSupportFromQueue(const std::string& name, const COutPoint& outPoint, CSupportValue& support);

    virtual std::string adjustNameForValidHeight(const std::string& name, int validHeight) const;

    supportEntryType getSupportsForName(const std::string& name) const;

    int getDelayForName(const std::string& name) const;
    virtual int getDelayForName(const std::string& name, const uint160& claimId) const;

    CClaimTrie::iterator cacheData(const std::string& name, bool create = true);

    bool getLastTakeoverForName(const std::string& name, uint160& claimId, int& takeoverHeight) const;

    int getNumBlocksOfContinuousOwnership(const std::string& name) const;

    expirationQueueType expirationQueueCache;
    expirationQueueType supportExpirationQueueCache;

    int nNextHeight; // Height of the block that is being worked on, which is
                     // one greater than the height of the chain's tip

    template <typename T>
    void reactivate(const expirationQueueRowType& row, int height, bool increment);

private:
    uint256 hashBlock;

    std::unordered_map<std::string, std::pair<uint160, int>> takeoverCache;

    bool fRequireTakeoverHeights;

    claimQueueType claimQueueCache;
    queueNameType claimQueueNameCache;
    supportQueueType supportQueueCache;
    queueNameType supportQueueNameCache;
    claimIndexElementListType claimsToAdd;
    claimIndexClaimListType claimsToDelete;

    std::unordered_map<std::string, supportEntryType> cacheSupports;
    std::unordered_set<std::string> nodesToDelete;
    std::unordered_set<std::string> alreadyCachedNodes;
    std::unordered_map<std::string, bool> takeoverWorkaround;
    std::unordered_set<std::string> removalWorkaround;

    bool shouldUseTakeoverWorkaround(const std::string& key) const;
    void addTakeoverWorkaroundPotential(const std::string& key);
    void confirmTakeoverWorkaroundNeeded(const std::string& key);

    bool clear();

    void markAsDirty(const std::string& name, bool fCheckTakeover);
    bool removeSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover);
    bool removeClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover);
    bool removeClaimFromQueue(const std::string& name, const COutPoint& outPoint, CClaimValue& claim);

    template <typename T>
    std::pair<const int, std::vector<queueEntryType<T>>>* getQueueCacheRow(int nHeight, bool createIfNotExists = false);

    template <typename T>
    typename queueNameType::value_type* getQueueCacheNameRow(const std::string& name, bool createIfNotExists = false);

    template <typename T>
    typename expirationQueueType::value_type* getExpirationQueueCacheRow(int nHeight, bool createIfNotExists = false);

    template <typename T>
    bool haveInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight);

    template <typename T>
    T add(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount, int nHeight);

    template <typename T>
    bool remove(T& value, const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover = false);

    template <typename T>
    bool addToQueue(const std::string& name, const T& value);

    template <typename T>
    bool removeFromQueue(const std::string& name, const COutPoint& outPoint, T& value);

    template <typename T>
    bool addToCache(const std::string& name, const T& value, bool fCheckTakeover = false);

    template <typename T>
    bool removeFromCache(const std::string& name, const COutPoint& outPoint, T& value, bool fCheckTakeover = false);

    template <typename T>
    bool undoSpend(const std::string& name, const T& value, int nValidAtHeight);

    template <typename T>
    void undoIncrement(insertUndoType& insertUndo, std::vector<queueEntryType<T>>& expireUndo, std::set<T>* deleted = nullptr);

    template <typename T>
    void undoDecrement(insertUndoType& insertUndo, std::vector<queueEntryType<T>>& expireUndo, std::vector<CClaimIndexElement>* added = nullptr, std::set<T>* deleted = nullptr);

    template <typename T>
    void undoIncrement(const std::string& name, insertUndoType& insertUndo, std::vector<queueEntryType<T>>& expireUndo);

    // for unit test
    friend struct ClaimTrieChainFixture;
    friend class CClaimTrieCacheTest;
};

class CClaimTrieCacheExpirationFork : public CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheExpirationFork(CClaimTrie* base, bool fRequireTakeoverHeights = true);

    void setExpirationTime(int time);
    int expirationTime() const override;

    void expirationForkActive(int height, bool increment);

    bool incrementBlock(insertUndoType& insertUndo,
        claimQueueRowType& expireUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo,
        std::vector<std::pair<std::string, int>>& takeoverHeightUndo) override;

    bool decrementBlock(insertUndoType& insertUndo,
        claimQueueRowType& expireUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo) override;

private:
    int nExpirationTime;
    bool forkForExpirationChange(bool increment);
};

class CClaimTrieCacheNormalizationFork : public CClaimTrieCacheExpirationFork
{
public:
    explicit CClaimTrieCacheNormalizationFork(CClaimTrie* base, bool fRequireTakeoverHeights = true)
        : CClaimTrieCacheExpirationFork(base, fRequireTakeoverHeights), overrideInsertNormalization(false), overrideRemoveNormalization(false)
    {
    }

    bool shouldNormalize() const;

    // lower-case and normalize any input string name
    // see: https://unicode.org/reports/tr15/#Norm_Forms
    std::string normalizeClaimName(const std::string& name, bool force = false) const; // public only for validating name field on update op

    bool incrementBlock(insertUndoType& insertUndo,
        claimQueueRowType& expireUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo,
        std::vector<std::pair<std::string, int>>& takeoverHeightUndo) override;

    bool decrementBlock(insertUndoType& insertUndo,
        claimQueueRowType& expireUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo) override;

    bool getProofForName(const std::string& name, CClaimTrieProof& proof) override;
    bool getInfoForName(const std::string& name, CClaimValue& claim) const override;
    CClaimsForNameType getClaimsForName(const std::string& name) const override;

protected:
    bool insertClaimIntoTrie(const std::string& name, const CClaimValue& claim, bool fCheckTakeover = false) override;
    bool removeClaimFromTrie(const std::string& name, const COutPoint& outPoint, CClaimValue& claim, bool fCheckTakeover = false) override;

    bool insertSupportIntoMap(const std::string& name, const CSupportValue& support, bool fCheckTakeover) override;
    bool removeSupportFromMap(const std::string& name, const COutPoint& outPoint, CSupportValue& support, bool fCheckTakeover) override;

    int getDelayForName(const std::string& name, const uint160& claimId) const override;

    std::string adjustNameForValidHeight(const std::string& name, int validHeight) const override;

private:
    bool overrideInsertNormalization;
    bool overrideRemoveNormalization;

    bool normalizeAllNamesInTrieIfNecessary(insertUndoType& insertUndo,
        claimQueueRowType& removeUndo,
        insertUndoType& insertSupportUndo,
        supportQueueRowType& expireSupportUndo,
        std::vector<std::pair<std::string, int>>& takeoverHeightUndo);
};

typedef CClaimTrieCacheNormalizationFork CClaimTrieCache;

#endif // BITCOIN_CLAIMTRIE_H
