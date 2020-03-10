// Copyright (c) 2017-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <clientversion.h>
#include <index/txindex.h>
#include <streams.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>

std::unique_ptr<TxIndex> g_txindex;

/**
 * Access to the txindex database (indexes/txindex/)
 *
 * The database stores a block locator of the chain the database is synced to
 * so that the TxIndex can efficiently determine the point it last stopped at.
 * A locator is used instead of a simple hash of the chain tip because blocks
 * and block index entries may not be flushed to disk until after this database
 * is updated.
 */

TxIndex::TxIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
        : m_db(MakeUnique<BaseIndex::DB>(GetDataDir() / "txindex", n_cache_size, f_memory, f_wipe))
{
    (*m_db) << "CREATE TABLE IF NOT EXISTS tx_to_block ("
               "txID BLOB NOT NULL PRIMARY KEY, "
               "file INTEGER NOT NULL, "
               "blockPos INTEGER NOT NULL, "
               "txPos INTEGER NOT NULL)";

    if (f_wipe)
        (*m_db) << "DELETE FROM tx_to_block";
}

bool TxIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex)
{
    // Exclude genesis block transaction because outputs are not spendable.
    if (pindex->nHeight == 0) return true;

    // transaction is begin and commited in the caller of this method
    auto query = (*m_db) << "INSERT OR REPLACE INTO tx_to_block VALUES(?,?,?,?)";
    auto pos = pindex->GetBlockPos();
    std::size_t offset = ::GetSizeOfCompactSize(block.vtx.size());
    for (const auto& tx : block.vtx) {
        query << tx->GetHash() << pos.nFile << pos.nPos << offset;
        query++;
        offset += ::GetSerializeSize(*tx, CLIENT_VERSION);
    }
    query.used(true);
    return true;
}

bool TxIndex::FindTx(const uint256& tx_hash, uint256& block_hash, CTransactionRef& tx) const
{
    FlatFilePos postx;
    long txPos = 0;
    {
        auto query = (*m_db) << "SELECT file, blockPos, txPos FROM tx_to_block WHERE txID = ?" << tx_hash;
        auto it = query.begin();
        if (it == query.end())
            return false;
        *it >> postx.nFile >> postx.nPos >> txPos;
    }
    CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
    if (file.IsNull()) {
        return error("%s: OpenBlockFile failed", __func__);
    }
    CBlockHeader header;
    try {
        file >> header;
        if (txPos > 0 && fseek(file.Get(), txPos, SEEK_CUR)) {
            return error("%s: fseek(...) failed", __func__);
        }
        file >> tx;
    } catch (const std::exception& e) {
        return error("%s: Deserialize or I/O error - %s", __func__, e.what());
    }
    if (tx->GetHash() != tx_hash) {
        return error("%s: txid mismatch", __func__);
    }
    block_hash = header.GetHash();
    return true;
}