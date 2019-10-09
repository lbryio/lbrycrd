#ifndef CLAIMTRIE_TRIE_H
#define CLAIMTRIE_TRIE_H

#include <claimtrie/data.h>
#include <claimtrie/txoutpoint.h>
#include <claimtrie/uints.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

CUint256 getValueHash(const CTxOutPoint& outPoint, int nHeightOfLastTakeover);

struct CClaimNsupports
{
    CClaimNsupports() = default;
    CClaimNsupports(CClaimNsupports&&) = default;
    CClaimNsupports(const CClaimNsupports&) = default;

    bool operator<(const CClaimNsupports& other) const;
    CClaimNsupports& operator=(CClaimNsupports&&) = default;
    CClaimNsupports& operator=(const CClaimNsupports&) = default;

    CClaimNsupports(CClaimValue claim, int64_t effectiveAmount, std::vector<CSupportValue> supports = {});

    bool IsNull() const;

    CClaimValue claim;
    int64_t effectiveAmount = 0;
    std::vector<CSupportValue> supports;
};

struct CClaimSupportToName
{
    CClaimSupportToName(std::string name, int nLastTakeoverHeight, std::vector<CClaimNsupports> claimsNsupports, std::vector<CSupportValue> unmatchedSupports);

    const CClaimNsupports& find(const CUint160& claimId) const;
    const CClaimNsupports& find(const std::string& partialId) const;

    const std::string name;
    const int nLastTakeoverHeight;
    const std::vector<CClaimNsupports> claimsNsupports;
    const std::vector<CSupportValue> unmatchedSupports;
};

class CClaimTrie
{
    friend class CClaimTrieCacheBase;
    friend class ClaimTrieChainFixture;
    friend class CClaimTrieCacheHashFork;
    friend class CClaimTrieCacheExpirationFork;
    friend class CClaimTrieCacheNormalizationFork;
public:
    CClaimTrie() = default;
    CClaimTrie(CClaimTrie&&) = delete;
    CClaimTrie(const CClaimTrie&) = delete;
    CClaimTrie(bool fWipe, int height,
               int nNormalizedNameForkHeight,
               int64_t nOriginalClaimExpirationTime,
               int64_t nExtendedClaimExpirationTime,
               int64_t nExtendedClaimExpirationForkHeight,
               int64_t nAllClaimsInMerkleForkHeight,
               int proportionalDelayFactor = 32);

    CClaimTrie& operator=(CClaimTrie&&) = delete;
    CClaimTrie& operator=(const CClaimTrie&) = delete;

    bool empty();
    bool SyncToDisk();

protected:
    int nNextHeight = 0;
    sqlite::database db;
    const int nProportionalDelayFactor = 0;

    const int nNormalizedNameForkHeight = -1;
    const int64_t nOriginalClaimExpirationTime = -1;
    const int64_t nExtendedClaimExpirationTime = -1;
    const int64_t nExtendedClaimExpirationForkHeight = -1;
    const int64_t nAllClaimsInMerkleForkHeight = -1;
};

struct CClaimTrieProofNode
{
    CClaimTrieProofNode(std::vector<std::pair<unsigned char, CUint256>> children, bool hasValue, CUint256 valHash);

    CClaimTrieProofNode(CClaimTrieProofNode&&) = default;
    CClaimTrieProofNode(const CClaimTrieProofNode&) = default;
    CClaimTrieProofNode& operator=(CClaimTrieProofNode&&) = default;
    CClaimTrieProofNode& operator=(const CClaimTrieProofNode&) = default;

    std::vector<std::pair<unsigned char, CUint256>> children;
    bool hasValue;
    CUint256 valHash;
};

struct CClaimTrieProof
{
    CClaimTrieProof() = default;
    CClaimTrieProof(CClaimTrieProof&&) = default;
    CClaimTrieProof(const CClaimTrieProof&) = default;
    CClaimTrieProof& operator=(CClaimTrieProof&&) = default;
    CClaimTrieProof& operator=(const CClaimTrieProof&) = default;

    std::vector<std::pair<bool, CUint256>> pairs;
    std::vector<CClaimTrieProofNode> nodes;
    int nHeightOfLastTakeover = 0;
    bool hasValue = false;
    CTxOutPoint outPoint;
};

template <typename T>
using queueEntryType = std::pair<std::string, T>;

typedef std::vector<queueEntryType<CClaimValue>> claimUndoType;
typedef std::vector<queueEntryType<CSupportValue>> supportUndoType;
typedef std::vector<CNameOutPointHeightType> insertUndoType;
typedef std::vector<queueEntryType<std::pair<int, CUint160>>> takeoverUndoType;

class CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheBase(CClaimTrie* base);
    virtual ~CClaimTrieCacheBase();

    CUint256 getMerkleHash();

    bool flush();
    bool empty() const;
    bool checkConsistency();
    bool ReadFromDisk(int nHeight, const CUint256& rootHash);

    std::size_t getTotalNamesInTrie() const;
    std::size_t getTotalClaimsInTrie() const;
    int64_t getTotalValueOfClaimsInTrie(bool fControllingOnly) const;

    bool haveClaim(const std::string& name, const CTxOutPoint& outPoint) const;
    bool haveClaimInQueue(const std::string& name, const CTxOutPoint& outPoint, int& nValidAtHeight) const;

    bool haveSupport(const std::string& name, const CTxOutPoint& outPoint) const;
    bool haveSupportInQueue(const std::string& name, const CTxOutPoint& outPoint, int& nValidAtHeight) const;

    bool addClaim(const std::string& name, const CTxOutPoint& outPoint, const CUint160& claimId, int64_t nAmount,
                  int nHeight, int nValidHeight = -1, const std::vector<unsigned char>& metadata = {});

    bool addSupport(const std::string& name, const CTxOutPoint& outPoint, const CUint160& supportedClaimId, int64_t nAmount,
                    int nHeight, int nValidHeight = -1, const std::vector<unsigned char>& metadata = {});

    bool removeClaim(const CUint160& claimId, const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight);
    bool removeSupport(const CTxOutPoint& outPoint, std::string& nodeName, int& validHeight);

    virtual bool incrementBlock(insertUndoType& insertUndo,
                                claimUndoType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportUndoType& expireSupportUndo,
                                takeoverUndoType& takeoverHeightUndo);

    virtual bool decrementBlock(insertUndoType& insertUndo,
                                claimUndoType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportUndoType& expireSupportUndo);

    virtual bool getProofForName(const std::string& name, const CUint160& claim, CClaimTrieProof& proof);
    virtual bool getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset = 0) const;

    virtual int expirationTime() const;

    virtual bool finalizeDecrement(takeoverUndoType& takeoverHeightUndo);

    virtual CClaimSupportToName getClaimsForName(const std::string& name) const;
    virtual std::string adjustNameForValidHeight(const std::string& name, int validHeight) const;

    bool findNameForClaim(std::vector<unsigned char> claim, CClaimValue& value, std::string& name);
    void getNamesInTrie(std::function<void(const std::string&)> callback);
    bool getLastTakeoverForName(const std::string& name, CUint160& claimId, int& takeoverHeight) const;

protected:
    CClaimTrie* base;
    mutable sqlite::database db;
    int nNextHeight; // Height of the block that is being worked on, which is
    bool transacting;
    mutable std::unordered_set<std::string> removalWorkaround;

    mutable sqlite::database_binder claimHashQuery, childHashQuery;

    virtual CUint256 recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly);
    supportEntryType getSupportsForName(const std::string& name) const;

    virtual int getDelayForName(const std::string& name, const CUint160& claimId) const;

    bool deleteNodeIfPossible(const std::string& name, std::string& parent, int64_t& claims);
    void ensureTreeStructureIsUpToDate();

private:
    // for unit test
    friend struct ClaimTrieChainFixture;
    friend class CClaimTrieCacheTest;

    bool activateAllFor(insertUndoType& insertUndo, insertUndoType& insertSupportUndo, const std::string& takeover);
};

#endif // CLAIMTRIE_TRIE_H
