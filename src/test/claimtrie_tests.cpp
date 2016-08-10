// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "consensus/validation.h"
#include "consensus/merkle.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "txmempool.h"
#include "claimtrie.h"
#include "nameclaim.h"
#include "coins.h"
#include "streams.h"
#include "chainparams.h"
#include "policy/policy.h"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include "test/test_bitcoin.h"

using namespace std;

CScript scriptPubKey = CScript() << OP_TRUE;

BOOST_FIXTURE_TEST_SUITE(claimtrie_tests, RegTestingSetup)

CMutableTransaction BuildTransaction(const uint256& prevhash)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = prevhash;
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript();
    tx.vout[0].nValue = 0;

    return tx;
}

CMutableTransaction BuildTransaction(const CMutableTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(numOutputs);
    tx.vin[0].prevout.hash = prev.GetHash();
    tx.vin[0].prevout.n = prevout;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    CAmount valuePerOutput = prev.vout[prevout].nValue;
    unsigned int numOutputsCopy = numOutputs;
    while ((numOutputsCopy = numOutputsCopy >> 1) > 0)
    {
        valuePerOutput = valuePerOutput >> 1;
    }
    for (unsigned int i = 0; i < numOutputs; ++i)
    {
        tx.vout[i].scriptPubKey = CScript();
        tx.vout[i].nValue = valuePerOutput;
    }

    return tx;
}

CMutableTransaction BuildTransaction(const CTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1)
{
    return BuildTransaction(CMutableTransaction(prev), prevout, numOutputs);
}

void AddToMempool(CMutableTransaction& tx)
{
    //CCoinsView dummy;
    //CCoinsViewCache view(&dummy);
    //CAmount inChainInputValue;
    //double dPriority = view.GetPriority(tx, chainActive.Height(), inChainInputValue);
    //unsigned int nSigOps = GetLegacySigOpCount(tx);
    //nSigOps += GetP2SHSigOpCount(tx, view);
    //LockPoints lp;
    //BOOST_CHECK(CheckSequenceLocks(tx, STANDARD_LOCKTIME_VERIFY_FLAGS, &lp));
    //mempool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, GetTime(), 111.1, chainActive.Height(), mempool.HasNoInputsOf(tx), 10000000000, false, nSigOps, lp));
    CValidationState state;
    bool *fMissingInputs;
    CFeeRate txFeeRate = CFeeRate(0);
    BOOST_CHECK(AcceptToMemoryPool(mempool, state, tx, false, fMissingInputs, &txFeeRate));
    //TestMemPoolEntryHelper entry;
    //entry.nFee = 11;
    //entry.dPriority = 111.0;
    //entry.nHeight = 11;
    //mempool.addUnchecked(tx.GetHash(), entry.Fee(10000).Time(GetTime()).FromTx(tx));
}

bool CreateBlock(CBlockTemplate* pblocktemplate)
{
    static int unique_block_counter = 0;
    CBlock* pblock = &pblocktemplate->block;
    pblock->nVersion = 1;
    pblock->nTime = chainActive.Tip()->GetBlockTime()+Params().GetConsensus().nPowTargetSpacing;
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(unique_block_counter++) << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblock->vtx[0] = CTransaction(txCoinbase);
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
    for (int i = 0; ; ++i)
    {
        pblock->nNonce = i;
        if (CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, Params().GetConsensus()))
        {
            break;
        }
    }
    CValidationState state;
    bool success = (ProcessNewBlock(state, Params(), NULL, pblock, true, NULL) && state.IsValid() && pblock->GetHash() == chainActive.Tip()->GetBlockHash());
    pblock->hashPrevBlock = pblock->GetHash();
    return success;
}

bool RemoveBlock(uint256& blockhash)
{
    CValidationState state;
    if (mapBlockIndex.count(blockhash) == 0)
        return false;
    CBlockIndex* pblockindex = mapBlockIndex[blockhash];
    InvalidateBlock(state, Params().GetConsensus(), pblockindex);
    if (state.IsValid())
    {
        ActivateBestChain(state, Params());
    }
    mempool.clear();
    return state.IsValid();

}

bool CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases)
{
    CBlockTemplate *pblocktemplate;
    coinbases.clear();
    BOOST_CHECK(pblocktemplate = CreateNewBlock(Params(), scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 1);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 100 + num_coinbases; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
        if (coinbases.size() < num_coinbases)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }
    delete pblocktemplate;
    return true;
}

bool CreateBlocks(unsigned int num_blocks, unsigned int num_txs)
{
    CBlockTemplate *pblocktemplate;
    BOOST_CHECK(pblocktemplate = CreateNewBlock(Params(), scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == num_txs);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < num_blocks; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;
    return true;
}

BOOST_AUTO_TEST_CASE(claimtrie_merkle_hash)
{
    fRequireStandard = false;
    CClaimValue unused;
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    uint160 hash160;
    CMutableTransaction tx1 = BuildTransaction(hash0);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    COutPoint tx4OutPoint(tx4.GetHash(), 0);
    CMutableTransaction tx5 = BuildTransaction(tx4.GetHash());
    COutPoint tx5OutPoint(tx5.GetHash(), 0);
    CMutableTransaction tx6 = BuildTransaction(tx5.GetHash());
    COutPoint tx6OutPoint(tx6.GetHash(), 0);

    uint256 hash1;
    hash1.SetHex("71c7b8d35b9a3d7ad9a1272b68972979bbd18589f1efe6f27b0bf260a6ba78fa");
    
    uint256 hash2;
    hash2.SetHex("c4fc0e2ad56562a636a0a237a96a5f250ef53495c2cb5edd531f087a8de83722");
    
    uint256 hash3;
    hash3.SetHex("baf52472bd7da19fe1e35116cfb3bd180d8770ffbe3ae9243df1fb58a14b0975");
    
    uint256 hash4;
    hash4.SetHex("c73232a755bf015f22eaa611b283ff38100f2a23fb6222e86eca363452ba0c51");

    BOOST_CHECK(pclaimTrie->empty());

    CClaimTrieCache ntState(pclaimTrie, false);
    ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1OutPoint, hash160, 50, 100, 200));
    ntState.insertClaimIntoTrie(std::string("test2"), CClaimValue(tx2OutPoint, hash160, 50, 100, 200));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!ntState.empty());
    BOOST_CHECK(ntState.getMerkleHash() == hash1);

    ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3OutPoint, hash160, 50, 101, 201));
    BOOST_CHECK(ntState.getMerkleHash() == hash1);
    ntState.insertClaimIntoTrie(std::string("tes"), CClaimValue(tx4OutPoint, hash160, 50, 100, 200));
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.insertClaimIntoTrie(std::string("testtesttesttest"), CClaimValue(tx5OutPoint, hash160, 50, 100, 200));
    ntState.removeClaimFromTrie(std::string("testtesttesttest"), tx5OutPoint, unused);
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState1(pclaimTrie, false);
    ntState1.removeClaimFromTrie(std::string("test"), tx1OutPoint, unused);
    ntState1.removeClaimFromTrie(std::string("test2"), tx2OutPoint, unused);
    ntState1.removeClaimFromTrie(std::string("test"), tx3OutPoint, unused);
    ntState1.removeClaimFromTrie(std::string("tes"), tx4OutPoint, unused);

    BOOST_CHECK(ntState1.getMerkleHash() == hash0);

    CClaimTrieCache ntState2(pclaimTrie, false);
    ntState2.insertClaimIntoTrie(std::string("abab"), CClaimValue(tx6OutPoint, hash160, 50, 100, 200));
    ntState2.removeClaimFromTrie(std::string("test"), tx1OutPoint, unused);

    BOOST_CHECK(ntState2.getMerkleHash() == hash3);

    ntState2.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash3);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState3(pclaimTrie, false);
    ntState3.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1OutPoint, hash160, 50, 100, 200));
    BOOST_CHECK(ntState3.getMerkleHash() == hash4);
    ntState3.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash4);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState4(pclaimTrie, false);
    ntState4.removeClaimFromTrie(std::string("abab"), tx6OutPoint, unused);
    BOOST_CHECK(ntState4.getMerkleHash() == hash2);
    ntState4.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState5(pclaimTrie, false);
    ntState5.removeClaimFromTrie(std::string("test"), tx3OutPoint, unused);

    BOOST_CHECK(ntState5.getMerkleHash() == hash2);
    ntState5.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState6(pclaimTrie, false);
    ntState6.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3OutPoint, hash160, 50, 101, 201));

    BOOST_CHECK(ntState6.getMerkleHash() == hash2);
    ntState6.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState7(pclaimTrie, false);
    ntState7.removeClaimFromTrie(std::string("test"), tx3OutPoint, unused);
    ntState7.removeClaimFromTrie(std::string("test"), tx1OutPoint, unused);
    ntState7.removeClaimFromTrie(std::string("tes"), tx4OutPoint, unused);
    ntState7.removeClaimFromTrie(std::string("test2"), tx2OutPoint, unused);
    
    BOOST_CHECK(ntState7.getMerkleHash() == hash0);
    ntState7.flush();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->checkConsistency());
}

BOOST_AUTO_TEST_CASE(claimtrie_insert_update_claim)
{
    
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);
    LOCK(cs_main);

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sName3("atest123");
    std::string sValue1("testa");
    std::string sValue2("testb");
    std::string sValue3("123testa");

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchName3(sName3.begin(), sName3.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());
    std::vector<unsigned char> vchValue3(sValue3.begin(), sValue3.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(6, coinbases));

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    
    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    std::vector<unsigned char> vchTx1ClaimId(tx1ClaimId.begin(), tx1ClaimId.end());
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = tx1.vout[0].nValue - 1;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    
    CMutableTransaction tx3 = BuildTransaction(tx1);
    tx3.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName1 << vchTx1ClaimId << vchValue1 << OP_2DROP << OP_2DROP << OP_TRUE;
    tx3.vout[0].nValue -= 10000;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    
    CMutableTransaction tx4 = BuildTransaction(tx2);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;
    COutPoint tx4OutPoint(tx4.GetHash(), 0);
    
    CMutableTransaction tx5 = BuildTransaction(coinbases[2]);
    tx5.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx5OutPoint(tx5.GetHash(), 0);
    
    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;
    COutPoint tx6OutPoint(tx6.GetHash(), 0);
    
    CMutableTransaction tx7 = BuildTransaction(coinbases[3]);
    tx7.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx7.vout[0].nValue = tx1.vout[0].nValue - 10001;
    uint160 tx7ClaimId = ClaimIdHash(tx7.GetHash(), 0);
    std::vector<unsigned char> vchTx7ClaimId(tx7ClaimId.begin(), tx7ClaimId.end());
    COutPoint tx7OutPoint(tx7.GetHash(), 0);
    
    CMutableTransaction tx8 = BuildTransaction(tx3, 0, 2);
    tx8.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName1 << vchTx1ClaimId << vchValue1 << OP_2DROP << OP_2DROP << OP_TRUE;
    tx8.vout[0].nValue = tx8.vout[0].nValue - 1;
    tx8.vout[1].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx8OutPoint0(tx8.GetHash(), 0);
    COutPoint tx8OutPoint1(tx8.GetHash(), 1);
    
    CMutableTransaction tx9 = BuildTransaction(tx7);
    tx9.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName1 << vchTx7ClaimId << vchValue2 << OP_2DROP << OP_2DROP << OP_TRUE;
    tx9.vout[0].nValue -= 10000;
    COutPoint tx9OutPoint(tx9.GetHash(), 0);
    
    CMutableTransaction tx10 = BuildTransaction(coinbases[4]);
    tx10.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName1 << vchTx1ClaimId << vchValue1 << OP_2DROP << OP_2DROP << OP_TRUE;
    COutPoint tx10OutPoint(tx10.GetHash(), 0);

    CMutableTransaction tx11 = BuildTransaction(tx10);
    tx11.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName1 << vchTx1ClaimId << vchValue1 << OP_2DROP << OP_2DROP << OP_TRUE;
    COutPoint tx11OutPoint(tx11.GetHash(), 0);

    CMutableTransaction tx12 = BuildTransaction(tx10);
    tx12.vout[0].scriptPubKey = CScript() << OP_TRUE;
    COutPoint tx12OutPoint(tx12.GetHash(), 0);

    CMutableTransaction tx13 = BuildTransaction(coinbases[5]);
    tx13.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName3 << vchValue3 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx13OutPoint(tx13.GetHash(), 0);
    
    CClaimValue val;

    int nThrowaway;

    std::vector<uint256> blocks_to_invalidate;

    // Verify claims for uncontrolled names go in immediately

    AddToMempool(tx7);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx7OutPoint);

    // Verify claims for controlled names are delayed, and that the bigger claim wins when inserted

    BOOST_CHECK(CreateBlocks(5, 1));

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(5, 1));

    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // Verify updates to the best claim get inserted immediately, and others don't.

    AddToMempool(tx3);
    AddToMempool(tx9);

    BOOST_CHECK(CreateBlocks(1, 3));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx1OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx7OutPoint));
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName1, tx9OutPoint, nThrowaway));

    // Disconnect all blocks until the first block, and then reattach them, in memory only

    //FlushStateToDisk();

    CCoinsViewCache coins(pcoinsTip);
    CClaimTrieCache trieCache(pclaimTrie);
    CBlockIndex* pindexState = chainActive.Tip();
    CValidationState state;
    CBlockIndex* pindex;
    for (pindex = chainActive.Tip(); pindex && pindex->pprev; pindex=pindex->pprev)
    {
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, pindex, Params().GetConsensus()));
        if (pindex == pindexState && (coins.DynamicMemoryUsage() + pcoinsTip->DynamicMemoryUsage()) <= nCoinCacheUsage)
        {
            bool fClean = true;
            BOOST_CHECK(DisconnectBlock(block, state, pindex, coins, trieCache, &fClean));
            pindexState = pindex->pprev;
        }
    }
    while (pindex != chainActive.Tip())
    {
        pindex = chainActive.Next(pindex);
        CBlock block;
        BOOST_CHECK(ReadBlockFromDisk(block, pindex, Params().GetConsensus()));
        BOOST_CHECK(ConnectBlock(block, state, pindex, coins, trieCache));
    }
    
    // Roll back the last block, make sure tx1 and tx7 are put back in the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName1, tx7OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->empty());

    // Roll all the way back, make sure all txs are out of the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName1, val));
    

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, then advance a few blocks.
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(10, 1));
    
    // Put tx7 in the chain, verify it goes into the queue

    AddToMempool(tx7);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Undo that block and make sure it's not in the queue

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Make sure it's not in the queue

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Go back to the beginning

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test spend a claim which was just inserted into the trie

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie

    AddToMempool(tx2);
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 3));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Verify that if a claim in the queue is spent, it does not get into the trie

    // Put tx5 into the chain, advance until it's in the trie for a few blocks

    AddToMempool(tx5);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(CreateBlocks(5, 1));
    
    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName2, tx2OutPoint, nThrowaway));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(3, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(5, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName2, tx2OutPoint, nThrowaway));
    
    BOOST_CHECK(CreateBlocks(2, 1));
    
    BOOST_CHECK(pclaimTrie->haveClaim(sName2, tx2OutPoint));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.outPoint == tx5OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.outPoint == tx5OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName2, tx2OutPoint));

    // roll back to the beginning
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing a spent update which updated a claim still in the queue

    // Create the claim that will cause the others to be in the queue

    AddToMempool(tx7);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(CreateBlocks(5, 1));

    // Create the original claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    //blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName1, tx1OutPoint, nThrowaway));

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(CreateBlocks(2, 1));
    
    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));
    //blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx1OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx1OutPoint));
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName1, tx3OutPoint, nThrowaway));

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx3OutPoint));

    // undo spending the update (undo tx6 spending tx3)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // make sure the update (tx3) still goes into effect when it's supposed to

    BOOST_CHECK(CreateBlocks(8, 1));
    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaim(sName1, tx3OutPoint));

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(5, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // update the original claim (tx3 spends tx1)
    
    AddToMempool(tx3);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);
    
    // spend the update (tx6 spends tx3)
    
    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    
    // undo spending the update (undo tx6 spending tx3)
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);

    // Test having two updates to a claim in the same transaction

    // Add both txouts (tx8 spends tx3)
    
    AddToMempool(tx8);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // ensure txout 0 made it into the trie and txout 1 did not
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx8OutPoint0);

    // roll forward until tx8 output 1 gets into the trie

    BOOST_CHECK(CreateBlocks(6, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    // ensure txout 1 made it into the trie and is now in control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx8OutPoint1);

    // roll back to before tx8

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // make sure invalid updates don't wreak any havoc

    // put tx1 into the trie

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(5, 1));

    // put in bad tx10

    AddToMempool(tx10);

    BOOST_CHECK(CreateBlocks(1, 2));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back, make sure nothing bad happens

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // put it back in

    AddToMempool(tx10);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // update it

    AddToMempool(tx11);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(10, 1));

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back to before the update

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // make sure tx10 would have gotten into the trie, then run tests again
    
    BOOST_CHECK(CreateBlocks(10, 1));

    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // update it

    AddToMempool(tx11);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // make sure tx11 would have gotten into the trie

    BOOST_CHECK(CreateBlocks(20, 1));

    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(!pclaimTrie->haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Put tx10 and tx11 in without tx1 in

    AddToMempool(tx10);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // update with tx11

    AddToMempool(tx11);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back to before tx11

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // spent tx10 with tx12 instead which is not a claim operation of any kind

    AddToMempool(tx12);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    // make sure all claim for names which exist in the trie but have no
    // values get inserted immediately

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    AddToMempool(tx13);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // roll back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
}

BOOST_AUTO_TEST_CASE(claimtrie_claim_expiration)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue("testa");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue(sValue.begin(), sValue.end());

    std::vector<CTransaction> coinbases;
   
    BOOST_CHECK(CreateCoinbases(2, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = BuildTransaction(tx1);
    tx2.vout[0].scriptPubKey = CScript() << OP_TRUE;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    
    CMutableTransaction tx3 = BuildTransaction(coinbases[1]);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    tx3.vout[0].nValue = tx1.vout[0].nValue >> 1;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    int nThrowaway;
    
    std::vector<uint256> blocks_to_invalidate;
    // set expiration time to 200 blocks after the block is created

    pclaimTrie->setExpirationTime(200);

    // create a claim. verify the expiration event has been scheduled.

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(CreateBlocks(100, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    
    // roll forward a bit and then roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(CreateBlocks(100, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // roll back some more.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // spend the claim. verify the expiration event is removed.
    
    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    
    // roll back the spend. verify the expiration event is returned.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
   
    // advance until the expiration event occurs. verify the event occurs on time.
    
    BOOST_CHECK(CreateBlocks(100, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // spend the expired claim

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // undo the spend. verify everything remains empty.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // verify the expiration event happens at the right time again

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify it gets reinserted and expiration gets scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll all the way back. verify the claim is removed and the expiration event is removed.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());

    // Make sure that when a claim expires, a lesser claim for the same name takes over

    CClaimValue val;

    // create one claim for the name

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance a little while and insert the second claim

    BOOST_CHECK(CreateBlocks(4, 1));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // advance until tx3 is valid, ensure tx1 is winning

    BOOST_CHECK(CreateBlocks(4, 1));

    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName, tx3OutPoint, nThrowaway));

    BOOST_CHECK(CreateBlocks(1, 1));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // advance again until tx3 is valid

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    BOOST_CHECK(CreateBlocks(188, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);

    // spend tx1

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    // roll back to when tx1 and tx3 are in the trie and tx1 is winning

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());
}

BOOST_AUTO_TEST_CASE(claimtrie_supporting_claims)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 1;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 5;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    std::vector<unsigned char> vchTx1ClaimId(tx1ClaimId.begin(), tx1ClaimId.end());
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1ClaimId << OP_2DROP << OP_DROP << OP_TRUE;
    tx3.vout[0].nValue = 5;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx5 = BuildTransaction(tx2);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx7 = BuildTransaction(tx1);
    tx7.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName << vchTx1ClaimId << vchValue1 << OP_2DROP << OP_2DROP << OP_TRUE;
    COutPoint tx7OutPoint(tx7.GetHash(), 0);

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    // Test 1: create 1 LBC claim (tx1), create 5 LBC support (tx3), create 5 LBC claim (tx2)
    // Verify that tx1 retains control throughout
    // spend tx3, verify that tx2 gains control
    // roll back to before tx3 is spent, verify tx1 regains control
    // update tx1 with tx7, verify tx7 has control
    // roll back to before tx7 is inserted, verify tx1 regains control
    // roll back to before tx2 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to before tx3 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to insertion of tx3, and don't insert it
    // insert tx2, advance until it is valid, verify tx2 gains control

    // Put tx1 in the blockchain

    AddToMempool(tx1);
    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(5, 1));

    // Put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(3, 1));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(9, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // spend tx3


    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);
    
    // unspend tx3, verify tx1 regains control

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // update tx1 with tx7, verify tx7 has control

    AddToMempool(tx7);
    BOOST_CHECK(CreateBlocks(1, 2));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx7.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx7OutPoint);

    // roll back to before tx7 is inserted, verify tx1 has control
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // roll back to before tx2 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));

    // Verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // roll back to before tx3 is inserted
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(4, 1));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(9, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Make sure that when a support in the queue gets spent and then the spend is
    // undone, it goes back into the queue in the right spot

    // put tx2 and tx1 into the trie

    AddToMempool(tx1);
    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 3));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(5, 1));

    // put tx3 into the support queue

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance a couple of blocks

    BOOST_CHECK(CreateBlocks(2, 1));

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // undo spend of tx3, verify it gets back in the right place in the queue

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(3, 1));
    BOOST_CHECK(CreateBlocks(1, 1));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // spend tx3 again, then undo the spend and roll back until it's back in the queue

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // advance until tx3 is valid again

    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());
}

BOOST_AUTO_TEST_CASE(claimtrie_supporting_claims2)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 1;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 5;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    std::vector<unsigned char> vchTx1ClaimId(tx1ClaimId.begin(), tx1ClaimId.end());
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1ClaimId << OP_2DROP << OP_DROP << OP_TRUE;
    tx3.vout[0].nValue = 5;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx5 = BuildTransaction(tx2);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    int nThrowaway;
    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;
    
    // Test 2: create 1 LBC claim (tx1), create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 loses control when tx2 becomes valid, and then tx1 gains control when tx3 becomes valid
    // Then, verify that tx2 regains control when A) tx3 is spent and B) tx3 is undone
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance a few blocks
    
    BOOST_CHECK(CreateBlocks(4, 1));

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is in the trie

    BOOST_CHECK(CreateBlocks(4, 1));

    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->haveClaimInQueue(sName, tx2OutPoint, nThrowaway));

    BOOST_CHECK(CreateBlocks(1, 1));
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(4, 1));

    // put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(4, 1));

    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // undo spend

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 4: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 retains control throughout

    // put tx1 into the blockchain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(5, 1));

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // advance some, insert tx3, should be immediately valid

    BOOST_CHECK(CreateBlocks(2, 1));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // advance until tx2 is valid, verify tx1 retains control

    BOOST_CHECK(CreateBlocks(2, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName, tx2OutPoint));

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 5: create 5 LBC claim (tx2), wait till valid, create 1 LBC claim (tx1), create 5 LBC support (tx3)
    // Verify that tx2 retains control until support becomes valid

    // insert tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // advance a few blocks

    BOOST_CHECK(CreateBlocks(9, 1));

    // insert tx1 into the block chain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // advance some
    
    BOOST_CHECK(CreateBlocks(5, 1));

    // insert tx3 into the block chain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(4, 1));
    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);
    BOOST_CHECK(pclaimTrie->haveClaim(sName, tx1OutPoint));

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(10, 1));
    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());


    // Test 6: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3), spend tx1
    // Verify that tx1 retains control until it is spent

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // advance a few blocks

    BOOST_CHECK(CreateBlocks(5, 1));
    
    // insert tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // advance some, insert tx3

    BOOST_CHECK(CreateBlocks(2, 1));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(2, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // spend tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);
    
    // undo spend of tx1

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // Test 7: create 1 LBC claim (tx1), wait till valid, create 5 LBC support (tx3), spend tx1
    // Verify name trie is empty

    // insert tx1 into blockchain
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // insert tx3 into blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // spent tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(claimtrie_invalid_claimid)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight = chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(2, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 1;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 5;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);

    CMutableTransaction tx3 = BuildTransaction(tx2);
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    std::vector<unsigned char> vchTx1ClaimId(tx1ClaimId.begin(), tx1ClaimId.end());
    tx3.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName << vchTx1ClaimId << vchValue2 << OP_2DROP << OP_2DROP << OP_TRUE;
    tx3.vout[0].nValue = 4;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    CMutableTransaction tx4 = BuildTransaction(tx3);
    tx4.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName << vchTx1ClaimId << vchValue2 << OP_2DROP << OP_2DROP << OP_TRUE;
    tx4.vout[0].nValue = 3;
    COutPoint tx4OutPoint(tx4.GetHash(), 0);

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    // Verify that supports expire

    // Create a 1 LBC claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Make sure it gets way in there

    BOOST_CHECK(CreateBlocks(100, 1));

    // Create a 5 LBC claim (tx2)

    AddToMempool(tx2);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Create a tx with a bogus claimId

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty()); 
    
    // Verify it's not in the claim trie

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    
    BOOST_CHECK(!pclaimTrie->haveClaim(sName, tx4OutPoint));

    // Update the tx with the bogus claimId

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx4.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Verify it's not in the claim trie

    BOOST_CHECK(!pclaimTrie->haveClaim(sName, tx4OutPoint));

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // Go forward a few hundred blocks and verify it's not in there

    BOOST_CHECK(CreateBlocks(300, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(!pclaimTrie->haveClaim(sName, tx4OutPoint));

    // go all the way back
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
}

BOOST_AUTO_TEST_CASE(claimtrie_expiring_supports)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight = chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 1;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 5;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    std::vector<unsigned char> vchTx1ClaimId(tx1ClaimId.begin(), tx1ClaimId.end());
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1ClaimId << OP_2DROP << OP_DROP << OP_TRUE;
    tx3.vout[0].nValue = 5;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM << vchName << vchTx1ClaimId << vchValue1 << OP_2DROP << OP_2DROP << OP_TRUE;
    COutPoint tx4OutPoint(tx4.GetHash(), 0);

    CMutableTransaction tx5 = BuildTransaction(tx3);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;
    COutPoint tx5OutPoint(tx5.GetHash(), 0);

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    pclaimTrie->setExpirationTime(200);

    // Verify that supports expire

    // Create a 1 LBC claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Create a 5 LBC support (tx3)

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Advance some, then insert 5 LBC claim (tx2)

    BOOST_CHECK(CreateBlocks(49, 1));

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(50, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);

    // Update tx1 so that it expires after tx3 expires

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    // Advance until the support expires

    BOOST_CHECK(CreateBlocks(50, 1));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(CreateBlocks(47, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // undo the block, make sure control goes back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    // redo the block, make sure it expires again

    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // roll back some, spend the support, and make sure nothing unexpected
    // happens at the time the support should have expired
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    AddToMempool(tx5);

    BOOST_CHECK(CreateBlocks(1, 2));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    BOOST_CHECK(CreateBlocks(50, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);
    
    //undo the spend, and make sure it still expires on time

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    BOOST_CHECK(CreateBlocks(48, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(claimtrienode_serialize_unserialize)
{
    fRequireStandard = false;
    CDataStream ss(SER_DISK, 0);

    uint160 hash160;

    CClaimTrieNode n1;
    CClaimTrieNode n2;
    CClaimValue throwaway;
    
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.hash.SetHex("a79e8a5b28f7fa5e8836a4b48da9988bdf56ce749f81f413cb754f963a516200");
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    CClaimValue v1(COutPoint(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0), hash160, 50, 0, 100);
    CClaimValue v2(COutPoint(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1), hash160, 100, 1, 101);

    n1.insertClaim(v1);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.insertClaim(v2);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeClaim(v1.outPoint, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeClaim(v2.outPoint, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);
}

bool verify_proof(const CClaimTrieProof proof, uint256 rootHash, const std::string& name)
{
    uint256 previousComputedHash;
    std::string computedReverseName;
    bool verifiedValue = false;

    for (std::vector<CClaimTrieProofNode>::const_reverse_iterator itNodes = proof.nodes.rbegin(); itNodes != proof.nodes.rend(); ++itNodes)
    {
        bool foundChildInChain = false;
        std::vector<unsigned char> vchToHash;
        for (std::vector<std::pair<unsigned char, uint256> >::const_iterator itChildren = itNodes->children.begin(); itChildren != itNodes->children.end(); ++itChildren)
        {
            vchToHash.push_back(itChildren->first);
            uint256 childHash;
            if (itChildren->second.IsNull())
            {
                if (previousComputedHash.IsNull())
                {
                    return false;
                }
                if (foundChildInChain)
                {
                    return false;
                }
                foundChildInChain = true;
                computedReverseName += itChildren->first;
                childHash = previousComputedHash;
            }
            else
            {
                childHash = itChildren->second;
            }
            vchToHash.insert(vchToHash.end(), childHash.begin(), childHash.end());
        }
        if (itNodes != proof.nodes.rbegin() && !foundChildInChain)
        {
            return false;
        }
        if (itNodes->hasValue)
        {
            uint256 valHash;
            if (itNodes->valHash.IsNull())
            {
                if (itNodes != proof.nodes.rbegin())
                {
                    return false;
                }
                if (!proof.hasValue)
                {
                    return false;
                }
                valHash = getValueHash(proof.outPoint,
                                       proof.nHeightOfLastTakeover);

                verifiedValue = true;
            }
            else
            {
                valHash = itNodes->valHash;
            }
            vchToHash.insert(vchToHash.end(), valHash.begin(), valHash.end());
        }
        else if (proof.hasValue && itNodes == proof.nodes.rbegin())
        {
            return false;
        }
        CHash256 hasher;
        std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
        hasher.Write(vchToHash.data(), vchToHash.size());
        hasher.Finalize(&(vchHash[0]));
        uint256 calculatedHash(vchHash);
        previousComputedHash = calculatedHash;
    }
    if (previousComputedHash != rootHash)
    {
        return false;
    }
    if (proof.hasValue && !verifiedValue)
    {
        return false;
    }
    std::string::reverse_iterator itComputedName = computedReverseName.rbegin();
    std::string::const_iterator itName = name.begin();
    for (; itName != name.end() && itComputedName != computedReverseName.rend(); ++itName, ++itComputedName)
    {
        if (*itName != *itComputedName)
        {
            return false;
        }
    }
    return (!proof.hasValue || itName == name.end());
}

BOOST_AUTO_TEST_CASE(claimtrievalue_proof)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName1("a");
    std::string sValue1("testa");
    
    std::string sName2("abc");
    std::string sValue2("testabc");

    std::string sName3("abd");
    std::string sValue3("testabd");

    std::string sName4("zyx");
    std::string sValue4("testzyx");

    std::string sName5("zyxa");
    std::string sName6("omg");
    std::string sName7("");

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());

    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<unsigned char> vchName3(sName3.begin(), sName3.end());
    std::vector<unsigned char> vchValue3(sValue3.begin(), sValue3.end());

    std::vector<unsigned char> vchName4(sName4.begin(), sName4.end());
    std::vector<unsigned char> vchValue4(sValue4.begin(), sValue4.end());

    std::vector<unsigned char> vchName7(sName7.begin(), sName7.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(5, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);
    
    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    
    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName3 << vchValue3 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    
    CMutableTransaction tx4 = BuildTransaction(coinbases[3]);
    tx4.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName4 << vchValue4 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx4OutPoint(tx4.GetHash(), 0);

    CMutableTransaction tx5 = BuildTransaction(coinbases[4]);
    tx5.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName7 << vchValue4 << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx5OutPoint(tx5.GetHash(), 0);

    CClaimValue val;

    std::vector<uint256> blocks_to_invalidate;

    // create a claim. verify the expiration event has been scheduled.

    AddToMempool(tx1);
    AddToMempool(tx2);
    AddToMempool(tx3);
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 5));

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.outPoint == tx1OutPoint);
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.outPoint == tx2OutPoint);
    BOOST_CHECK(pclaimTrie->getInfoForName(sName3, val));
    BOOST_CHECK(val.outPoint == tx3OutPoint);
    BOOST_CHECK(pclaimTrie->getInfoForName(sName4, val));
    BOOST_CHECK(val.outPoint == tx4OutPoint);

    CClaimTrieCache cache(pclaimTrie);

    CClaimTrieProof proof;

    proof = cache.getProofForName(sName1);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName1));
    BOOST_CHECK(proof.outPoint == tx1OutPoint);

    proof = cache.getProofForName(sName2);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName2));
    BOOST_CHECK(proof.outPoint == tx2OutPoint);

    proof = cache.getProofForName(sName3);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName3));
    BOOST_CHECK(proof.outPoint == tx3OutPoint);

    proof = cache.getProofForName(sName4);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName4));
    BOOST_CHECK(proof.outPoint == tx4OutPoint);

    proof = cache.getProofForName(sName5);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName5));
    BOOST_CHECK(proof.hasValue == false);

    proof = cache.getProofForName(sName6);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName6));
    BOOST_CHECK(proof.hasValue == false);

    proof = cache.getProofForName(sName7);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName7));
    BOOST_CHECK(proof.hasValue == false);
    
    AddToMempool(tx5);

    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(pclaimTrie->getInfoForName(sName7, val));
    BOOST_CHECK(val.outPoint == tx5OutPoint);

    cache = CClaimTrieCache(pclaimTrie);

    proof = cache.getProofForName(sName1);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName1));
    BOOST_CHECK(proof.outPoint == tx1OutPoint);

    proof = cache.getProofForName(sName2);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName2));
    BOOST_CHECK(proof.outPoint == tx2OutPoint);

    proof = cache.getProofForName(sName3);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName3));
    BOOST_CHECK(proof.outPoint == tx3OutPoint);

    proof = cache.getProofForName(sName4);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName4));
    BOOST_CHECK(proof.outPoint == tx4OutPoint);

    proof = cache.getProofForName(sName5);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName5));
    BOOST_CHECK(proof.hasValue == false);

    proof = cache.getProofForName(sName6);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName6));
    BOOST_CHECK(proof.hasValue == false);

    proof = cache.getProofForName(sName7);
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName7));
    BOOST_CHECK(proof.outPoint == tx5OutPoint);

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());
    
}

// Check that blocks with bogus calimtrie hash is rejected
BOOST_AUTO_TEST_CASE(bogus_claimtrie_hash)
{
    fRequireStandard = false;
    BOOST_CHECK(pclaimTrie->nCurrentHeight = chainActive.Height() + 1);
    LOCK(cs_main);
    std::string sName("test");
    std::string sValue1("test");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases));
    
    int orig_chain_height = chainActive.Height();

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 1;
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    AddToMempool(tx1);

    CBlockTemplate *pblockTemp;
    BOOST_CHECK(pblockTemp = CreateNewBlock(Params(), scriptPubKey));
    pblockTemp->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    pblockTemp->block.nVersion = 1;
    pblockTemp->block.nTime = chainActive.Tip()->GetBlockTime()+Params().GetConsensus().nPowTargetSpacing;
    CMutableTransaction txCoinbase(pblockTemp->block.vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblockTemp->block.vtx[0] = CTransaction(txCoinbase);    
    pblockTemp->block.hashMerkleRoot = BlockMerkleRoot(pblockTemp->block); 
    //create bogus hash 
    
    uint256 bogusHashClaimTrie;
    bogusHashClaimTrie.SetHex("aaa"); 
    pblockTemp->block.hashClaimTrie = bogusHashClaimTrie; 
    
    for (int i = 0; ; ++i)
    {
        pblockTemp->block.nNonce = i;
        if (CheckProofOfWork(pblockTemp->block.GetPoWHash(), pblockTemp->block.nBits, Params().GetConsensus()))
        {
            break;
        }
    }
    CValidationState state;
    bool success = ProcessNewBlock(state, Params(), NULL, &pblockTemp->block, true, NULL);
    // will process , but will not be connected 
    BOOST_CHECK(success);
    BOOST_CHECK(state.IsValid());
    BOOST_CHECK(pblockTemp->block.GetHash() != chainActive.Tip()->GetBlockHash());
    BOOST_CHECK_EQUAL(orig_chain_height,chainActive.Height());
    delete pblockTemp;

}

BOOST_AUTO_TEST_SUITE_END()
