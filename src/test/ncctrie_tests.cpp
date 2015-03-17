// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "primitives/transaction.h"
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

void AttachBlock(CNCCTrieCache& cache, std::vector<CNCCTrieQueueUndo>& block_undos)
{
    CNCCTrieQueueUndo undo;
    cache.incrementBlock(undo);
    block_undos.push_back(undo);
}

void DetachBlock(CNCCTrieCache& cache, std::vector<CNCCTrieQueueUndo>& block_undos)
{
    CNCCTrieQueueUndo& undo = block_undos.back();
    cache.decrementBlock(undo);
    block_undos.pop_back();
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
    std::vector<CNCCTrieQueueUndo> block_undos;
    int start_block = pnccTrie->nCurrentHeight;
    int current_block = start_block;

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    int tx1_height, tx2_height, tx3_height, tx4_height, tx1_undo_height, tx3_undo_height;

    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);

    CNCCTrieCache ntState(pnccTrie);

    tx1_height = current_block;
    ntState.addClaim(std::string("atest"), tx1.GetHash(), 0, 50, tx1_height);
    tx3_height = current_block;
    ntState.addClaim(std::string("btest"), tx3.GetHash(), 0, 50, tx3_height);
    AttachBlock(ntState, block_undos);
    current_block++;
    ntState.flush();
    for (; current_block < start_block + 100; ++current_block)
    {
        CNCCTrieCache s(pnccTrie);
        AttachBlock(s, block_undos);
        s.flush();
    }
    CNodeValue v;
    BOOST_CHECK(!pnccTrie->getInfoForName(std::string("atest"), v));
    BOOST_CHECK(!pnccTrie->getInfoForName(std::string("btest"), v));
    
    CNCCTrieCache ntState1(pnccTrie);
    AttachBlock(ntState1, block_undos);
    ntState1.flush();
    current_block++;

    CNCCTrieCache ntState2(pnccTrie);
    BOOST_CHECK(pnccTrie->getInfoForName(std::string("atest"), v));
    BOOST_CHECK(pnccTrie->getInfoForName(std::string("btest"), v));
    
    BOOST_CHECK(ntState2.spendClaim(std::string("atest"), tx1.GetHash(), 0, tx1_height, tx1_undo_height));
    BOOST_CHECK(ntState2.spendClaim(std::string("btest"), tx3.GetHash(), 0, tx3_height, tx3_undo_height));

    tx2_height = current_block;
    ntState2.addClaim(std::string("atest"), tx2.GetHash(), 0, 50, tx2_height, tx1.GetHash(), 0);
    tx4_height = current_block;
    ntState2.addClaim(std::string("btest"), tx4.GetHash(), 0, 50, tx4_height);

    AttachBlock(ntState2, block_undos);
    current_block++;
    ntState2.flush();

    BOOST_CHECK(pnccTrie->getInfoForName(std::string("atest"), v));
    BOOST_CHECK(!pnccTrie->getInfoForName(std::string("btest"), v));
    BOOST_CHECK(v.txhash == tx2.GetHash());

    CNCCTrieCache ntState3(pnccTrie);
    DetachBlock(ntState3, block_undos);
    current_block--;
    BOOST_CHECK(ntState3.undoAddClaim(std::string("atest"), tx2.GetHash(), 0, tx2_height));
    BOOST_CHECK(ntState3.undoAddClaim(std::string("btest"), tx4.GetHash(), 0, tx4_height));
    ntState3.undoSpendClaim(std::string("atest"), tx1.GetHash(), 0, 50, tx1_height, tx1_undo_height);
    ntState3.undoSpendClaim(std::string("btest"), tx3.GetHash(), 0, 50, tx3_height, tx3_undo_height);
    ntState3.flush();
    
    for (; current_block > start_block; --current_block)
    {
        CNCCTrieCache s(pnccTrie);
        DetachBlock(s, block_undos);
        s.flush();
    }
    CNCCTrieCache ntState4(pnccTrie);
    BOOST_CHECK(ntState4.undoAddClaim(std::string("atest"), tx1.GetHash(), 0, tx1_height));
    BOOST_CHECK(ntState4.undoAddClaim(std::string("btest"), tx3.GetHash(), 0, tx3_height));
    ntState4.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pnccTrie->queueEmpty());
}

BOOST_AUTO_TEST_CASE(ncctrie_undo)
{
    std::vector<CNCCTrieQueueUndo> block_undos;
    int start_block = pnccTrie->nCurrentHeight;
    int current_block = start_block;
    CNodeValue val;
    
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    int tx1_height, tx2_height;
    int tx1_undo_height, tx2_undo_height;

    BOOST_CHECK(pnccTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pnccTrie->queueEmpty());

    CNCCTrieCache ntState(pnccTrie);

    std::string name("a");

    /* Test undoing a claim before the claim gets into the trie */
    
    // make claim at start_block
    tx1_height = current_block;
    ntState.addClaim(name, tx1.GetHash(), 0, 50, tx1_height);
    AttachBlock(ntState, block_undos);
    current_block++;
    ntState.flush();
    BOOST_CHECK(!pnccTrie->queueEmpty());
    
    // undo block and remove claim
    DetachBlock(ntState, block_undos);
    current_block--;
    ntState.undoAddClaim(name, tx1.GetHash(), 0, tx1_height);
    ntState.flush();
    BOOST_CHECK(pnccTrie->queueEmpty());

    /* Test undoing a claim that has gotten into the trie */

    // make claim at start_block
    tx1_height = current_block;
    ntState.addClaim(name, tx1.GetHash(), 0, 50, tx1_height);
    AttachBlock(ntState, block_undos);
    current_block++;
    ntState.flush();
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // move to block start_block + 101
    for (; current_block < start_block + 101; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    
    // undo block and remove claim
    for (; current_block > start_block; --current_block)
    {
        DetachBlock(ntState, block_undos);
        ntState.flush();
    }
    ntState.undoAddClaim(name, tx1.GetHash(), 0, tx1_height);
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    /* Test undoing a spend which involves a claim just inserted into the queue */

    // make and spend claim at start_block
    tx1_height = current_block;
    ntState.addClaim(name, tx1.GetHash(), 0, 50, tx1_height);
    ntState.spendClaim(name, tx1.GetHash(), 0, tx1_height, tx1_undo_height);
    AttachBlock(ntState, block_undos);
    current_block++;
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // roll back the block
    DetachBlock(ntState, block_undos);
    current_block--;
    ntState.undoSpendClaim(name, tx1.GetHash(), 0, 50, tx1_height, tx1_undo_height);
    ntState.undoAddClaim(name, tx1.GetHash(), 0, tx1_height);
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    /* Test undoing a spend which involves a claim inserted into the queue by a previous block*/
    
    // make claim at block start_block
    tx1_height = current_block;
    ntState.addClaim(name, tx1.GetHash(), 0, 50, tx1_height);
    AttachBlock(ntState, block_undos);
    current_block++;
    ntState.flush();
    
    // spend the claim in block start_block + 50
    for (; current_block < start_block + 50; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    ntState.spendClaim(name, tx1.GetHash(), 0, tx1_height, tx1_undo_height);
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    
    // attach block start_block + 100 and make sure nothing is inserted
    for (; current_block < start_block + 101; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // roll back to block start_block + 50 and undo the spend
    for (; current_block > start_block + 50; --current_block)
    {
        DetachBlock(ntState, block_undos);
        ntState.flush();
    }
    ntState.undoSpendClaim(name, tx1.GetHash(), 0, 50, tx1_height, tx1_undo_height);
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // make sure it still gets inserted at block start_block + 100
    for (; current_block < start_block + 100; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    /* Test undoing a spend which involves a claim in the trie */

    // spend the claim at block start_block + 150
    for (; current_block < start_block + 150; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    ntState.spendClaim(name, tx1.GetHash(), 0, tx1_height, tx1_undo_height);
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // roll back the block
    DetachBlock(ntState, block_undos);
    --current_block;
    ntState.undoSpendClaim(name, tx1.GetHash(), 0, 50, tx1_height, tx1_undo_height);
    ntState.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    /* Test undoing an spent update which updated the best claim to a name*/

    // update the claim at block start_block + 150
    tx2_height = current_block;
    ntState.spendClaim(name, tx1.GetHash(), 0, tx1_height, tx1_undo_height);
    ntState.addClaim(name, tx2.GetHash(), 0, 75, tx2_height, tx1.GetHash(), 0);
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // move forward a bit
    for (; current_block < start_block + 200; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }

    // spend the update
    ntState.spendClaim(name, tx2.GetHash(), 0, tx2_height, tx2_undo_height);
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // undo the spend
    DetachBlock(ntState, block_undos);
    --current_block;
    ntState.undoSpendClaim(name, tx2.GetHash(), 0, 75, tx2_height, tx2_undo_height);
    ntState.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    /* Test undoing a spent update which updated a claim still in the queue */

    // roll everything back to block start_block + 50
    for (; current_block > start_block + 151; --current_block)
    {
        DetachBlock(ntState, block_undos);
        ntState.flush();
    }
    DetachBlock(ntState, block_undos);
    --current_block;
    ntState.undoAddClaim(name, tx2.GetHash(), 0, tx2_height);
    ntState.undoSpendClaim(name, tx1.GetHash(), 0, 50, tx1_height, tx1_undo_height);
    ntState.flush();
    for (; current_block > start_block + 50; --current_block)
    {
        DetachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // update the claim at block start_block + 50
    tx2_height = current_block;
    ntState.spendClaim(name, tx1.GetHash(), 0, tx1_height, tx1_undo_height);
    ntState.addClaim(name, tx2.GetHash(), 0, 75, tx2_height, tx1.GetHash(), 0);
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());

    // check that it gets inserted at block start_block + 150
    for (; current_block < start_block + 150; ++current_block)
    {
        AttachBlock(ntState, block_undos);
        ntState.flush();
    }
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(!pnccTrie->queueEmpty());
    AttachBlock(ntState, block_undos);
    ++current_block;
    ntState.flush();
    BOOST_CHECK(!pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());

    // roll back to start_block
    for (; current_block > start_block + 50; --current_block)
    {
        DetachBlock(ntState, block_undos);
        ntState.flush();
    }
    ntState.undoAddClaim(name, tx2.GetHash(), 0, tx2_height);
    ntState.undoSpendClaim(name, tx1.GetHash(), 0, 75, tx1_height, tx1_undo_height);
    ntState.flush();
    for (; current_block > start_block; --current_block)
    {
        DetachBlock(ntState, block_undos);
        ntState.flush();
    }
    ntState.undoAddClaim(name, tx1.GetHash(), 0, tx1_height);
    ntState.flush();
    BOOST_CHECK(pnccTrie->empty());
    BOOST_CHECK(pnccTrie->queueEmpty());
    BOOST_CHECK(pnccTrie->nCurrentHeight == start_block);
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
