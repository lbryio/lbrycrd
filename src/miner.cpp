// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "main.h"
#include "nameclaim.h"
#include "claimtrie.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <queue>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

class ScoreCompare
{
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b)
    {
        return CompareTxMemPoolEntryByScore()(*b,*a); // Convert to less than
    }
};

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    return nNewTime - nOldTime;
}

CBlockTemplate* CreateNewBlock(const CChainParams& chainparams, const CScript& scriptPubKeyIn)
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to between 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CTxMemPool::setEntries inBlock;
    CTxMemPool::setEntries waitSet;

    // This vector will be sorted into a priority queue:
    vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    std::priority_queue<CTxMemPool::txiter, std::vector<CTxMemPool::txiter>, ScoreCompare> clearedTxs;
    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    uint64_t nBlockSize = 1000;
    uint64_t nBlockTx = 0;
    unsigned int nBlockSigOps = 100;
    int lastFewTxs = 0;
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        pblock->nTime = GetAdjustedTime();
        CCoinsViewCache view(pcoinsTip);
        if (!pclaimTrie)
        {
            return NULL;
        }
        CClaimTrieUpdateBuffer trieCache(pclaimTrie);
        const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

        pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
        // -regtest only: allow overriding block.nVersion with
        // -blockversion=N to test forking scenarios
        if (chainparams.MineBlocksOnDemand())
            pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

        int64_t nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                                ? nMedianTimePast
                                : pblock->GetBlockTime();

        bool fPriorityBlock = nBlockPrioritySize > 0;
        if (fPriorityBlock) {
            vecPriority.reserve(mempool.mapTx.size());
            for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
                 mi != mempool.mapTx.end(); ++mi)
            {
                double dPriority = mi->GetPriority(nHeight);
                CAmount dummy;
                mempool.ApplyDeltas(mi->GetTx().GetHash(), dPriority, dummy);
                vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
            }
            std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        }

        CTxMemPool::indexed_transaction_set::index<mining_score>::type::iterator mi = mempool.mapTx.get<mining_score>().begin();
        CTxMemPool::txiter iter;

        while (mi != mempool.mapTx.get<mining_score>().end() || !clearedTxs.empty())
        {
            bool priorityTx = false;
            if (fPriorityBlock && !vecPriority.empty()) { // add a tx from priority queue to fill the blockprioritysize
                priorityTx = true;
                iter = vecPriority.front().second;
                actualPriority = vecPriority.front().first;
                std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                vecPriority.pop_back();
            }
            else if (clearedTxs.empty()) { // add tx with next highest score
                iter = mempool.mapTx.project<0>(mi);
                mi++;
            }
            else {  // try to add a previously postponed child tx
                iter = clearedTxs.top();
                clearedTxs.pop();
            }

            if (inBlock.count(iter))
            {
                continue; // could have been added to the priorityBlock
            }

            const CTransaction& tx = iter->GetTx();

            bool fOrphan = false;
            BOOST_FOREACH(CTxMemPool::txiter parent, mempool.GetMemPoolParents(iter))
            {
                if (!inBlock.count(parent)) {
                    fOrphan = true;
                    break;
                }
            }
            if (fOrphan) {
                if (priorityTx)
                    waitPriMap.insert(std::make_pair(iter,actualPriority));
                else
                    waitSet.insert(iter);
                continue;
            }

            unsigned int nTxSize = iter->GetTxSize();
            if (fPriorityBlock &&
                (nBlockSize + nTxSize >= nBlockPrioritySize || !AllowFree(actualPriority))) {
                fPriorityBlock = false;
                waitPriMap.clear();
            }
            if (!priorityTx &&
                (iter->GetModifiedFee() < ::minRelayTxFee.GetFee(nTxSize) && nBlockSize >= nBlockMinSize)) {
                break;
            }
            if (nBlockSize + nTxSize >= nBlockMaxSize) {
                if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
                    break;
                }
                // Once we're within 1000 bytes of a full block, only look at 50 more txs
                // to try to fill the remaining space.
                if (nBlockSize > nBlockMaxSize - 1000) {
                    lastFewTxs++;
                }
                continue;
            }

            if (!IsFinalTx(tx, nHeight, nLockTimeCutoff))
                continue;

            typedef std::vector<std::pair<std::string, uint160> > spentClaimsType;
            spentClaimsType spentClaims;

            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                int nTxinHeight = 0;
                CScript scriptPubKey;
                bool fGotCoins = false;
                if (coins)
                {
                    if (txin.prevout.n < coins->vout.size())
                    {
                        nTxinHeight = coins->nHeight;
                        scriptPubKey = coins->vout[txin.prevout.n].scriptPubKey;
                        fGotCoins = true;
                    }
                }
                else // must be in block or else
                {
                    BOOST_FOREACH(CTxMemPool::txiter inBlockEntry, inBlock)
                    {
                        CTransaction inBlockTx = inBlockEntry->GetTx();
                        if (inBlockTx.GetHash() == txin.prevout.hash)
                        {
                            if (inBlockTx.vout.size() >= txin.prevout.n)
                            {
                                nTxinHeight = nHeight;
                                scriptPubKey = inBlockTx.vout[txin.prevout.n].scriptPubKey;
                                fGotCoins = true;
                            }
                        }
                    }
                }
                if (!fGotCoins)
                {
                    LogPrintf("Tried to include a transaction but could not find the txout it was spending. This is bad. Please send this log file to the maintainers of this program.\n");
                    throw std::runtime_error("Tried to include a transaction but could not find the txout it was spending.");
                }

                std::vector<std::vector<unsigned char> > vvchParams;
                int op;

                if (DecodeClaimScript(scriptPubKey, op, vvchParams))
                {
                    if (op == OP_CLAIM_NAME || op == OP_UPDATE_CLAIM)
                    {
                        uint160 claimId;
                        if (op == OP_CLAIM_NAME)
                        {
                            assert(vvchParams.size() == 2);
                            claimId = ClaimIdHash(txin.prevout.hash, txin.prevout.n);
                        }
                        else if (op == OP_UPDATE_CLAIM)
                        {
                            assert(vvchParams.size() == 3);
                            claimId = uint160(vvchParams[1]);
                        }
                        std::string name(vvchParams[0].begin(), vvchParams[0].end());
                        int throwaway;
                        if (trieCache.spendClaim(name, COutPoint(txin.prevout.hash, txin.prevout.n), nTxinHeight, throwaway))
                        {
                            std::pair<std::string, uint160> entry(name, claimId);
                            spentClaims.push_back(entry);
                        }
                        else
                        {
                            LogPrintf("%s(): The claim was not found in the trie or queue and therefore can't be updated\n", __func__);
                        }
                    }
                    else if (op == OP_SUPPORT_CLAIM)
                    {
                        assert(vvchParams.size() == 2);
                        std::string name(vvchParams[0].begin(), vvchParams[0].end());
                        int throwaway;
                        if (!trieCache.spendSupport(name, COutPoint(txin.prevout.hash, txin.prevout.n), nTxinHeight, throwaway))
                        {
                            LogPrintf("%s(): The support was not found in the trie or queue\n", __func__);
                        }
                    }
                }
            }
            
            for (unsigned int i = 0; i < tx.vout.size(); ++i)
            {
                const CTxOut& txout = tx.vout[i];
            
                std::vector<std::vector<unsigned char> > vvchParams;
                int op;
                if (DecodeClaimScript(txout.scriptPubKey, op, vvchParams))
                {
                    if (op == OP_CLAIM_NAME)
                    {
                        assert(vvchParams.size() == 2);
                        std::string name(vvchParams[0].begin(), vvchParams[0].end());
                        if (!trieCache.addClaim(name, COutPoint(tx.GetHash(), i), ClaimIdHash(tx.GetHash(), i), txout.nValue, nHeight))
                        {
                            LogPrintf("%s: Something went wrong inserting the name\n", __func__);
                        }
                    }
                    else if (op == OP_UPDATE_CLAIM)
                    {
                        assert(vvchParams.size() == 3);
                        std::string name(vvchParams[0].begin(), vvchParams[0].end());
                        uint160 claimId(vvchParams[1]);
                        spentClaimsType::iterator itSpent;
                        for (itSpent = spentClaims.begin(); itSpent != spentClaims.end(); ++itSpent)
                        {
                            if (itSpent->first == name && itSpent->second == claimId)
                            {
                                break;
                            }
                        }
                        if (itSpent != spentClaims.end())
                        {
                            spentClaims.erase(itSpent);
                            if (!trieCache.addClaim(name, COutPoint(tx.GetHash(), i), claimId, txout.nValue, nHeight))
                            {
                                LogPrintf("%s: Something went wrong updating a claim\n", __func__);
                            }
                        }
                        else
                        {
                            LogPrintf("%s(): This update refers to a claim that was not found in the trie or queue, and therefore cannot be updated. The claim may have expired or it may have never existed.\n", __func__);
                        }
                    }
                    else if (op == OP_SUPPORT_CLAIM)
                    {
                        assert(vvchParams.size() == 2);
                        std::string name(vvchParams[0].begin(), vvchParams[0].end());
                        uint160 supportedClaimId(vvchParams[1]);
                        if (!trieCache.addSupport(name, COutPoint(tx.GetHash(), i), txout.nValue, supportedClaimId, nHeight))
                        {
                            LogPrintf("%s: Something went wrong inserting the claim support\n", __func__);
                        }
                    }
                }
            }
            CValidationState state;

            unsigned int nTxSigOps = iter->GetSigOpCount();
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS) {
                if (nBlockSigOps > MAX_BLOCK_SIGOPS - 2) {
                    break;
                }
                continue;
            }

            CAmount nTxFees = iter->GetFee();
            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority)
            {
                double dPriority = iter->GetPriority(nHeight);
                CAmount dummy;
                mempool.ApplyDeltas(tx.GetHash(), dPriority, dummy);
                LogPrintf("priority %.1f fee %s txid %s\n",
                          dPriority , CFeeRate(iter->GetModifiedFee(), nTxSize).ToString(), tx.GetHash().ToString());
            }

            inBlock.insert(iter);

            // Add transactions that depend on this one to the priority queue
            BOOST_FOREACH(CTxMemPool::txiter child, mempool.GetMemPoolChildren(iter))
            {
                if (fPriorityBlock) {
                    waitPriIter wpiter = waitPriMap.find(child);
                    if (wpiter != waitPriMap.end()) {
                        vecPriority.push_back(TxCoinAgePriority(wpiter->second,child));
                        std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                        waitPriMap.erase(wpiter);
                    }
                }
                else {
                    if (waitSet.count(child)) {
                        clearedTxs.push(child);
                        waitSet.erase(child);
                    }
                }
            }
        }
        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u txs: %u fees: %ld sigops %d\n", nBlockSize, nBlockTx, nFees, nBlockSigOps);

        // Compute final coinbase transaction.
        txNew.vout[0].nValue = nFees + GetBlockSubsidy(nHeight, chainparams.GetConsensus());
        txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;
        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -nFees;

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
        pblock->nNonce         = 0;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);
        insertUndoType dummyInsertUndo;
        claimQueueRowType dummyExpireUndo;
        insertUndoType dummyInsertSupportUndo;
        supportQueueRowType dummyExpireSupportUndo;
        std::vector<std::pair<std::string, int> > dummyTakeoverHeightUndo;
        trieCache.incrementBlock(dummyInsertUndo, dummyExpireUndo, dummyInsertSupportUndo, dummyExpireSupportUndo, dummyTakeoverHeightUndo);
        pblock->hashClaimTrie = trieCache.getMerkleHash();

        CValidationState state;
        if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
            throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, FormatStateMessage(state)));
        }
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

static bool ProcessBlockFound(const CBlock* pblock, const CChainParams& chainparams)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitcoinMiner: generated block is stale");
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, Params(), NULL, pblock, true, NULL))
        return error("BitcoinMiner: ProcessNewBlock, block not accepted");

    return true;
}

void static BitcoinMiner(const CChainParams& chainparams)
{
    LogPrintf("LBRYcrdMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bitcoin-miner");

    unsigned int nExtraNonce = 0;

    boost::shared_ptr<CReserveScript> coinbaseScript;
    GetMainSignals().ScriptForMining(coinbaseScript);

    try {
        // Throw an error if no script was provided.  This can happen
        // due to some internal error but also if the keypool is empty.
        // In the latter case, already the pointer is NULL.
        if (!coinbaseScript || coinbaseScript->reserveScript.empty())
            throw std::runtime_error("No coinbase script available (mining requires a wallet)");

        while (true) {
            if (chainparams.MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                } while (true);
            }

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlock(Params(), coinbaseScript->reserveScript));
            if (!pblocktemplate.get())
            {
                LogPrintf("Error in BitcoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running BitcoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            //
            // Search
            //
            int64_t nStart = GetTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            uint256 hash;
            pblock->nNonce = 0;
            bool found = false;
            while (true)
            {
                unsigned int nHashesDone = 0;
                
                // Check if something found
                while (true)
                {
                    hash = pblock->GetPoWHash();
                    if (((uint16_t*)&hash)[15] == 0 && UintToArith256(hash) <= hashTarget)
                    {
                        found = true;
                        // Found a solution
                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
                        LogPrintf("LBRYcrdMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(pblock, chainparams);
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);
                        coinbaseScript->KeepScript();

                        // In regression test mode, stop mining after a block is found.
                        if (chainparams.MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                    pblock->nNonce += 1;
                    if ((pblock->nNonce & 0xFF) == 0)
                    {
                        nHashesDone = 0xFF+1;
                        break;
                    }
                }
                if (found)
                    break;
                
                // Meter hashes/sec
                static int64_t nHashCounter;
                if (nHPSTimerStart == 0)
                {
                    nHPSTimerStart = GetTimeMillis();
                    nHashCounter = 0;
                }
                else
                    nHashCounter += nHashesDone;
                if (GetTimeMillis() - nHPSTimerStart > 4000)
                {
                    static CCriticalSection cs;
                    {
                        LOCK(cs);
                        if (GetTimeMillis() - nHPSTimerStart > 4000)
                        {
                            dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                            nHPSTimerStart = GetTimeMillis();
                            nHashCounter = 0;
                            static int64_t nLogTime;
                            if (GetTime() - nLogTime > 30 * 60)
                            {
                                nLogTime = GetTime();
                                LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec/1000.0);
                            }
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && chainparams.MiningRequiresPeers())
                    break;
                if (pblock->nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                if (UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev) < 0)
                    break; // Recreate the block if the clock has run backwards,
                           // so that we can use the correct time.
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("LBRYcrdMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("LBRYcrdMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateBitcoins(bool fGenerate, int nThreads, const CChainParams& chainparams)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0)
        nThreads = GetNumCores();

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, boost::cref(chainparams)));
}
