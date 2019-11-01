#ifndef CLAIMTRIE_TRIE_H
#define CLAIMTRIE_TRIE_H

#include <data.h>
#include <sqlite.h>
#include <txoutpoint.h>
#include <uints.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <utility>

CUint256 getValueHash(const CTxOutPoint& outPoint, int nHeightOfLastTakeover);

class CClaimTrie
{
    friend class CClaimTrieCacheBase;
    friend class ClaimTrieChainFixture;
    friend class CClaimTrieCacheHashFork;
    friend class CClaimTrieCacheExpirationFork;
    friend class CClaimTrieCacheNormalizationFork;

public:
    CClaimTrie() = delete;
    CClaimTrie(CClaimTrie&&) = delete;
    CClaimTrie(const CClaimTrie&) = delete;
    CClaimTrie(bool fWipe, int height = 0,
               const std::string& dataDir = ".",
               int nNormalizedNameForkHeight = 1,
               int64_t nOriginalClaimExpirationTime = 1,
               int64_t nExtendedClaimExpirationTime = 1,
               int64_t nExtendedClaimExpirationForkHeight = 1,
               int64_t nAllClaimsInMerkleForkHeight = 1,
               int proportionalDelayFactor = 32);

    CClaimTrie& operator=(CClaimTrie&&) = delete;
    CClaimTrie& operator=(const CClaimTrie&) = delete;

    bool empty();
    bool SyncToDisk();

protected:
    int nNextHeight;
    sqlite::database db;
    const int nProportionalDelayFactor;

    const int nNormalizedNameForkHeight;
    const int64_t nOriginalClaimExpirationTime;
    const int64_t nExtendedClaimExpirationTime;
    const int64_t nExtendedClaimExpirationForkHeight;
    const int64_t nAllClaimsInMerkleForkHeight;
};

template <typename T>
using queueEntryType = std::pair<std::string, T>;

#ifdef SWIG_INTERFACE // swig has a problem with using in typedef
using claimUndoPair = std::pair<std::string, CClaimValue>;
using supportUndoPair = std::pair<std::string, CSupportValue>;
using takeoverUndoPair = std::pair<std::string, std::pair<int, CUint160>>;
#else
using claimUndoPair = queueEntryType<CClaimValue>;
using supportUndoPair = queueEntryType<CSupportValue>;
using takeoverUndoPair = queueEntryType<std::pair<int, CUint160>>;
#endif

typedef std::vector<claimUndoPair> claimUndoType;
typedef std::vector<supportUndoPair> supportUndoType;
typedef std::vector<CNameOutPointHeightType> insertUndoType;
typedef std::vector<takeoverUndoPair> takeoverUndoType;

class CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheBase(CClaimTrie* base);
    virtual ~CClaimTrieCacheBase();

    bool flush();
    bool checkConsistency();
    CUint256 getMerkleHash();
    bool validateDb(const CUint256& rootHash);

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

    void getNamesInTrie(std::function<void(const std::string&)> callback) const;
    bool getLastTakeoverForName(const std::string& name, CUint160& claimId, int& takeoverHeight) const;
    bool findNameForClaim(std::vector<unsigned char> claim, CClaimValue& value, std::string& name) const;

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
