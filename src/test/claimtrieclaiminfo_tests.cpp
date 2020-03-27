// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <miner.h>
#include <test/claimtriefixture.h>
#include <validation.h>

using namespace std;

extern std::vector<CClaimNsupports> seqSort(const std::vector<CClaimNsupports>& source);
extern std::size_t indexOf(const std::vector<CClaimNsupports>& source, const uint160& claimId);
extern boost::test_tools::predicate_result ValidatePairs(CClaimTrieCache& cache, const std::vector<std::pair<bool, uint256>>& pairs, uint256 claimHash);
extern uint256 claimInfoHash(const std::string& name, const COutPoint& outPoint, int bid, int seq, int nHeightOfLastTakeover);

BOOST_FIXTURE_TEST_SUITE(claimtrieclaiminfo_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(hash_includes_all_claiminfo_rollback_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setClaimInfoForkHeight(5);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(1);

    auto currentRoot = fixture.getMerkleHash();
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(currentRoot, fixture.getMerkleHash());
    fixture.IncrementBlocks(3);
    BOOST_CHECK_NE(currentRoot, fixture.getMerkleHash());
    fixture.DecrementBlocks(3);
    BOOST_CHECK_EQUAL(currentRoot, fixture.getMerkleHash());
}

BOOST_AUTO_TEST_CASE(hash_includes_all_claiminfo_single_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setClaimInfoForkHeight(2);
    fixture.IncrementBlocks(4);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(1);

    COutPoint outPoint(tx1.GetHash(), 0);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);

    CClaimTrieProof proof;
    BOOST_CHECK(fixture.getProofForName("test", claimId, proof));
    BOOST_CHECK(proof.hasValue);
    BOOST_CHECK_EQUAL(proof.outPoint, outPoint);
    auto claimHash = claimInfoHash("test", outPoint, 0, 0, proof.nHeightOfLastTakeover);
    BOOST_CHECK(ValidatePairs(fixture, proof.pairs, claimHash));
}

BOOST_AUTO_TEST_CASE(hash_includes_all_claiminfo_triple_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setClaimInfoForkHeight(2);
    fixture.IncrementBlocks(4);

    std::string names[] = {"test", "tester", "tester2"};
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "one", 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "two", 2);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "thr", 3);
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "for", 4);
    CMutableTransaction tx8 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "fiv", 5);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), names[1], "two", 2);
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), names[1], "thr", 3);
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(), names[2], "one", 1);
    fixture.IncrementBlocks(1);

    for (const auto& name : names) {
        int bid = 0;
        auto cfn = fixture.getClaimsForName(name);
        auto seqOrder = seqSort(cfn.claimsNsupports);
        for (auto& claimSupports : cfn.claimsNsupports) {
            CClaimTrieProof proof;
            BOOST_CHECK_EQUAL(name, cfn.name); // normalization depends
            auto& claim = claimSupports.claim;
            BOOST_CHECK(fixture.getProofForName(name, claim.claimId, proof));
            BOOST_CHECK(proof.hasValue);
            BOOST_CHECK_EQUAL(proof.outPoint, claim.outPoint);
            auto seq = indexOf(seqOrder, claim.claimId);
            auto claimHash = claimInfoHash(name, claim.outPoint, bid++, seq, proof.nHeightOfLastTakeover);
            BOOST_CHECK(ValidatePairs(fixture, proof.pairs, claimHash));
        }
    }
}

BOOST_AUTO_TEST_CASE(hash_includes_all_claiminfo_branched_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setClaimInfoForkHeight(2);
    fixture.IncrementBlocks(4);

    std::string names[] = {"test", "toast", "tot", "top", "toa", "toad"};
    for (const auto& name : names)
        fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);

    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "two", 2);
    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "tre", 3);
    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "qua", 4);
    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "cin", 5);
    fixture.IncrementBlocks(1);

    for (const auto& name : names) {
        int bid = 0;
        auto cfn = fixture.getClaimsForName(name);
        auto seqOrder = seqSort(cfn.claimsNsupports);
        for (auto& claimSupports : cfn.claimsNsupports) {
            CClaimTrieProof proof;
            BOOST_CHECK_EQUAL(name, cfn.name); // normalization depends
            auto& claim = claimSupports.claim;
            BOOST_CHECK(fixture.getProofForName(name, claim.claimId, proof));
            BOOST_CHECK(proof.hasValue);
            BOOST_CHECK_EQUAL(proof.outPoint, claim.outPoint);
            auto seq = indexOf(seqOrder, claim.claimId);
            auto claimHash = claimInfoHash(name, claim.outPoint, bid++, seq, proof.nHeightOfLastTakeover);
            BOOST_CHECK(ValidatePairs(fixture, proof.pairs, claimHash));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
