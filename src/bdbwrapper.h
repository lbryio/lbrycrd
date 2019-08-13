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

#include <db_cxx.h>

class CBdbBatch;
class CBdbIterator;

class CBdbWrapper
{
    friend CBdbBatch;
    friend CBdbIterator;
    std::unique_ptr<DbEnv> pdbenv;
    std::unique_ptr<Db> pdb;
    mutable CDataStream ssKey;
    mutable CDataStream ssValue;

    mutable std::size_t valueSize;
    mutable std::unique_ptr<char[]> valueData;


    void VerifyRC(int rc, int target, int line) const {
        if (rc != target) {
            auto error = DbEnv::strerror(rc);
            LogPrintf("BDB error %d on line %d: %s\n", rc, line, error);
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
    CBdbWrapper(const fs::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false):
            ssKey(SER_DISK, CLIENT_VERSION), ssValue(SER_DISK, CLIENT_VERSION), valueSize(1024), valueData(new char[valueSize]) {

        if (!fMemory)
            TryCreateDirectories(path);

        pdbenv.reset(new DbEnv(DB_CXX_NO_EXCEPTIONS));
        pdbenv->set_cachesize(0, nCacheSize, 1);

        pdbenv->set_flags(DB_TXN_WRITE_NOSYNC, 1);
        pdbenv->set_flags(DB_TXN_NOSYNC, 1);
        pdbenv->set_flags(DB_AUTO_COMMIT, 0);
        pdbenv->log_set_config(DB_LOG_AUTO_REMOVE, 1);

        if (fMemory) {
            pdbenv->log_set_config(DB_LOG_IN_MEMORY, 1);
            pdbenv->set_lg_bsize(nCacheSize);
        }
        else {
            auto logPath = path / "log";
            TryCreateDirectories(logPath);
            pdbenv->set_lg_dir(logPath.c_str());
            pdbenv->set_lg_bsize(2 * 1024 * 1024);
            pdbenv->set_lg_max(20 * 1024 * 1024);
        }

        auto rc = pdbenv->open(fMemory ? nullptr : path.c_str(), DB_CREATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_RECOVER | DB_PRIVATE, S_IRUSR | S_IWUSR);
        VerifyRC(rc, 0, __LINE__);

        pdb.reset(new Db(pdbenv.get(), 0));
        DbTxn* openTxn;
        pdbenv->txn_begin(nullptr, &openTxn, 0); // really weird that it requires a txn on open in order to support them later
        rc = pdb->open(openTxn, fMemory ? nullptr : "primary.db", nullptr, DB_BTREE, DB_CREATE, 0); // | bdb.DB_CHKSUM
        VerifyRC(rc, 0, __LINE__);

        if (fMemory) {
            auto mpf = pdb->get_mpf();
            rc = mpf->set_flags(DB_MPOOL_NOFILE, 1);
            VerifyRC(rc, 0, __LINE__);
        }

        if (fWipe) {
            uint32_t count;
            rc = pdb->truncate(openTxn, &count, 0); // txn, result count, flags
            VerifyRC(rc, 0, __LINE__);
        }
        openTxn->commit(0);
        // assert(!fWipe || IsEmpty());
    }
    ~CBdbWrapper() {
        pdb->close(0);
        pdb.reset(); // ensure db goes first
        pdbenv->close(0);
    }

    CBdbWrapper(const CBdbWrapper&) = delete;

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        ssKey << key;

        Dbt datKey(ssKey.data(), ssKey.size());

        READ_AGAIN:
        Dbt datVal(valueData.get(), 0);
        datVal.set_flags(DB_DBT_USERMEM);
        datVal.set_ulen(valueSize);

        auto rc = pdb->get(nullptr, &datKey, &datVal, 0);
        if (rc == DB_BUFFER_SMALL) {
            valueSize *= 2;
            valueData.reset(new char[valueSize]);
            goto READ_AGAIN;
        }
        ssKey.clear();

        if (rc == 0) {
            try {
                CDataStream ssValue(valueData.get(), valueData.get() + datVal.get_size(), SER_DISK, CLIENT_VERSION);
                ssValue >> value;
            }
            catch (const std::exception &) {
                return false;
            }
            return true;
        }

        if (rc == DB_NOTFOUND)
            return false;

        VerifyRC(rc, 0, __LINE__);
        return false;
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        ssKey << key;
        Dbt datKey(ssKey.data(), ssKey.size());

        auto rc = pdb->exists(nullptr, &datKey, 0);
        ssKey.clear();
        return rc == 0;
    }

    bool Sync()
    {
        auto rc = pdbenv->txn_checkpoint(0, 0, DB_FORCE);
        return rc == 0;
    }

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty() {
        DB_HASH_STAT stat;
        auto rc = pdb->stat(nullptr, &stat, DB_FAST_STAT);
        return rc == 0 && stat.hash_nkeys == 0;
    }
};

/** Batch of changes queued to be written to a CDBWrapper */
class CBdbBatch
{
    friend class CBdbWrapper;

private:
    CBdbWrapper &parent;
    DbTxn* txn;

public:
    /**
     * @param[in] _parent   CDBWrapper that this batch is to be submitted to
     */
    explicit CBdbBatch(CBdbWrapper &_parent) : parent(_parent), txn(nullptr) {
        auto rc = _parent.pdbenv->txn_begin(nullptr, &txn, DB_TXN_WRITE_NOSYNC);
        parent.VerifyRC(rc, 0, __LINE__);
    };

    ~CBdbBatch() {
        assert(txn == nullptr);
    }

    void Commit(bool fSync = false) {
        auto rc = txn->commit(0);
        parent.VerifyRC(rc, 0, __LINE__);
        txn = nullptr;
        if (fSync)
            parent.Sync();
    }

    void Clear()
    {
        txn->abort();
        txn = nullptr;
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        parent.ssKey << key;
        parent.ssValue << value;

        Dbt datKey(parent.ssKey.data(), parent.ssKey.size());
        Dbt datVal(parent.ssValue.data(), parent.ssValue.size());

        auto rc = parent.pdb->put(txn, &datKey, &datVal, 0); // DB_NOOVERWRITE
        parent.ssKey.clear();
        parent.ssValue.clear();

        parent.VerifyRC(rc, 0, __LINE__);
    }

    template <typename K>
    void Erase(const K& key)
    {
        parent.ssKey << key;
        Dbt datKey(parent.ssKey.data(), parent.ssKey.size());

        auto rc = parent.pdb->del(txn, &datKey, 0);
        parent.ssKey.clear();
    }
};

class CBdbIterator
{
private:
    const CBdbWrapper &parent;
    Dbc* cursor;

    std::size_t keySize, actualKeySize;
    std::unique_ptr<char[]> keyData;

    std::size_t valueSize, actualValueSize;
    std::unique_ptr<char[]> valueData;

    void Jump(uint32_t flag) {
        Dbt key(keyData.get(), 0);
        key.set_flags(DB_DBT_USERMEM);
        key.set_ulen(keySize);
        Dbt value(valueData.get(), 0);
        value.set_flags(DB_DBT_USERMEM);
        value.set_ulen(valueSize);

        auto rc = cursor->get(&key, &value, flag);
        if (rc == DB_BUFFER_SMALL) {
            valueSize *= 2;
            valueData.reset(new char[valueSize]);
            Jump(flag);
            return;
        }
        if (rc == 0) {
            actualKeySize = key.get_size();
            actualValueSize = value.get_size();
        }
        else {
            actualKeySize = 0;
            actualValueSize = 0;
        }
    }

public:

    /**
     * @param[in] _parent          Parent CDBWrapper instance.
     * @param[in] _piter           The original leveldb iterator.
     */
    CBdbIterator(const CBdbWrapper &_parent) : parent(_parent),
                                               keySize(512), keyData(new char[keySize]), actualKeySize(0),
                                               valueSize(1024), valueData(new char[1024]), actualValueSize(0) {
        auto rc = parent.pdb->cursor(nullptr, &cursor, 0);
        parent.VerifyRC(rc, 0, __LINE__);
    }

    ~CBdbIterator() {
        cursor->close();
    }

    bool Valid() const { return actualKeySize > 0; }

    void SeekToFirst() { Jump(DB_FIRST); }

    void Next() { Jump(DB_NEXT); }

    template<typename K> bool GetKey(K& key) {
        if (actualKeySize) {
            try {
                CDataStream retKey(keyData.get(), keyData.get() + actualKeySize, SER_DISK, CLIENT_VERSION);
                retKey >> key;
                return true;
            } catch (const std::exception &) {}
        }
        return false;
    }

    template<typename V> bool GetValue(V& value) {
        if (actualValueSize) {
            try {
                CDataStream retValue(valueData.get(), valueData.get() + actualValueSize, SER_DISK, CLIENT_VERSION);
                retValue >> value;
                return true;
            } catch (const std::exception &) {}
        }
        return false;
    }

    int GetValueSize() {
        return int(actualValueSize);
    }
};