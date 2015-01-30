// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "primitives/transaction.h"
#include "ncc.h"
#include "coins.h"
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

BOOST_AUTO_TEST_CASE(ncctrie_create_insert_remov)
{
    CMutableTransaction tx1 = BuildTransaction(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    CMutableTransaction tx5 = BuildTransaction(tx4.GetHash());
    CMutableTransaction tx6 = BuildTransaction(tx5.GetHash());

    uint256 hash1;
    hash1.SetHex("81727ef4dd44787941b9bba2c143a3607d92ee1f0d1d24d0da5aac6aa44ae12f");
    
    uint256 hash2;
    hash2.SetHex("653304cb35e66280b52ee6c52fcf2b8b1032ced6fea7a6e1548f24c47b1a3910");
    
    uint256 hash3;
    hash3.SetHex("48c0f04ab06338b25c66a6f237084eec49a5d761b5bfbe125d213fce33948242");
    
    uint256 hash4;
    hash4.SetHex("a79e8a5b28f7fa5e8836a4b48da9988bdf56ce749f81f413cb754f963a516200");

    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    
    CNCCTrie trie(&coins);

    BOOST_CHECK(trie.empty());

    CNCCTrieCache ntState(&trie);
    ntState.insertName(std::string("test"), tx1.GetHash(), 0, 50, 100);
    ntState.insertName(std::string("test2"), tx2.GetHash(), 0, 50, 100);

    BOOST_CHECK(trie.empty());
    BOOST_CHECK(!ntState.empty());
    BOOST_CHECK(ntState.getMerkleHash() == hash1);

    ntState.insertName(std::string("test"), tx3.GetHash(), 0, 50, 101);
    BOOST_CHECK(ntState.getMerkleHash() == hash1);
    ntState.insertName(std::string("tes"), tx4.GetHash(), 0, 50, 100);
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.insertName(std::string("testtesttesttest"), tx5.GetHash(), 0, 50, 100);
    ntState.removeName(std::string("testtesttesttest"), tx5.GetHash(), 0, 50, 100);
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.Flush();

    BOOST_CHECK(!trie.empty());
    BOOST_CHECK(trie.getMerkleHash() == hash2);
    BOOST_CHECK(trie.checkConsistency());
    CNCCTrieCache ntState2(&trie);
    ntState2.insertName(std::string("abab"), tx6.GetHash(), 0, 50, 100);
    ntState2.removeName(std::string("test"), tx1.GetHash(), 0, 50, 100);

    BOOST_CHECK(ntState2.getMerkleHash() == hash3);

    ntState2.Flush();

    BOOST_CHECK(!trie.empty());
    BOOST_CHECK(trie.getMerkleHash() == hash3);
    BOOST_CHECK(trie.checkConsistency());

    CNCCTrieCache ntState3(&trie);
    ntState3.insertName(std::string("test"), tx1.GetHash(), 0, 50, 100);
    BOOST_CHECK(ntState3.getMerkleHash() == hash4);
    ntState3.Flush();
    BOOST_CHECK(!trie.empty());
    BOOST_CHECK(trie.getMerkleHash() == hash4);
    BOOST_CHECK(trie.checkConsistency());

    CNCCTrieCache ntState4(&trie);
    ntState4.removeName(std::string("abab"), tx6.GetHash(), 0, 50, 100);
    BOOST_CHECK(ntState4.getMerkleHash() == hash2);
    ntState4.Flush();
    BOOST_CHECK(!trie.empty());
    BOOST_CHECK(trie.getMerkleHash() == hash2);
    BOOST_CHECK(trie.checkConsistency());

    CNCCTrieCache ntState5(&trie);
    ntState5.removeName(std::string("test"), tx3.GetHash(), 0, 50, 101);

    BOOST_CHECK(ntState5.getMerkleHash() == hash2);
    ntState5.Flush();
    BOOST_CHECK(!trie.empty());
    BOOST_CHECK(trie.getMerkleHash() == hash2);
    BOOST_CHECK(trie.checkConsistency());

    CNCCTrieCache ntState6(&trie);
    ntState6.insertName(std::string("test"), tx3.GetHash(), 0, 50, 101);

    BOOST_CHECK(ntState6.getMerkleHash() == hash2);
    ntState6.Flush();
    BOOST_CHECK(!trie.empty());
    BOOST_CHECK(trie.getMerkleHash() == hash2);
    BOOST_CHECK(trie.checkConsistency());
}

BOOST_AUTO_TEST_SUITE_END()
