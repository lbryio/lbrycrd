// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "ncctrie.h"
#include "coins.h"
#include "streams.h"
#include <boost/test/unit_test.hpp>
#include <iostream>

using namespace std;

BOOST_AUTO_TEST_SUITE(ncctrie_tests)

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

CMutableTransaction BuildTransaction(const CMutableTransaction& prev)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = prev.GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript();
    tx.vout[0].nValue = prev.vout[0].nValue;

    return tx;
}

CMutableTransaction BuildTransaction(const CTransaction& prev)
{
    return BuildTransaction(CMutableTransaction(prev));
}

void AddToMempool(CMutableTransaction& tx)
{
    mempool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, GetTime(), 111.0, chainActive.Height()));
}

bool CreateBlock(CBlockTemplate* pblocktemplate, bool f = false)
{
    static int unique_block_counter = 0;
    CBlock* pblock = &pblocktemplate->block;
    pblock->nVersion = 1;
    pblock->nTime = chainActive.Tip()->GetMedianTimePast()+1;
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(unique_block_counter++) << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockValue(chainActive.Height(), 0);
    pblock->vtx[0] = CTransaction(txCoinbase);
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
    for (int i = 0; ; ++i)
    {
        pblock->nNonce = i;
        if (CheckProofOfWork(pblock->GetHash(), pblock->nBits))
            break;
    }
    CValidationState state;
    bool success = (ProcessNewBlock(state, NULL, pblock) && state.IsValid() && pblock->GetHash() == chainActive.Tip()->GetBlockHash());
    pblock->hashPrevBlock = pblock->GetHash();
    return success;
}

bool RemoveBlock(uint256& blockhash)
{
    CValidationState state;
    if (mapBlockIndex.count(blockhash) == 0)
        return false;
    CBlockIndex* pblockindex = mapBlockIndex[blockhash];
    InvalidateBlock(state, pblockindex);
    if (state.IsValid())
    {
        ActivateBestChain(state);
    }
    return state.IsValid();

}

BOOST_AUTO_TEST_CASE(ncctrie_merkle_hash)
{
    int unused;
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    CMutableTransaction tx5 = BuildTransaction(tx4.GetHash());
    CMutableTransaction tx6 = BuildTransaction(tx5.GetHash());

    uint256 hash1;
    hash1.SetHex("2f3ae25e923dfa6fc24f0ff5d4367239425effaf47b703857436947a2dbfdda1");
    
    uint256 hash2;
    hash2.SetHex("833c0a897d1e32b16a7988c574dbcd750ed6695b947dea7b16e7782eac070a5d");
    
    uint256 hash3;
    hash3.SetHex("5ea4c62fb56bbca0cdf6fe4b2668c737641b4f48af8ef5041faa42df12aa3078");
    
    uint256 hash4;
    hash4.SetHex("47ae7f3ab9c0e9b13d08d5535a76fd36cecad0529baca390cea77b5bd3d99290");

    BOOST_CHECK(pnccTrie->empty());

    CNCCTrieCache ntState(pnccTrie);
    ntState.insertClaimIntoTrie(std::string("test"), CNodeValue(tx1.GetHash(), 0, 50, 100, 200));
    ntState.insertClaimIntoTrie(std::string("test2"), CNodeValue(tx2.GetHash(), 0, 50, 100, 200));

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!ntState.empty());
    BOOST_CHECK(ntState.getMerkleHash() == hash1);

    ntState.insertClaimIntoTrie(std::string("test"), CNodeValue(tx3.GetHash(), 0, 50, 101, 201));
    BOOST_CHECK(ntState.getMerkleHash() == hash1);
    ntState.insertClaimIntoTrie(std::string("tes"), CNodeValue(tx4.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.insertClaimIntoTrie(std::string("testtesttesttest"), CNodeValue(tx5.GetHash(), 0, 50, 100, 200));
    ntState.removeClaimFromTrie(std::string("testtesttesttest"), tx5.GetHash(), 0, unused);
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.flush();

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState1(pnccTrie);
    ntState1.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);

    BOOST_CHECK(ntState1.getMerkleHash() == hash0);

    CNCCTrieCache ntState2(pnccTrie);
    ntState2.insertClaimIntoTrie(std::string("abab"), CNodeValue(tx6.GetHash(), 0, 50, 100, 200));
    ntState2.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);

    BOOST_CHECK(ntState2.getMerkleHash() == hash3);

    ntState2.flush();

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash3);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState3(pnccTrie);
    ntState3.insertClaimIntoTrie(std::string("test"), CNodeValue(tx1.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState3.getMerkleHash() == hash4);
    ntState3.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash4);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState4(pnccTrie);
    ntState4.removeClaimFromTrie(std::string("abab"), tx6.GetHash(), 0, unused);
    BOOST_CHECK(ntState4.getMerkleHash() == hash2);
    ntState4.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState5(pnccTrie);
    ntState5.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);

    BOOST_CHECK(ntState5.getMerkleHash() == hash2);
    ntState5.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState6(pnccTrie);
    ntState6.insertClaimIntoTrie(std::string("test"), CNodeValue(tx3.GetHash(), 0, 50, 101, 201));

    BOOST_CHECK(ntState6.getMerkleHash() == hash2);
    ntState6.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pnccTrie->checkConsistency());

    CNCCTrieCache ntState7(pnccTrie);
    ntState7.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    
    BOOST_CHECK(ntState7.getMerkleHash() == hash0);
    ntState7.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pnccTrie->checkConsistency());
}

BOOST_AUTO_TEST_CASE(ncctrie_insert_update_claim)
{
    BOOST_CHECK(pnccTrie->nCurrentHeight == chainActive.Height() + 1);
    
    CBlockTemplate *pblocktemplate;
    LOCK(cs_main);
    Checkpoints::fEnabled = false;

    CScript scriptPubKey = CScript() << OP_TRUE;

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());
    

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    std::vector<CTransaction> coinbases;

    for (unsigned int i = 0; i < 103; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
        if (coinbases.size() < 3)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }

    delete pblocktemplate;

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    
    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx3 = BuildTransaction(tx1);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx4 = BuildTransaction(tx2);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;
    CMutableTransaction tx5 = BuildTransaction(coinbases[2]);
    tx5.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CNodeValue val;

    std::vector<uint256> blocks_to_invalidate;
    
    // Verify updates to the best claim get inserted immediately, and others don't.
    
    // Put tx1 and tx2 into the blockchain, and then advance 100 blocks to put them in the trie
    
    AddToMempool(tx1);
    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 3);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    delete pblocktemplate;
    
    // Verify tx1 and tx2 are in the trie

    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));

    // Spend tx1 with tx3, tx2 with tx4, and put in tx5.
    
    AddToMempool(tx3);
    AddToMempool(tx4);
    AddToMempool(tx5);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 4);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    // Verify tx1, tx2, and tx5 are not in the trie, but tx3 is in the trie.

    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));

    // Roll back the last block, make sure tx1 and tx2 are put back in the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Roll all the way back, make sure all txs are out of the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pnccTrie->getInfoForName(sName2, val));
    mempool.clear();
    
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pnccTrie->queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, and then undo that block.
    
    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, true));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Make sure it's not in the queue

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a claim that has gotten into the trie

    // Put tx1 in the chain, and then advance until it's in the trie

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    delete pblocktemplate;
       
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // Remove it from the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim just inserted into the queue

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie

    AddToMempool(tx2);
    AddToMempool(tx4);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 3);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim inserted into the queue by a previous block
    
    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie

    AddToMempool(tx2);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 51; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    mempool.clear();

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    delete pblocktemplate;
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    
    AddToMempool(tx4);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // roll back to the beginning
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spent update which updated a claim still in the queue

    // Create the original claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate, true));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo spending the update (undo tx6 spending tx3)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    mempool.clear();

    // make sure the update (tx3) still goes into effect in 100 blocks

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 1; i < 100; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate, true));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo updating the original claim (tx3 spends tx1)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    mempool.clear();

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 50; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;

    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    delete pblocktemplate;

    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // update the original claim (tx3 spends tx1)
    
    AddToMempool(tx3);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    delete pblocktemplate;
    
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    
    // spend the update (tx6 spends tx3)
    
    AddToMempool(tx6);
    
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 2);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    BOOST_CHECK(CreateBlock(pblocktemplate));
    blocks_to_invalidate.push_back(pblocktemplate->block.hashPrevBlock);
    delete pblocktemplate;
    
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    
    // undo spending the update (undo tx6 spending tx3)
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
}

BOOST_AUTO_TEST_CASE(ncctrienode_serialize_unserialize)
{
    CDataStream ss(SER_DISK, 0);

    CNCCTrieNode n1;
    CNCCTrieNode n2;
    CNodeValue throwaway;
    
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

    CNodeValue v1(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0, 50, 0, 100);
    CNodeValue v2(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1, 100, 1, 101);

    n1.insertValue(v1);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.insertValue(v2);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeValue(v1.txhash, v1.nOut, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeValue(v2.txhash, v2.nOut, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);
}

BOOST_AUTO_TEST_SUITE_END()
