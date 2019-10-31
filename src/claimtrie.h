#ifndef BITCOIN_CLAIMTRIE_H
#define BITCOIN_CLAIMTRIE_H

#include <amount.h>
#include <chain.h>
#include <chainparams.h>

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <util.h>

#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <sqlite/sqlite3.h>

namespace sqlite
{
    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const uint160& vec) {
        void const* buf = reinterpret_cast<void const *>(vec.begin());
        return sqlite3_bind_blob(stmt, inx, buf, int(vec.size()), SQLITE_STATIC);
    }

    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const uint256& vec) {
        void const* buf = reinterpret_cast<void const *>(vec.begin());
        return sqlite3_bind_blob(stmt, inx, buf, int(vec.size()), SQLITE_STATIC);
    }

    inline void store_result_in_db(sqlite3_context* db, const uint160& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }

    inline void store_result_in_db(sqlite3_context* db, const uint256& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }
}

#include <sqlite/hdr/sqlite_modern_cpp.h>

namespace sqlite {
    template<>
    struct has_sqlite_type<uint256, SQLITE_BLOB, void> : std::true_type {};

    template<>
    struct has_sqlite_type<uint160, SQLITE_BLOB, void> : std::true_type {};

    inline uint160 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<uint160>) {
        uint160 ret;
        auto ptr = sqlite3_column_blob(stmt, inx);
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes == ret.size());
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }

    inline uint256 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<uint256>) {
        uint256 ret;
        auto ptr = sqlite3_column_blob(stmt, inx);
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes == ret.size());
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }

}

uint256 getValueHash(const COutPoint& outPoint, int nHeightOfLastTakeover);

struct CClaimValue
{
    COutPoint outPoint;
    uint160 claimId;
    CAmount nAmount = 0;
    CAmount nEffectiveAmount = 0;
    int nHeight = 0;
    int nValidAtHeight = 0;

    CClaimValue() = default;

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
    CAmount nAmount = 0;
    int nHeight = 0;
    int nValidAtHeight = 0;

    CSupportValue() = default;

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

struct CNameOutPointHeightType
{
    std::string name;
    COutPoint outPoint;
    int nValidHeight = 0;

    CNameOutPointHeightType() = default;

    CNameOutPointHeightType(std::string name, const COutPoint& outPoint, int nValidHeight)
            : name(std::move(name)), outPoint(outPoint), nValidHeight(nValidHeight)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(outPoint);
        READWRITE(nValidHeight);
    }
};

struct CClaimNsupports
{
    CClaimNsupports() = default;
    CClaimNsupports(CClaimNsupports&&) = default;
    CClaimNsupports(const CClaimNsupports&) = default;

    CClaimNsupports& operator=(CClaimNsupports&&) = default;
    CClaimNsupports& operator=(const CClaimNsupports&) = default;

    CClaimNsupports(const CClaimValue& claim, CAmount effectiveAmount, const std::vector<CSupportValue>& supports = {})
        : claim(claim), effectiveAmount(effectiveAmount), supports(supports)
    {
    }

    bool IsNull() const
    {
        return claim.claimId.IsNull();
    }

    bool operator<(const CClaimNsupports& other) const {
        return claim < other.claim;
    }

    CClaimValue claim;
    CAmount effectiveAmount = 0;
    std::vector<CSupportValue> supports;
};

static const CClaimNsupports invalid;

struct CClaimSupportToName
{
    CClaimSupportToName(const std::string& name, int nLastTakeoverHeight, std::vector<CClaimNsupports> claimsNsupports, std::vector<CSupportValue> unmatchedSupports)
        : name(name), nLastTakeoverHeight(nLastTakeoverHeight), claimsNsupports(std::move(claimsNsupports)), unmatchedSupports(std::move(unmatchedSupports))
    {
    }

    const CClaimNsupports& find(const uint160& claimId) const
    {
        auto it = std::find_if(claimsNsupports.begin(), claimsNsupports.end(), [&claimId](const CClaimNsupports& value) {
            return claimId == value.claim.claimId;
        });
        return it != claimsNsupports.end() ? *it : invalid;
    }

    const CClaimNsupports& find(const std::string& partialId) const
    {
        std::string lowered(partialId);
        for (auto& c: lowered)
            c = std::tolower(c);

        auto it = std::find_if(claimsNsupports.begin(), claimsNsupports.end(), [&lowered](const CClaimNsupports& value) {
            return value.claim.claimId.GetHex().find(lowered) == 0;
        });
        return it != claimsNsupports.end() ? *it : invalid;
    }

    const std::string name;
    const int nLastTakeoverHeight;
    const std::vector<CClaimNsupports> claimsNsupports;
    const std::vector<CSupportValue> unmatchedSupports;
};

class CClaimTrieCacheBase;
class CClaimTrie
{
    friend CClaimTrieCacheBase;
    std::string dbPath;
    int nNextHeight;
    sqlite::database db; // keep below dbPath

public:
    const int nProportionalDelayFactor;

    CClaimTrie() = delete;
    CClaimTrie(CClaimTrie&&) = delete;
    CClaimTrie(const CClaimTrie&) = delete;
    CClaimTrie(bool fWipe, int height, int proportionalDelayFactor = 32);

    CClaimTrie& operator=(CClaimTrie&&) = delete;
    CClaimTrie& operator=(const CClaimTrie&) = delete;

    bool SyncToDisk();
    bool empty(); // for tests
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
    CClaimTrieProof() = default;
    CClaimTrieProof(CClaimTrieProof&&) = default;
    CClaimTrieProof(const CClaimTrieProof&) = default;
    CClaimTrieProof& operator=(CClaimTrieProof&&) = default;
    CClaimTrieProof& operator=(const CClaimTrieProof&) = default;

    std::vector<std::pair<bool, uint256>> pairs;
    std::vector<CClaimTrieProofNode> nodes;
    int nHeightOfLastTakeover = 0;
    bool hasValue = false;
    COutPoint outPoint;
};

template <typename T>
using queueEntryType = std::pair<std::string, T>;

typedef std::vector<queueEntryType<CClaimValue>> claimUndoType;
typedef std::vector<queueEntryType<CSupportValue>> supportUndoType;
typedef std::vector<CNameOutPointHeightType> insertUndoType;
typedef std::vector<queueEntryType<std::pair<int, uint160>>> takeoverUndoType;

class CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheBase(CClaimTrie* base);
    virtual ~CClaimTrieCacheBase();

    uint256 getMerkleHash();

    bool flush();
    bool checkConsistency();
    bool ValidateTipMatches(const CBlockIndex* tip);

    bool haveClaim(const std::string& name, const COutPoint& outPoint) const;
    bool haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const;

    bool haveSupport(const std::string& name, const COutPoint& outPoint) const;
    bool haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const;

    bool addClaim(const std::string& name, const COutPoint& outPoint, const uint160& claimId, CAmount nAmount,
            int nHeight, int nValidHeight = -1, const std::vector<unsigned char>& metadata = {});
    bool addSupport(const std::string& name, const COutPoint& outPoint, CAmount nAmount,
            const uint160& supportedClaimId, int nHeight, int nValidHeight = -1, const std::vector<unsigned char>& metadata = {});

    bool removeSupport(const COutPoint& outPoint, std::string& nodeName, int& validHeight);
    bool removeClaim(const uint160& claimId, const COutPoint& outPoint, std::string& nodeName, int& validHeight);

    virtual bool incrementBlock(insertUndoType& insertUndo,
                                claimUndoType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportUndoType& expireSupportUndo,
                                takeoverUndoType& takeovers);

    virtual bool decrementBlock(insertUndoType& insertUndo,
                                claimUndoType& expireUndo,
                                insertUndoType& insertSupportUndo,
                                supportUndoType& expireSupportUndo);

    virtual bool getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset = 0) const;
    virtual bool getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof);

    virtual int expirationTime() const;

    virtual bool finalizeDecrement(takeoverUndoType& takeovers);

    virtual CClaimSupportToName getClaimsForName(const std::string& name) const;
    virtual std::string adjustNameForValidHeight(const std::string& name, int validHeight) const;

    std::size_t getTotalNamesInTrie() const;
    std::size_t getTotalClaimsInTrie() const;
    CAmount getTotalValueOfClaimsInTrie(bool fControllingOnly) const;

    bool findNameForClaim(const std::vector<unsigned char>& claim, CClaimValue& value, std::string& name);
    void getNamesInTrie(std::function<void(const std::string&)> callback);
    bool getLastTakeoverForName(const std::string& name, uint160& claimId, int& takeoverHeight) const;

protected:
    CClaimTrie* base;
    mutable sqlite::database db;
    int nNextHeight; // Height of the block that is being worked on, which is
    bool transacting;
    // one greater than the height of the chain's tip

    virtual uint256 recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly);
    supportEntryType getSupportsForName(const std::string& name) const;

    virtual int getDelayForName(const std::string& name, const uint160& claimId) const;

    bool deleteNodeIfPossible(const std::string& name, std::string& parent, std::vector<std::string>& claims);
    void ensureTreeStructureIsUpToDate();

private:
    // for unit test
    friend struct ClaimTrieChainFixture;
    friend class CClaimTrieCacheTest;

    void activateAllFor(insertUndoType& insertUndo, insertUndoType& insertSupportUndo,
                        const std::string& takeover);
};

class CClaimTrieCacheExpirationFork : public CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheExpirationFork(CClaimTrie* base);

    void setExpirationTime(int time);
    int expirationTime() const override;

    virtual void initializeIncrement();
    bool finalizeDecrement(takeoverUndoType& takeovers) override;

    bool incrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo,
                        takeoverUndoType& takeovers) override;

    bool decrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo) override;

private:
    int nExpirationTime;
    bool forkForExpirationChange(bool increment);
};

class CClaimTrieCacheNormalizationFork : public CClaimTrieCacheExpirationFork
{
public:
    explicit CClaimTrieCacheNormalizationFork(CClaimTrie* base)
        : CClaimTrieCacheExpirationFork(base)
    {
    }

    bool shouldNormalize() const;

    // lower-case and normalize any input string name
    // see: https://unicode.org/reports/tr15/#Norm_Forms
    std::string normalizeClaimName(const std::string& name, bool force = false) const; // public only for validating name field on update op

    bool incrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo,
                        takeoverUndoType& takeovers) override;

    bool decrementBlock(insertUndoType& insertUndo,
                        claimUndoType& expireUndo,
                        insertUndoType& insertSupportUndo,
                        supportUndoType& expireSupportUndo) override;

    bool getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof) override;
    bool getInfoForName(const std::string& name, CClaimValue& claim, int heightOffset = 0) const override;
    CClaimSupportToName getClaimsForName(const std::string& name) const override;
    std::string adjustNameForValidHeight(const std::string& name, int validHeight) const override;

protected:
    int getDelayForName(const std::string& name, const uint160& claimId) const override;

private:
    bool normalizeAllNamesInTrieIfNecessary(bool forward);
};

class CClaimTrieCacheHashFork : public CClaimTrieCacheNormalizationFork
{
public:
    explicit CClaimTrieCacheHashFork(CClaimTrie* base);

    bool getProofForName(const std::string& name, const uint160& finalClaim, CClaimTrieProof& proof) override;
    void initializeIncrement() override;
    bool finalizeDecrement(takeoverUndoType& takeovers) override;

    bool allowSupportMetadata() const;

protected:
    uint256 recursiveComputeMerkleHash(const std::string& name, int takeoverHeight, bool checkOnly) override;
};

typedef CClaimTrieCacheHashFork CClaimTrieCache;

#endif // BITCOIN_CLAIMTRIE_H
