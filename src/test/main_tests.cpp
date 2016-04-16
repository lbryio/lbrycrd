// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "main.h"

#include "test/test_bitcoin.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyReductions(const Consensus::Params& consensusParams)
{
    int nHeight = 0;
    BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight, consensusParams), 400000000*COIN);
    
    // Verify that block reward is 1 until block 5100
    nHeight += 25;
    for (; nHeight < 5100; nHeight += 80)
    {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight, consensusParams), 1*COIN);
    }
    nHeight = 5100;

    // Verify it increases by 1 coin every 100 blocks

    for (int i = 1; nHeight < 55000; nHeight += 100, i++)
    {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight, consensusParams), i*COIN);
    }

    int maxReductions = 500;
    CAmount nInitialSubsidy = 500 * COIN;
    int nReductions;
    for (nReductions = 0; nReductions < maxReductions; nReductions++)
    {
        nHeight = (((nReductions * nReductions + nReductions) >> 1) * consensusParams.nSubsidyLevelInterval) + 55001;
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK_EQUAL(nSubsidy, nInitialSubsidy - nReductions * COIN);
    }
    nHeight = (((nReductions * nReductions + nReductions) >> 1) * consensusParams.nSubsidyLevelInterval) + 55001;
    BOOST_CHECK_EQUAL(GetBlockSubsidy(nHeight, consensusParams), 0);
}

static void TestBlockSubsidyReductions(int nSubsidyLevelInterval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidyLevelInterval = nSubsidyLevelInterval;
    TestBlockSubsidyReductions(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    TestBlockSubsidyReductions(Params(CBaseChainParams::MAIN).GetConsensus()); // As in main
    TestBlockSubsidyReductions(1); // As in regtest
    TestBlockSubsidyReductions(5); // Just another interval
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const Consensus::Params& consensusParams = Params(CBaseChainParams::MAIN).GetConsensus();
    CAmount nSum = 0;
    nSum += GetBlockSubsidy(0, consensusParams);
    for (int nHeight = 1; nHeight < 5000001; nHeight += 1000) {
        CAmount nSubsidy = GetBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= 500 * COIN);
        nSum += nSubsidy * 1000;
        BOOST_CHECK(MoneyRange(nSum));
    }
    BOOST_CHECK_EQUAL(nSum, 108322100000000000LL);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}

BOOST_AUTO_TEST_SUITE_END()
