
#ifndef CLAIMTRIE_DATA_H
#define CLAIMTRIE_DATA_H

#include <claimtrie/sqlite/sqlite3.h>
#include <claimtrie/txoutpoint.h>
#include <claimtrie/uints.h>

#include <cstring>
#include <string>
#include <vector>

namespace sqlite
{
    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const CUint160& val) {
        return sqlite3_bind_blob(stmt, inx, val.begin(), int(val.size()), SQLITE_STATIC);
    }

    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const CUint256& val) {
        return sqlite3_bind_blob(stmt, inx, val.begin(), int(val.size()), SQLITE_STATIC);
    }

    inline void store_result_in_db(sqlite3_context* db, const CUint160& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }

    inline void store_result_in_db(sqlite3_context* db, const CUint256& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }
}

#include <claimtrie/sqlite/hdr/sqlite_modern_cpp.h>

namespace sqlite
{
    template<>
    struct has_sqlite_type<CUint256, SQLITE_BLOB, void> : std::true_type {};

    template<>
    struct has_sqlite_type<CUint160, SQLITE_BLOB, void> : std::true_type {};

    inline CUint160 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<CUint160>) {
        CUint160 ret;
        auto ptr = sqlite3_column_blob(stmt, inx);
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes == ret.size());
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }

    inline CUint256 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<CUint256>) {
        CUint256 ret;
        auto ptr = sqlite3_column_blob(stmt, inx);
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes == ret.size());
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }
}

struct CClaimValue
{
    CTxOutPoint outPoint;
    CUint160 claimId;
    int64_t nAmount = 0;
    int64_t nEffectiveAmount = 0;
    int nHeight = 0;
    int nValidAtHeight = 0;

    CClaimValue() = default;
    CClaimValue(CTxOutPoint outPoint, CUint160 claimId, int64_t nAmount, int nHeight, int nValidAtHeight);

    CClaimValue(CClaimValue&&) = default;
    CClaimValue(const CClaimValue&) = default;
    CClaimValue& operator=(CClaimValue&&) = default;
    CClaimValue& operator=(const CClaimValue&) = default;

    bool operator<(const CClaimValue& other) const;
    bool operator==(const CClaimValue& other) const;
    bool operator!=(const CClaimValue& other) const;

    std::string ToString() const;
};

struct CSupportValue
{
    CTxOutPoint outPoint;
    CUint160 supportedClaimId;
    int64_t nAmount = 0;
    int nHeight = 0;
    int nValidAtHeight = 0;

    CSupportValue() = default;
    CSupportValue(CTxOutPoint outPoint, CUint160 supportedClaimId, int64_t nAmount, int nHeight, int nValidAtHeight);

    CSupportValue(CSupportValue&&) = default;
    CSupportValue(const CSupportValue&) = default;
    CSupportValue& operator=(CSupportValue&&) = default;
    CSupportValue& operator=(const CSupportValue&) = default;

    bool operator==(const CSupportValue& other) const;
    bool operator!=(const CSupportValue& other) const;

    std::string ToString() const;
};

typedef std::vector<CClaimValue> claimEntryType;
typedef std::vector<CSupportValue> supportEntryType;

struct CNameOutPointHeightType
{
    std::string name;
    CTxOutPoint outPoint;
    int nValidHeight = 0;

    CNameOutPointHeightType() = default;
    CNameOutPointHeightType(std::string name, CTxOutPoint outPoint, int nValidHeight);
};

struct CClaimIndexElement
{
    std::string name;
    CClaimValue claim;

    CClaimIndexElement() = default;
    CClaimIndexElement(std::string name, CClaimValue claim);
};

#endif // CLAIMTRIE_DATA_H
