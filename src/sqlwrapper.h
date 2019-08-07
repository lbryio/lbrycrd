// Copyright (c) 2012-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <clientversion.h>
#include <fs.h>
#include <serialize.h>
#include <streams.h>
#include <util.h>
#include <utilstrencodings.h>
#include <version.h>

#include <sqlite3.h>

class CSqlBatch;
class CSqlIterator;

class CSqlWrapper
{
    friend CSqlBatch;
    friend CSqlIterator;
    sqlite3 *pdb;
    mutable CDataStream ssKey;
    mutable CDataStream ssValue;

    void VerifyRC(int rc, int target, int line) const {
        if (rc != target) {
            auto error = sqlite3_errmsg(pdb);
            LogPrintf("Query error %d on line %d: %s\n", rc, line, error);
            assert(rc == target);
        }
    }

public:
    /**
     * @param[in] path        Location in the filesystem where leveldb data will be stored.
     * @param[in] nCacheSize  Configures various leveldb cache settings.
     * @param[in] fMemory     If true, use leveldb's memory environment.
     * @param[in] fWipe       If true, remove all existing data.
     * @param[in] obfuscate   If true, store data obfuscated via simple XOR. If false, XOR
     *                        with a zero'd byte array.
     */
    CSqlWrapper(const fs::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false):
            ssKey(SER_DISK, CLIENT_VERSION), ssValue(SER_DISK, CLIENT_VERSION) {
        int rc = sqlite3_open_v2(fMemory ? ":memory:" : path.c_str(), &pdb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
        if (rc != SQLITE_OK)
        {
            LogPrintf("ERROR: Unable to open %s. Message: %s\n", path, sqlite3_errmsg(pdb));
            assert(rc == SQLITE_OK);
        }
        char* error = nullptr;
        rc = sqlite3_exec(pdb, "CREATE TABLE IF NOT EXISTS kv (key BLOB PRIMARY KEY, value BLOB);", nullptr, nullptr, &error);
        if (rc != SQLITE_OK) {
            LogPrintf("ERROR: Unable to create kv table. Message: %s\n", error);
            sqlite3_free(error);
            sqlite3_close(pdb);
            assert(rc == SQLITE_OK);
        }

        if (fWipe) {
            rc = sqlite3_exec(pdb, "DELETE FROM kv;", nullptr, nullptr, &error);
            if (rc != SQLITE_OK) {
                LogPrintf("ERROR: Unable to drop kv table. Message: %s\n", error);
                sqlite3_free(error);
                sqlite3_close(pdb);
                assert(rc == SQLITE_OK);
            }
        }
        std::string pragmas = "PRAGMA cache_size=-" + std::to_string(nCacheSize >> 10)
                + "; PRAGMA journal_mode=WAL; PRAGMA temp_store=MEMORY; PRAGMA synchronous=NORMAL; PRAGMA wal_autocheckpoint=10000;";
        rc = sqlite3_exec(pdb, pragmas.c_str(), nullptr, nullptr, &error);
        if (rc != SQLITE_OK) {
            LogPrintf("ERROR: Unable to set cache size. Message: %s\n", error);
            sqlite3_free(error);
            sqlite3_close(pdb);
            assert(rc == SQLITE_OK);
        }
    }
    ~CSqlWrapper() {
        sqlite3_close(pdb);
    }

    CSqlWrapper(const CSqlWrapper&) = delete;

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        ssKey << key;

        sqlite3_stmt *stmt = nullptr;
        auto rc = sqlite3_prepare_v2(pdb, "SELECT value FROM kv WHERE key = ?", -1, &stmt, nullptr);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_step(stmt);
        bool ret = false;
        if (rc == SQLITE_ROW) {
            auto blob_size = sqlite3_column_bytes(stmt, 0);
            auto blob = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)); // released on step/finalize
            if (blob_size && blob) {
                try {
                    CDataStream ssValue(blob, blob + blob_size, SER_DISK, CLIENT_VERSION);
                    ssValue >> value;
                    ret = true;
                } catch (const std::exception &) {}
            }
        }
        sqlite3_finalize(stmt);
        ssKey.clear();
        return ret;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        ssKey << key;
        ssValue << value;

        sqlite3_stmt *stmt = nullptr;
        auto rc = sqlite3_prepare_v2(pdb, "REPLACE INTO kv VALUES(?, ?)", -1, &stmt, nullptr);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_bind_blob(stmt, 2, ssValue.data(), ssValue.size(), SQLITE_STATIC);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_step(stmt);
        VerifyRC(rc, SQLITE_DONE, __LINE__);
        auto ret = sqlite3_changes(pdb) > 0;
        sqlite3_finalize(stmt);

        ssKey.clear();
        ssValue.clear();

        return ret;
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        ssKey << key;

        sqlite3_stmt *stmt = nullptr;
        auto rc = sqlite3_prepare_v2(pdb, "SELECT 1 FROM kv WHERE key = ?", -1, &stmt, nullptr);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_step(stmt);
        bool ret = false;
        if (rc == SQLITE_ROW)
            ret = 1 == sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        ssKey.clear();
        return ret;
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        ssKey << key;

        sqlite3_stmt *stmt = nullptr;
        auto rc = sqlite3_prepare_v2(pdb, "DELETE FROM kv WHERE key = ?", -1, &stmt, nullptr);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_bind_blob(stmt, 1, ssKey.data(), ssKey.size(), SQLITE_STATIC);
        VerifyRC(rc, SQLITE_OK, __LINE__);
        rc = sqlite3_step(stmt);
        VerifyRC(rc, SQLITE_DONE, __LINE__);
        auto ret = sqlite3_changes(pdb) > 0;
        sqlite3_finalize(stmt);
        ssKey.clear();
        if (fSync) Sync();
        return ret;
    }

    bool WriteBatch(CSqlBatch& batch, bool fSync = false) {
        int rc = sqlite3_exec(pdb, "COMMIT TRANSACTION;", nullptr, nullptr, nullptr);
        if (rc == SQLITE_OK) {
            if (fSync)
                Sync();
            return true;
        }
        return false;
    }

    // not available for LevelDB; provide for compatibility with BDB
    bool Flush()
    {
        return true;
    }

    bool Sync()
    {
        auto rc = sqlite3_wal_checkpoint_v2(pdb, nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
        return rc == SQLITE_OK;
    }

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty() {
        int64_t count = -1;
        static auto cb = [](void* state, int argc, char** argv, char** cols) {
            auto data = reinterpret_cast<int64_t*>(state);
            if (argc)
                *data = std::atoll(argv[0]);
            return SQLITE_OK;
        };
        int rc = sqlite3_exec(pdb, "SELECT COUNT(*) FROM kv", cb, &count, nullptr);
        assert(rc == SQLITE_OK);
        return count == 0;
    }
};

/** Batch of changes queued to be written to a CDBWrapper */
class CSqlBatch
{
    friend class CSqlWrapper;

private:
    CSqlWrapper &parent;

public:
    /**
     * @param[in] _parent   CDBWrapper that this batch is to be submitted to
     */
    explicit CSqlBatch(CSqlWrapper &_parent) : parent(_parent) {
        int rc = sqlite3_exec(parent.pdb, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        parent.VerifyRC(rc, SQLITE_OK, __LINE__);
    };

    void Clear()
    {
        int rc = sqlite3_exec(parent.pdb, "ROLLBACK TRANSACTION;", nullptr, nullptr, nullptr);
        parent.VerifyRC(rc, SQLITE_OK, __LINE__);
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        auto ret = parent.Write(key, value, false);
        assert(ret);
    }

    template <typename K>
    void Erase(const K& key)
    {
        parent.Erase(key, false);
    }
};

class CSqlIterator
{
private:
    const CSqlWrapper &parent;
    sqlite3_stmt *stmt;
    int last_step;

public:

    /**
     * @param[in] _parent          Parent CDBWrapper instance.
     * @param[in] _piter           The original leveldb iterator.
     */
    CSqlIterator(const CSqlWrapper &_parent) :
            parent(_parent) {
        stmt = nullptr;
        auto rc = sqlite3_prepare_v2(parent.pdb, "SELECT * FROM kv;", -1, &stmt, nullptr);
        parent.VerifyRC(rc, SQLITE_OK, __LINE__);
    };

    ~CSqlIterator() {
        last_step = SQLITE_DONE;
        sqlite3_finalize(stmt);
    }

    bool Valid() const { return last_step == SQLITE_ROW; }

    void SeekToFirst() { auto rc = sqlite3_reset(stmt); assert(rc == SQLITE_OK); Next(); }

    void Next() { last_step = sqlite3_step(stmt); }

    template<typename K> bool GetKey(K& key) {
        if (last_step != SQLITE_ROW)
            return false;

        auto blob_size = sqlite3_column_bytes(stmt, 0);
        auto blob = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)); // released on step/finalize
        if (blob_size && blob) {
            try {
                CDataStream ssKey(blob, blob + blob_size, SER_DISK, CLIENT_VERSION);
                ssKey >> key;
                return true;
            } catch (const std::exception &) {}
        }
        return false;
    }

    template<typename V> bool GetValue(V& value) {
        if (last_step != SQLITE_ROW)
            return false;

        auto blob_size = sqlite3_column_bytes(stmt, 1);
        auto blob = reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)); // released on step/finalize
        if (blob_size && blob) {
            try {
                CDataStream ssValue(blob, blob + blob_size, SER_DISK, CLIENT_VERSION);
                ssValue >> value;
                return true;
            } catch (const std::exception &) {}
        }
        return false;
    }

    int GetValueSize() {
        if (last_step != SQLITE_ROW)
            return -1;
        return sqlite3_column_bytes(stmt, 1);
    }
};

