// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include <coins.h>
#include <chain.h>
#include <primitives/block.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <sqlite/sqlite3.h>

namespace sqlite {
    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const CScript& val) {
        return sqlite3_bind_blob(stmt, inx, val.data(), int(val.size()), SQLITE_STATIC);
    }

    inline void store_result_in_db(sqlite3_context* db, const CScript& val) {
        sqlite3_result_blob(db, val.data(), int(val.size()), SQLITE_TRANSIENT);
    }

    inline int bind_col_in_db(sqlite3_stmt* stmt, int inx, const uint256& val) {
        return sqlite3_bind_blob(stmt, inx, val.begin(), int(val.size()), SQLITE_STATIC);
    }

    inline void store_result_in_db(sqlite3_context* db, const uint256& val) {
        sqlite3_result_blob(db, val.begin(), int(val.size()), SQLITE_TRANSIENT);
    }
}
#include <sqlite.h>
namespace sqlite {
    template<>
    struct has_sqlite_type<CScript, SQLITE_BLOB, void> : std::true_type {};

    inline CScript get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<CScript>) {
        auto ptr = (const unsigned char*)sqlite3_column_blob(stmt, inx);
        if (!ptr) return {};
        int bytes = sqlite3_column_bytes(stmt, inx);
        return CScript(ptr, ptr + bytes);
    }

    template<>
    struct has_sqlite_type<uint256, SQLITE_BLOB, void> : std::true_type {};

    inline uint256 get_col_from_db(sqlite3_stmt* stmt, int inx, result_type<uint256>) {
        auto type = sqlite3_column_type(stmt, inx);
        if (type == SQLITE_NULL)
            return {};
        if (type == SQLITE_INTEGER)
            return ArithToUint256(arith_uint256(sqlite3_column_int64(stmt, inx)));

        assert(type == SQLITE_BLOB);
        auto ptr = sqlite3_column_blob(stmt, inx);
        uint256 ret;
        if (!ptr) return ret;
        int bytes = sqlite3_column_bytes(stmt, inx);
        assert(bytes <= ret.size());
        std::memcpy(ret.begin(), ptr, bytes);
        return ret;
    }
}

class CBlockIndex;
class CCoinsViewDBCursor;
class uint256;

//! No need to periodic flush if at least this much space still available.
static constexpr int MAX_BLOCK_COINSDB_USAGE = 10;
//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 480;
//! -dbbatchsize default (bytes)
static const int64_t nDefaultDbBatchSize = 16 << 20;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache
static const int64_t nMaxBlockDBCache = 260;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 40;


struct CDiskTxPos : public CDiskBlockPos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(CDiskBlockPos, *this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB final : public CCoinsView
{
    friend CCoinsViewDBCursor;
    mutable sqlite::database db;

public:
    explicit CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock, bool sync) override;
    CCoinsViewCursor *Cursor() const override;
    size_t EstimateSize() const override;
};

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    ~CCoinsViewDBCursor() noexcept override {}

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;

    bool Valid() const override;
    void Next() override;

private:
    friend CCoinsViewDB;
    explicit CCoinsViewDBCursor(const uint256 &hashBlockIn, const CCoinsViewDB* view);
    const CCoinsViewDB* owner;
    mutable sqlite::database_binder query;
    mutable sqlite::row_iterator iter;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB
{
    sqlite::database db;

public:
    explicit CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool BatchWrite(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo,
                    int nLastFile, const std::vector<const CBlockIndex*>& blockInfo, bool sync);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &info);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindexing);
    void ReadReindexing(bool &fReindexing);
    bool ReadTxIndex(const uint256 &txid, CDiskTxPos &pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos>> &list);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&)> insertBlockIndex);
};

#endif // BITCOIN_TXDB_H
