// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <claimtrie/trie.h>
#include <index/base.h>
#include <shutdown.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>
#include <warnings.h>

#include <chrono>

constexpr int64_t SYNC_LOG_INTERVAL = 30; // seconds
constexpr int64_t SYNC_LOCATOR_WRITE_INTERVAL = 30; // seconds

template<typename... Args>
static void FatalError(const char* fmt, const Args&... args)
{
    std::string strMessage = tfm::format(fmt, args...);
    SetMiscWarning(strMessage);
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        "Error: A fatal internal error occurred, see debug.log for details",
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
}

static const sqlite::sqlite_config sharedConfig {
    sqlite::OpenFlags::READWRITE | sqlite::OpenFlags::CREATE,
    nullptr, sqlite::Encoding::UTF8
};

BaseIndex::DB::DB(const fs::path& path, size_t n_cache_size, bool fMemory, bool fWipe)
    : sqlite::database(fMemory ? ":memory:" : path.string() + ".sqlite", sharedConfig)
{
    applyPragmas(*this, n_cache_size >> 10); // in -KB

    (*this) << "CREATE TABLE IF NOT EXISTS locator (branch BLOB NOT NULL COLLATE BINARY);";

    if (fWipe) {
        (*this) << "DELETE FROM locator";
    }
}

bool BaseIndex::DB::ReadBestBlock(CBlockLocator& locator) const
{
    locator.SetNull();
    bool success = false;
    auto& branches = locator.vHave;
    for (auto&& row : (*this) << "SELECT branch FROM locator") {
        row >> *branches.insert(branches.end(), uint256{});
        success = true;
    }
    return success;
}

bool BaseIndex::DB::WriteBestBlock(const CBlockLocator& locator)
{
    (*this) << "DELETE FROM locator";
    for (auto& branch : locator.vHave)
        (*this) << "INSERT INTO locator VALUES(?)" << branch;
    return (*this).rows_modified() > 0;
}

BaseIndex::~BaseIndex()
{
    Interrupt();
    Stop();
}

bool BaseIndex::Init()
{
    CBlockLocator locator;
    if (!GetDB().ReadBestBlock(locator)) {
        locator.SetNull();
    }

    LOCK(cs_main);
    if (locator.IsNull()) {
        m_best_block_index = nullptr;
    } else {
        m_best_block_index = FindForkInGlobalIndex(::ChainActive(), locator);
    }
    m_synced = m_best_block_index.load() == ::ChainActive().Tip();
    return true;
}

static const CBlockIndex* NextSyncBlock(const CBlockIndex* pindex_prev) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    AssertLockHeld(cs_main);

    if (!pindex_prev) {
        return ::ChainActive().Genesis();
    }

    const CBlockIndex* pindex = ::ChainActive().Next(pindex_prev);
    if (pindex) {
        return pindex;
    }

    return ::ChainActive().Next(::ChainActive().FindFork(pindex_prev));
}

void BaseIndex::ThreadSync()
{
    auto start = std::chrono::high_resolution_clock::now();
    const CBlockIndex* pindex = m_best_block_index.load();
    if (!m_synced) {
        auto& consensus_params = Params().GetConsensus();
        GetDB() << "BEGIN";

        int64_t last_log_time = 0;
        int64_t last_locator_write_time = 0;
        while (true) {
            if (m_interrupt) {
                m_best_block_index = pindex;
                // No need to handle errors in Commit. If it fails, the error will be already be
                // logged. The best way to recover is to continue, as index cannot be corrupted by
                // a missed commit to disk for an advanced index state.
                Commit(true);
                return;
            }

            {
                LOCK(cs_main);
                const CBlockIndex* pindex_next = NextSyncBlock(pindex);
                if (!pindex_next) {
                    m_best_block_index = pindex;
                    m_synced = true;
                    // No need to handle errors in Commit. See rationale above.
                    Commit(true);
                    break;
                }
                if (pindex_next->pprev != pindex && !Rewind(pindex, pindex_next->pprev)) {
                    GetDB() << "ROLLBACK";
                    FatalError("%s: Failed to rewind index %s to a previous chain tip",
                               __func__, GetName());
                    return;
                }
                pindex = pindex_next;
            }

            int64_t current_time = GetTime();
            if (last_log_time + SYNC_LOG_INTERVAL < current_time) {
                LogPrintf("Syncing %s with block chain from height %d\n",
                          GetName(), pindex->nHeight);
                last_log_time = current_time;
            }

            if (last_locator_write_time + SYNC_LOCATOR_WRITE_INTERVAL < current_time) {
                m_best_block_index = pindex;
                last_locator_write_time = current_time;
                // No need to handle errors in Commit. See rationale above.
                Commit();
                GetDB() << "BEGIN";
            }

            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, consensus_params)) {
                GetDB() << "ROLLBACK";
                FatalError("%s: Failed to read block %s from disk",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
            if (!WriteBlock(block, pindex)) {
                GetDB() << "ROLLBACK";
                FatalError("%s: Failed to write block %s to index database",
                           __func__, pindex->GetBlockHash().ToString());
                return;
            }
        }
    }

    auto done = 1e-9 * std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
    if (pindex) {
        LogPrintf("%s is enabled at height %d. Indexing time: %f sec.\n", GetName(), pindex->nHeight, done);
    } else {
        LogPrintf("%s is enabled. Indexing time: %f sec.\n", GetName(), done);
    }
}

bool BaseIndex::Commit(bool syncToDisk)
{
    if (!CommitInternal() || sqlite::commit(GetDB()) != SQLITE_OK) {
        GetDB() << "ROLLBACK";
        return error("%s: Failed to commit latest %s state", __func__, GetName());
    }
    if (syncToDisk) {
        if (sqlite::sync(GetDB()) != SQLITE_OK)
            return error("%s: Unable to sync to disk", __func__);
    }
    return true;
}

bool BaseIndex::CommitInternal()
{
    CBlockLocator locator;
    {
        LOCK(cs_main);
        locator = ::ChainActive().GetLocator(m_best_block_index);
    }
    return GetDB().WriteBestBlock(locator);
}

bool BaseIndex::Rewind(const CBlockIndex* current_tip, const CBlockIndex* new_tip)
{
    assert(current_tip == m_best_block_index);
    assert(current_tip->GetAncestor(new_tip->nHeight) == new_tip);

    // In the case of a reorg, ensure persisted block locator is not stale.
    m_best_block_index = new_tip;
    if (!CommitInternal()) {
        // If commit fails, revert the best block index to avoid corruption.
        m_best_block_index = current_tip;
        return false;
    }

    return true;
}

void BaseIndex::BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex,
                               const std::vector<CTransactionRef>& txn_conflicted)
{
    if (!m_synced) {
        return;
    }

    const CBlockIndex* best_block_index = m_best_block_index.load();
    if (!best_block_index) {
        if (pindex->nHeight != 0) {
            FatalError("%s: First block connected is not the genesis block (height=%d)",
                       __func__, pindex->nHeight);
            return;
        }
    } else {
        // Ensure block connects to an ancestor of the current best block. This should be the case
        // most of the time, but may not be immediately after the sync thread catches up and sets
        // m_synced. Consider the case where there is a reorg and the blocks on the stale branch are
        // in the ValidationInterface queue backlog even after the sync thread has caught up to the
        // new chain tip. In this unlikely event, log a warning and let the queue clear.
        if (best_block_index->GetAncestor(pindex->nHeight - 1) != pindex->pprev) {
            LogPrintf("%s: WARNING: Block %s does not connect to an ancestor of " /* Continued */
                      "known best chain (tip=%s); not updating index\n",
                      __func__, pindex->GetBlockHash().ToString(),
                      best_block_index->GetBlockHash().ToString());
            return;
        }
        if (best_block_index != pindex->pprev && !Rewind(best_block_index, pindex->pprev)) {
            FatalError("%s: Failed to rewind index %s to a previous chain tip",
                       __func__, GetName());
            return;
        }
    }

    GetDB() << "BEGIN";
    if (WriteBlock(*block, pindex)) {
        Commit(true);
        m_best_block_index = pindex;
    } else {
        GetDB() << "ROLLBACK";
        FatalError("%s: Failed to write block %s to index",
                   __func__, pindex->GetBlockHash().ToString());
        return;
    }
}

bool BaseIndex::BlockUntilSyncedToCurrentChain()
{
    AssertLockNotHeld(cs_main);

    if (!m_synced) {
        return false;
    }

    {
        // Skip the queue-draining stuff if we know we're caught up with
        // ::ChainActive().Tip().
        LOCK(cs_main);
        const CBlockIndex* chain_tip = ::ChainActive().Tip();
        const CBlockIndex* best_block_index = m_best_block_index.load();
        if (best_block_index->GetAncestor(chain_tip->nHeight) == chain_tip) {
            return true;
        }
    }

    LogPrintf("%s: %s is catching up on block notifications\n", __func__, GetName());
    SyncWithValidationInterfaceQueue();
    return true;
}

void BaseIndex::Interrupt()
{
    m_interrupt();
}

void BaseIndex::Start()
{
    // Need to register this ValidationInterface before running Init(), so that
    // callbacks are not missed if Init sets m_synced to true.
    RegisterValidationInterface(this);
    if (!Init()) {
        FatalError("%s: %s failed to initialize", __func__, GetName());
        return;
    }

    m_thread_sync = std::thread(&TraceThread<std::function<void()>>, GetName(),
                                std::bind(&BaseIndex::ThreadSync, this));
}

void BaseIndex::Stop()
{
    UnregisterValidationInterface(this);

    if (m_thread_sync.joinable()) {
        m_thread_sync.join();
    }
}
