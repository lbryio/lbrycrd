// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <test/claimtriefixture.h>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(claimtriebranching_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(claim_replace_test) {
    // no competing bids
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "bass", "one", 1);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "basso", "two", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("bass", tx1));
    fixture.Spend(tx1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "bassfisher", "one", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.checkConsistency());
    BOOST_CHECK(!fixture.is_best_claim("bass", tx1));
    BOOST_CHECK(fixture.is_best_claim("bassfisher", tx2));
}

BOOST_AUTO_TEST_CASE(takeover_stability_test) {
    // no competing bids
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "@bass", "one", 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "@bass", "two", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("@bass", tx2));
    CUint160 id; int takeover;
    BOOST_REQUIRE(fixture.getLastTakeoverForName("@bass", id, takeover));
    auto height = chainActive.Tip()->nHeight;
    BOOST_CHECK_EQUAL(takeover, height);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "@bass", "three", 3);
    fixture.Spend(tx3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("@bass", tx2));
    BOOST_REQUIRE(fixture.getLastTakeoverForName("@bass", id, takeover));
    BOOST_CHECK_EQUAL(takeover, height);
}

BOOST_AUTO_TEST_CASE(unaffected_children_get_new_parents_test) {
    // this happens first on block 193976
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "longest123", "one", 1);
    fixture.IncrementBlocks(1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "longest", "two", 2);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "longest1", "three", 3);
    CMutableTransaction tx4 = fixture.MakeClaim(tx3, "longest1", "three1234", 2);
    fixture.IncrementBlocks(1);
    auto n1 = fixture.getNodeChildren("longest");
    auto n2 = fixture.getNodeChildren("longest1");
    BOOST_CHECK_EQUAL(n1[0], "longest1");
    BOOST_CHECK_EQUAL(n2[0], "longest123");
    // TODO: this test at present fails to cover the split node case of the same thing (which occurs on block 202577)
}

/*
    claims
        no competing bids
        there is a competing bid inserted same height
            check the greater one wins
                - quantity is same, check outpoint greater wins
        there is an existing competing bid
            check that rules for delays are observed
            check that a greater amount wins
            check that a smaller amount does not win

*/
BOOST_AUTO_TEST_CASE(claim_test)
{
    // no competing bids
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));

    // there is a competing bid inserted same height
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    BOOST_CHECK_EQUAL(2U, fixture.getClaimsForName("test").claimsNsupports.size());

    fixture.DecrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test",tx2));
    BOOST_CHECK(!fixture.is_best_claim("test",tx3));
    BOOST_CHECK_EQUAL(0U, fixture.getClaimsForName("test").claimsNsupports.size());

    // make two claims , one older
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx4));
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_claim_in_queue("test",tx5));
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx4));
    BOOST_CHECK_EQUAL(2U, fixture.getClaimsForName("test").claimsNsupports.size());

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    BOOST_CHECK(fixture.is_claim_in_queue("test",tx5));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    fixture.DecrementBlocks(1);

    // check claim takeover, note that CClaimTrie.nProportionalDelayFactor is set to 1
    // instead of 32 in test_bitcoin.cpp
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test",tx6));
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_claim_in_queue("test",tx7));
    BOOST_CHECK(fixture.is_best_claim("test", tx6));
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test",tx7));
    BOOST_CHECK_EQUAL(2U, fixture.getClaimsForName("test").claimsNsupports.size());

    fixture.DecrementBlocks(10);
    BOOST_CHECK(fixture.is_claim_in_queue("test",tx7));
    BOOST_CHECK(fixture.is_best_claim("test", tx6));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx6));
    fixture.DecrementBlocks(10);
}

/*
  Testing deferred claim activation via a tx with a locktime.
*/
BOOST_AUTO_TEST_CASE(claim_locktime_test)
{
    ClaimTrieChainFixture fixture;
    // Effectively disable expiration for this test.
    fixture.setExpirationForkHeight(1, 1, 10000);
    fixture.IncrementBlocks(1);

    // Create tx1 with a relative locktime for validity 10 blocks in
    // the future, staged for automatic takeover if accepted.
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2, 10);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test", tx1));
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx2));

    // Forward to locktime expiration and takeover delay time.
    fixture.IncrementBlocks(25);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));

    // Abandon/Spend tx1.
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1);

    // Ensure tx2 is now best.
    BOOST_CHECK(fixture.is_best_claim("test", tx2));

    // Rewind and check tx1 is best again.
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));

    // Rewind to before locktime and activation.
    fixture.DecrementBlocks(25);
    BOOST_CHECK(fixture.is_best_claim("test", tx2));
}

/*
    spent claims
        spending winning claim will make losing active claim winner
        spending winning claim will make inactive claim winner
        spending winning claim will empty out claim trie
*/
BOOST_AUTO_TEST_CASE(spend_claim_test)
{
    ClaimTrieChainFixture fixture;

    // spending winning claim will make losing active claim winner
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    fixture.DecrementBlocks(1);

    // spending winning claim will make inactive claim winner
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    fixture.Spend(tx3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx4));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    fixture.DecrementBlocks(10);


    //spending winning claim will empty out claim trie
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx5));
    fixture.Spend(tx5);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test",tx5));
    BOOST_CHECK(pclaimTrie->empty());

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx5));
    fixture.DecrementBlocks(1);
}

/*
    supports
        check support with wrong name does not work
        check claim with more support wins
        check support delay
*/
BOOST_AUTO_TEST_CASE(support_test)
{
    ClaimTrieChainFixture fixture;
    // check claim with more support wins
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, "test", 1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(), tx2, "test", 10);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test", 11));
    fixture.DecrementBlocks(1);

    // check support delay
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "two", 2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test", 2));
    CMutableTransaction s4 = fixture.MakeSupport(fixture.GetCoinbase(), tx3, "test", 10); //10 delay
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test", 2));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx3));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test", 11));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test", 2));
    fixture.DecrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test", 2));
    fixture.DecrementBlocks(10);
}
/*
    support on abandon
        supporting a claim the same block it gets abandoned,
        or abandoning a support the same block claim gets abandoned,
        when there were no other claims would crash lbrycrd,
        make sure this doesn't happen in below three tests
        (https://github.com/lbryio/lbrycrd/issues/77)
*/
BOOST_AUTO_TEST_CASE(support_on_abandon_test)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(1);

    //supporting and abandoning on the same block will cause it to crash
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));
}

BOOST_AUTO_TEST_CASE(support_on_abandon_2_test)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    fixture.IncrementBlocks(1);

    //abandoning a support and abandoning claim on the same block will cause it to crash
    fixture.Spend(tx1);
    fixture.Spend(s1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));
}

BOOST_AUTO_TEST_CASE(support_on_abandon_3_test)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    fixture.IncrementBlocks(1);

    auto supports = fixture.getSupportsForName("test");
    BOOST_CHECK_EQUAL(supports.size(), 2);

    fixture.Spend(tx1);
    fixture.Spend(s1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test", tx1));
    supports = fixture.getSupportsForName("test");
    BOOST_CHECK_EQUAL(supports.size(), 1);

    fixture.Spend(s2);
    fixture.IncrementBlocks(1);
    supports = fixture.getSupportsForName("test");
    BOOST_CHECK(supports.empty());

    fixture.DecrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    supports = fixture.getSupportsForName("test");
    BOOST_CHECK_EQUAL(supports.size(), 2);

    fixture.DecrementBlocks(1);

    supports = fixture.getSupportsForName("test");
    BOOST_CHECK(supports.empty());
}

BOOST_AUTO_TEST_CASE(update_on_support_test)
{
    // make sure that support on abandon bug does not affect
    // updates happening on the same block as a support
    // (it should not)
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    fixture.IncrementBlocks(1);

    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",1);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1,"test","two",ClaimIdHash(tx1.GetHash(),0),1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.is_best_claim("test",u1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
}

/*
    support spend
        spending suport on winning claim will cause it to lose

        spending a support on txin[i] where i is not 0
*/
BOOST_AUTO_TEST_CASE(support_spend_test)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),tx1,"test",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    CMutableTransaction sp1 = fixture.Spend(s1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));
    fixture.DecrementBlocks(1);

    // spend a support on txin[i] where i is not 0
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"x","one",3);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(),"test","two",2);
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(),"test","three",1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),tx5,"test",2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx5));
    BOOST_CHECK_EQUAL(2U, fixture.getClaimsForName("test").claimsNsupports.size());

    // build the spend where s2 is sppent on txin[1] and tx3 is  spent on txin[0]
    uint32_t prevout = 0;
    CMutableTransaction tx;
    tx.nVersion = CTransaction::CURRENT_VERSION;
    tx.nLockTime = 1U << 31; // Disable BIP68
    tx.vin.resize(2);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = tx3.GetHash();
    tx.vin[0].prevout.n = prevout;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vin[1].prevout.hash = s2.GetHash();
    tx.vin[1].prevout.n = prevout;
    tx.vin[1].scriptSig = CScript();
    tx.vin[1].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    tx.vout[0].nValue = 1;

    fixture.CommitTx(tx);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx5));
}

BOOST_AUTO_TEST_CASE(claimtrie_update_takeover_test)
{
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 5);
    auto cid = ClaimIdHash(tx1.GetHash(), 0);
    fixture.IncrementBlocks(1);
    CUint160 cid2;
    int takeover;
    int height = chainActive.Tip()->nHeight;
    fixture.getLastTakeoverForName("test", cid2, takeover);
    BOOST_CHECK_EQUAL(height, takeover);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1, "test", "a", cid, 4);
    fixture.IncrementBlocks(1);
    fixture.getLastTakeoverForName("test", cid2, takeover);
    BOOST_CHECK_EQUAL(height, takeover);
    CMutableTransaction u2 = fixture.MakeUpdate(u1, "test", "b", cid, 3);
    fixture.IncrementBlocks(1);
    fixture.getLastTakeoverForName("test", cid2, takeover);
    CClaimValue value;
    BOOST_REQUIRE(fixture.getInfoForName("test", value) && value.nAmount == 3);
    BOOST_CHECK_EQUAL(cid, uint160(cid2));
    BOOST_CHECK_EQUAL(height, takeover);
    fixture.DecrementBlocks(1);
    fixture.getLastTakeoverForName("test", cid2, takeover);
    BOOST_CHECK_EQUAL(cid, uint160(cid2));
    BOOST_CHECK_EQUAL(height, takeover);
    fixture.DecrementBlocks(1);
    fixture.getLastTakeoverForName("test", cid2, takeover);
    BOOST_CHECK_EQUAL(cid, uint160(cid2));
    BOOST_CHECK_EQUAL(height, takeover);
}

/*
    update
        update preserves claim id
        update preserves supports
        winning update on winning claim happens without delay
        losing update on winning claim happens without delay
        update on losing claim happens with delay , and wins


*/
BOOST_AUTO_TEST_CASE(claimtrie_update_test)
{
    //update preserves claim id
    ClaimTrieChainFixture fixture;
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1, "test", "one", ClaimIdHash(tx1.GetHash(), 0), 2);
    fixture.IncrementBlocks(1);
    CClaimValue val;
    fixture.getInfoForName("test",val);
    BOOST_CHECK_EQUAL(val.claimId, ClaimIdHash(tx1.GetHash(),0));
    BOOST_CHECK(fixture.is_best_claim("test",u1));
    fixture.DecrementBlocks(1);

    // update preserves supports
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx2, "test", 1);
    CMutableTransaction u2 = fixture.MakeUpdate(tx2, "test", "one", ClaimIdHash(tx2.GetHash(), 0), 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",2));
    fixture.DecrementBlocks(1);

    // winning update on winning claim happens without delay
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(10);
    CMutableTransaction u3 = fixture.MakeUpdate(tx3, "test", "one", ClaimIdHash(tx3.GetHash(), 0), 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",u3));
    BOOST_CHECK_EQUAL(2U, fixture.getClaimsForName("test").claimsNsupports.size());
    fixture.DecrementBlocks(11);

    // losing update on winning claim happens without delay
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 3);
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test", tx5));
    BOOST_CHECK_EQUAL(2U, fixture.getClaimsForName("test").claimsNsupports.size());
    CMutableTransaction u4 = fixture.MakeUpdate(tx5, "test", "one", ClaimIdHash(tx5.GetHash(), 0), 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx6));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx5));
    fixture.DecrementBlocks(10);

    // update on losing claim happens with delay , and wins
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 3);
    CMutableTransaction tx8 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test", tx7));

    CMutableTransaction tx;
    tx.nVersion = CTransaction::CURRENT_VERSION;
    tx.nLockTime = 1U << 31U; // Disable BIP68
    tx.vin.resize(2);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = tx8.GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vin[1].prevout.hash = fixture.GetCoinbase().GetHash();
    tx.vin[1].prevout.n = 0;
    tx.vin[1].scriptSig = CScript();
    tx.vin[1].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = UpdateClaimScript("test",ClaimIdHash(tx8.GetHash(),0),"one");
    tx.vout[0].nValue = 4;
    fixture.CommitTx(tx);

    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx7));
    fixture.IncrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test",tx));

    fixture.DecrementBlocks(10);
    BOOST_CHECK(fixture.is_best_claim("test",tx7));
    fixture.DecrementBlocks(11);
}

/*
 * tests for effectiveAmount
 */
BOOST_AUTO_TEST_CASE(get_effective_amount_for_claim)
{
    // simplest scenario. One claim, no supports
    ClaimTrieChainFixture fixture;
    CMutableTransaction claimtx = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    uint160 claimId = ClaimIdHash(claimtx.GetHash(), 0);
    fixture.IncrementBlocks(1);

    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId).effectiveAmount, 2);
    BOOST_CHECK_EQUAL(fixture.getClaimsForName("inexistent").find(claimId).effectiveAmount, 0); //not found returns 0

    // one claim, one support
    fixture.MakeSupport(fixture.GetCoinbase(), claimtx, "test", 40);
    fixture.IncrementBlocks(1);

    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId).effectiveAmount, 42);

    // Two claims, first one with supports
    CMutableTransaction claimtx2 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "two", 1);
    uint160 claimId2 = ClaimIdHash(claimtx2.GetHash(), 0);
    fixture.IncrementBlocks(10);

    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId).effectiveAmount, 42);
    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId2).effectiveAmount, 1);
    BOOST_CHECK_EQUAL(fixture.getClaimsForName("inexistent").find(claimId).effectiveAmount, 0);
    BOOST_CHECK_EQUAL(fixture.getClaimsForName("inexistent").find(claimId2).effectiveAmount, 0);

    // Two claims, both with supports, second claim effective amount being less than first claim
    fixture.MakeSupport(fixture.GetCoinbase(), claimtx2, "test", 6);
    fixture.IncrementBlocks(13); //delay

    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId).effectiveAmount, 42);
    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId2).effectiveAmount, 7);

    // Two claims, both with supports, second one taking over
    fixture.MakeSupport(fixture.GetCoinbase(), claimtx2, "test", 1330);
    fixture.IncrementBlocks(26); //delay

    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId).effectiveAmount, 42);
    BOOST_CHECK_EQUAL(fixture.getClaimsForName("test").find(claimId2).effectiveAmount, 1337);
}

/*
 * tests for getClaimById basic consistency checks
 */
BOOST_AUTO_TEST_CASE(get_claim_by_id_test)
{
    ClaimTrieChainFixture fixture;
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 2);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    BOOST_CHECK(fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId);

    fixture.MakeSupport(fixture.GetCoinbase(), tx1, name, 4);
    fixture.IncrementBlocks(1);

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), name, "two", 2);
    uint160 claimId2 = ClaimIdHash(tx2.GetHash(), 0);
    fixture.IncrementBlocks(5);

    BOOST_CHECK(fixture.getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId2);


    CMutableTransaction u1 = fixture.MakeUpdate(tx1, name, "updated one", claimId, 1);
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId);
    BOOST_CHECK_EQUAL(claimValue.nAmount, 1);
    BOOST_CHECK_EQUAL(claimValue.outPoint.hash, u1.GetHash());

    fixture.Spend(u1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.getClaimById(claimId, claimName, claimValue));

    fixture.DecrementBlocks(8);

    CClaimValue claimValue2;
    claimName = "";
    BOOST_CHECK(!fixture.getClaimById(claimId2, claimName, claimValue2));
    BOOST_CHECK(claimName != name);
    BOOST_CHECK(claimValue2.claimId != claimId2);

    BOOST_CHECK(fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId);

    fixture.DecrementBlocks(2);

    claimName = "";
    BOOST_CHECK(!fixture.getClaimById(claimId, claimName, claimValue2));
    BOOST_CHECK(claimName != name);
    BOOST_CHECK(claimValue2.claimId != claimId);
}

BOOST_AUTO_TEST_CASE(insert_update_claim_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sName3("atest123");
    std::string sValue1("testa");
    std::string sValue2("testb");
    std::string sValue3("123testa");

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK_EQUAL(fixture.getMerkleHash(), hash0);

    CMutableTransaction tx1 = BuildTransaction(fixture.GetCoinbase());
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME
                                         << std::vector<unsigned char>(sName1.begin(), sName1.end())
                                         << std::vector<unsigned char>(sValue1.begin(), sValue1.end()) << OP_2DROP << OP_DROP << OP_TRUE;
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, tx1.vout[0].nValue - 10001);
    uint160 tx7ClaimId = ClaimIdHash(tx7.GetHash(), 0);
    COutPoint tx7OutPoint(tx7.GetHash(), 0);

    CClaimValue val;
    int nThrowaway;

    // Verify claims (tx7) for uncontrolled names go in immediately
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx7OutPoint);

    // Verify claims for controlled names are delayed, and that the bigger claim wins when inserted
    fixture.IncrementBlocks(5);
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!fixture.queueEmpty());

    fixture.IncrementBlocks(5); // 12

    BOOST_CHECK(!fixture.queueEmpty());

    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);

    // Verify updates to the best claim get inserted immediately, and others don't.
    CMutableTransaction tx3 = fixture.MakeUpdate(tx1, sName1, sValue1, tx1ClaimId, tx1.vout[0].nValue - 10000);
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    CMutableTransaction tx9 = fixture.MakeUpdate(tx7, sName1, sValue2, tx7ClaimId, tx7.vout[0].nValue - 10000);
    COutPoint tx9OutPoint(tx9.GetHash(), 0);
    fixture.IncrementBlocks(1, true); // 14

    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.haveClaim(sName1, tx1OutPoint));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx7OutPoint));
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx3OutPoint);
    BOOST_CHECK(fixture.haveClaimInQueue(sName1, tx9OutPoint, nThrowaway));

    // Roll back the last block, make sure tx1 and tx7 are put back in the trie
    fixture.DecrementBlocks(); // 13

    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);
    BOOST_CHECK(fixture.haveClaim(sName1, tx7OutPoint));
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!pclaimTrie->empty());

    // Roll all the way back, make sure all txs are out of the trie
    fixture.DecrementBlocks();

    BOOST_CHECK(!fixture.getInfoForName(sName1, val));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK_EQUAL(fixture.getMerkleHash(), hash0);
    BOOST_CHECK(fixture.queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, then advance a few blocks.
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    fixture.IncrementBlocks(10); // 11

    // Put tx7 in the chain, verify it goes into the queue
    fixture.CommitTx(tx7);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    // Undo that block and make sure it's not in the queue
    fixture.DecrementBlocks();

    // Make sure it's not in the queue
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Go back to the beginning
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Test spend a claim which was just inserted into the trie

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2, tx1.vout[0].nValue - 1);
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    CMutableTransaction tx4 = fixture.Spend(tx2);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    fixture.DecrementBlocks(1);
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Verify that if a claim in the queue is spent, it does not get into the trie

    // Put tx5 into the chain, advance until it's in the trie for a few blocks

    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2);
    COutPoint tx5OutPoint(tx5.GetHash(), 0);
    fixture.IncrementBlocks(6, true);

    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.haveClaimInQueue(sName2, tx2OutPoint, nThrowaway));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    fixture.IncrementBlocks(3);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    fixture.IncrementBlocks(5);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.haveClaim(sName2, tx2OutPoint));

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    fixture.DecrementBlocks();
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.haveClaimInQueue(sName2, tx2OutPoint, nThrowaway));

    fixture.IncrementBlocks(2);

    BOOST_CHECK(fixture.haveClaim(sName2, tx2OutPoint));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName2, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx5OutPoint);
    BOOST_CHECK(fixture.haveClaim(sName2, tx2OutPoint));

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.haveClaim(sName2, tx2OutPoint));

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName2, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx5OutPoint);
    BOOST_CHECK(fixture.haveClaim(sName2, tx2OutPoint));

    // roll back to the beginning
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Test undoing a spent update which updated a claim still in the queue

    // Create the claim that will cause the others to be in the queue

    fixture.CommitTx(tx7);
    fixture.IncrementBlocks(6, true);

    // Create the original claim (tx1)

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.haveClaimInQueue(sName1, tx1OutPoint, nThrowaway));

    // move forward some, but not far enough for the claim to get into the trie
    fixture.IncrementBlocks(2);

    // update the original claim (tx3 spends tx1)
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx1OutPoint, nThrowaway));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx1OutPoint));
    BOOST_CHECK(fixture.haveClaimInQueue(sName1, tx3OutPoint, nThrowaway));

    // spend the update (tx6 spends tx3)

    CMutableTransaction tx6 = fixture.Spend(tx3);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.haveClaim(sName1, tx3OutPoint));

    // undo spending the update (undo tx6 spending tx3)
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    // make sure the update (tx3) still goes into effect when it's supposed to
    fixture.IncrementBlocks(9);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx3OutPoint);

    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.haveClaim(sName1, tx3OutPoint));

    // roll all the way back
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    fixture.IncrementBlocks(5);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);

    // update the original claim (tx3 spends tx1)
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx3OutPoint);

    // spend the update (tx6 spends tx3)
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // undo spending the update (undo tx6 spending tx3)
    fixture.DecrementBlocks();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx3OutPoint);

    // Test having two updates to a claim in the same transaction

    // Add both txouts (tx8 spends tx3)
    CMutableTransaction tx8 = BuildTransaction(tx3, 0, 2);
    tx8.vout[0].scriptPubKey = CScript() << OP_UPDATE_CLAIM
                                         << std::vector<unsigned char>(sName1.begin(), sName1.end())
                                         << std::vector<unsigned char>(tx1ClaimId.begin(), tx1ClaimId.end())
                                         << std::vector<unsigned char>(sValue1.begin(), sValue1.end()) << OP_2DROP << OP_2DROP << OP_TRUE;
    tx8.vout[0].nValue -= 1;
    tx8.vout[1].scriptPubKey = CScript() << OP_CLAIM_NAME
                                         << std::vector<unsigned char>(sName1.begin(), sName1.end())
                                         << std::vector<unsigned char>(sValue2.begin(), sValue2.end()) << OP_2DROP << OP_DROP << OP_TRUE;
    COutPoint tx8OutPoint0(tx8.GetHash(), 0);
    COutPoint tx8OutPoint1(tx8.GetHash(), 1);

    fixture.CommitTx(tx8);
    fixture.IncrementBlocks(1, true);

    // ensure txout 0 made it into the trie and txout 1 did not

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx8OutPoint0);

    // roll forward until tx8 output 1 gets into the trie
    fixture.IncrementBlocks(6);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    fixture.IncrementBlocks(1);

    // ensure txout 1 made it into the trie and is now in control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx8OutPoint1);

    // roll back to before tx8
    fixture.DecrementBlocks();

    // roll all the way back
    fixture.DecrementBlocks();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // make sure invalid updates don't wreak any havoc

    // put tx1 into the trie
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);
    BOOST_CHECK(fixture.queueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(5);

    // put in bad tx10
    CTransaction root = fixture.GetCoinbase();
    CMutableTransaction tx10 = fixture.MakeUpdate(root, sName1, sValue1, tx1ClaimId, root.vout[0].nValue);
    COutPoint tx10OutPoint(tx10.GetHash(), 0);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(fixture.queueEmpty());

    // roll back, make sure nothing bad happens
    fixture.DecrementBlocks();

    // put it back in
    fixture.CommitTx(tx10);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(fixture.queueEmpty());

    // update it
    CMutableTransaction tx11 = fixture.MakeUpdate(tx10, sName1, sValue1, tx1ClaimId, tx10.vout[0].nValue);
    COutPoint tx11OutPoint(tx11.GetHash(), 0);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(fixture.queueEmpty());

    fixture.IncrementBlocks(10);

    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(fixture.queueEmpty());

    // roll back to before the update
    fixture.DecrementBlocks();

    BOOST_CHECK(!fixture.haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(fixture.queueEmpty());

    // make sure tx10 would have gotten into the trie, then run tests again

    fixture.IncrementBlocks(10);

    BOOST_CHECK(!fixture.haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(fixture.queueEmpty());

    // update it
    fixture.CommitTx(tx11);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(fixture.queueEmpty());

    // make sure tx11 would have gotten into the trie

    fixture.IncrementBlocks(20);

    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx11OutPoint, nThrowaway));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx11OutPoint));
    BOOST_CHECK(!fixture.haveClaimInQueue(sName1, tx10OutPoint, nThrowaway));
    BOOST_CHECK(!fixture.haveClaim(sName1, tx10OutPoint));
    BOOST_CHECK(fixture.queueEmpty());

    // roll all the way back
    fixture.DecrementBlocks();

    // Put tx10 and tx11 in without tx1 in
    fixture.CommitTx(tx10);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // update with tx11
    fixture.CommitTx(tx11);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // roll back to before tx11
    fixture.DecrementBlocks();

    // spent tx10 with tx12 instead which is not a claim operation of any kind
    CMutableTransaction tx12 = BuildTransaction(tx10);

    fixture.IncrementBlocks(1);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // roll all the way back
    fixture.DecrementBlocks();

    // make sure all claim for names which exist in the trie but have no
    // values get inserted immediately

    CMutableTransaction tx13 = fixture.MakeClaim(fixture.GetCoinbase(), sName3, sValue3, 1);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // roll back
    fixture.DecrementBlocks();
}

BOOST_AUTO_TEST_CASE(basic_merkle_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue("testa");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue, 10);
    fixture.IncrementBlocks(20);
    auto tx1MerkleHash = fixture.getMerkleHash();
    fixture.DecrementBlocks(20);
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(20);
    BOOST_CHECK_EQUAL(tx1MerkleHash, fixture.getMerkleHash());
}

BOOST_AUTO_TEST_CASE(supporting_claims_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    int initialHeight = chainActive.Height();

    CClaimValue val;

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

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx1.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // Put tx3 into the blockchain
    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName, 5);
    fixture.IncrementBlocks(1); // 7

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx3.GetHash(), 0)));
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(3); // 10

    // Put tx2 into the blockchain
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx2.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    // advance until tx2 is valid
    fixture.IncrementBlocks(9); // 20

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    fixture.IncrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(val.outPoint.n, 0);
    auto tx1MerkleHash = fixture.getMerkleHash();

    CMutableTransaction tx4 = BuildTransaction(tx1);
    CMutableTransaction tx5 = BuildTransaction(tx2);
    CMutableTransaction tx6 = BuildTransaction(tx3);

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 22

    // verify tx2 gains control
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    // unspend tx3, verify tx1 regains control
    fixture.DecrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(tx1MerkleHash, fixture.getMerkleHash());

    // update tx1 with tx7, verify tx7 has control
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction tx7 = fixture.MakeUpdate(tx1, sName, sValue1, claimId, tx1.vout[0].nValue);
    fixture.IncrementBlocks(1); // 22

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx7.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx7.GetHash());
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    // roll back to before tx7 is inserted, verify tx1 has control
    fixture.DecrementBlocks(1); // 21

    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(tx1MerkleHash, fixture.getMerkleHash());

    // roll back to before tx2 is valid
    fixture.DecrementBlocks(1); // 20

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 21

    // Verify tx2 gains control
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    // roll back to before tx3 is inserted
    fixture.DecrementBlocks(15); // 6

    // advance a few blocks
    fixture.IncrementBlocks(4); // 10

    // Put tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx2.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    // advance until tx2 is valid
    fixture.IncrementBlocks(9); // 20

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    fixture.IncrementBlocks(2); // 22

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    // roll all the way back
    fixture.DecrementBlocks(22); // 0
    BOOST_CHECK_EQUAL(initialHeight, chainActive.Height());

    // Make sure that when a support in the queue gets spent and then the spend is
    // undone, it goes back into the queue in the right spot

    // put tx2 and tx1 into the trie

    fixture.CommitTx(tx1);
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // put tx3 into the support queue
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 7

    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(!fixture.supportQueueEmpty());

    // advance a couple of blocks
    fixture.IncrementBlocks(2); // 9

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 10

    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // undo spend of tx3, verify it gets back in the right place in the queue
    fixture.DecrementBlocks(1); // 9

    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(!fixture.supportQueueEmpty());

    fixture.IncrementBlocks(4); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(val.outPoint.n, 0);
    // tx1MerkleHash doesn't match right here because it is at a different activation height (part of the node hash)
    tx1MerkleHash = fixture.getMerkleHash();

    // spend tx3 again, then undo the spend and roll back until it's back in the queue
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 14

    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    fixture.DecrementBlocks(1); // 13

    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(tx1MerkleHash, fixture.getMerkleHash());

    // roll all the way back
    fixture.DecrementBlocks(13);
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(supporting_claims2_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    CClaimValue val;
    int nThrowaway;

    // Test 2: create 1 LBC claim (tx1), create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 loses control when tx2 becomes valid, and then tx1 gains control when tx3 becomes valid
    // Then, verify that tx2 regains control when A) tx3 is spent and B) tx3 is undone

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx1.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // advance a few blocks
    fixture.IncrementBlocks(4); // 5

    // put tx2 into the blockchain
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 6

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx2.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // advance until tx2 is in the trie
    fixture.IncrementBlocks(4); // 10

    BOOST_CHECK(!fixture.queueEmpty());
    COutPoint tx2cp(tx2.GetHash(), 0);
    BOOST_CHECK(fixture.haveClaimInQueue(sName, tx2cp, nThrowaway));

    fixture.IncrementBlocks(1); // 11
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    // advance a few blocks
    fixture.IncrementBlocks(4); // 15

    // put tx3 into the blockchain

    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName, 5);
    fixture.IncrementBlocks(1); // 16

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx3.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(!fixture.supportQueueEmpty());

    // advance until tx3 is valid
    fixture.IncrementBlocks(4); // 20

    BOOST_CHECK(!fixture.supportQueueEmpty());

    fixture.IncrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    CMutableTransaction tx4 = BuildTransaction(tx1);
    CMutableTransaction tx5 = BuildTransaction(tx2);
    CMutableTransaction tx6 = BuildTransaction(tx3);

    // spend tx3
    fixture.CommitTx(tx6);
    fixture.IncrementBlocks(1); // 22

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    // undo spend
    fixture.DecrementBlocks(1); // 21

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // roll back to before tx3 is valid
    fixture.DecrementBlocks(1); // 20

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(!fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(20); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // Test 4: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 retains control throughout

    // put tx1 into the blockchain

    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx1.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // put tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 7

    auto rootMerkleHash = fixture.getMerkleHash();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(rootMerkleHash, fixture.getMerkleHash());

    // advance some, insert tx3, should be immediately valid
    fixture.IncrementBlocks(2); // 9
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 10

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(rootMerkleHash, fixture.getMerkleHash());

    // advance until tx2 is valid, verify tx1 retains control
    fixture.IncrementBlocks(3); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    BOOST_CHECK_EQUAL(rootMerkleHash, fixture.getMerkleHash());
    BOOST_CHECK(fixture.haveClaim(sName, tx2cp));

    // roll all the way back
    fixture.DecrementBlocks(13); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // Test 5: create 5 LBC claim (tx2), wait till valid, create 1 LBC claim (tx1), create 5 LBC support (tx3)
    // Verify that tx2 retains control until support becomes valid

    // insert tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());

    // advance a few blocks
    fixture.IncrementBlocks(9); // 10

    // insert tx1 into the block chain
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());

    // advance some
    fixture.IncrementBlocks(5); // 16

    // insert tx3 into the block chain
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 17

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(!fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());

    // advance until tx1 is valid
    fixture.IncrementBlocks(5); // 22

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(!fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    COutPoint tx1cp(tx1.GetHash(), 0);
    BOOST_CHECK(fixture.haveClaim(sName, tx1cp));

    // advance until tx3 is valid
    fixture.IncrementBlocks(11); // 33

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(33); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());


    // Test 6: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3), spend tx1
    // Verify that tx1 retains control until it is spent
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // advance a few blocks
    fixture.IncrementBlocks(5); // 6

    // insert tx2 into the blockchain
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 7

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // advance some, insert tx3
    fixture.IncrementBlocks(2); // 9
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 10

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // advance until tx2 is valid
    fixture.IncrementBlocks(3); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // spend tx1
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1); // 14

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    // undo spend of tx1
    fixture.DecrementBlocks(1); // 13

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(13); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // Test 7: create 1 LBC claim (tx1), wait till valid, create 5 LBC support (tx3), spend tx1
    // Verify name trie is empty

    // insert tx1 into blockchain
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // insert tx3 into blockchain
    fixture.CommitTx(tx3);
    fixture.IncrementBlocks(1); // 2

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // spent tx1
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1); // 3

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // roll all the way back
    fixture.DecrementBlocks(3);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(invalid_claimid_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    // Verify that supports expire

    // Create a 1 LBC claim (tx1)
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1, true); // 1

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx1.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // Make sure it gets way in there
    fixture.IncrementBlocks(100); // 101

    // Create a 5 LBC claim (tx2)
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 102

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx2.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());

    // Create a tx with a bogus claimId
    uint160 tx1ClaimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction tx3 = fixture.MakeUpdate(tx2, sName, sValue2, tx1ClaimId, 4);
    CMutableTransaction tx4 = BuildTransaction(tx3);
    COutPoint tx4OutPoint(tx4.GetHash(), 0);

    fixture.IncrementBlocks(1); // 103

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx3.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Verify it's not in the claim trie
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    BOOST_CHECK(!fixture.haveClaim(sName, tx4OutPoint));

    // Update the tx with the bogus claimId
    fixture.CommitTx(tx4);
    fixture.IncrementBlocks(1); // 104

    BOOST_CHECK(pcoinsTip->HaveCoin(tx4OutPoint));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());

    // Verify it's not in the claim trie

    BOOST_CHECK(!fixture.haveClaim(sName, tx4OutPoint));

    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());

    // Go forward a few hundred blocks and verify it's not in there

    fixture.IncrementBlocks(101); // 205

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.haveClaim(sName, tx4OutPoint));

    // go all the way back
    fixture.DecrementBlocks();
    BOOST_CHECK(pclaimTrie->empty());
}

/*
 * tests for getClaimById basic consistency checks
 */
BOOST_AUTO_TEST_CASE(get_claim_by_id_test_2)
{
    ClaimTrieChainFixture fixture;
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction txx = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimIdx = ClaimIdHash(txx.GetHash(), 0);

    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    BOOST_CHECK(fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId);

    CMutableTransaction txa = fixture.Spend(tx1);
    CMutableTransaction txb = fixture.Spend(txx);

    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK(!fixture.getClaimById(claimIdx, claimName, claimValue));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId);

    // this test fails
    BOOST_CHECK(fixture.getClaimById(claimIdx, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimIdx);
}

BOOST_AUTO_TEST_CASE(update_on_support2_test)
{
    ClaimTrieChainFixture fixture;

    std::string name = "test";
    std::string value = "one";

    auto height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, value, 3);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, name, 1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, name, 1);
    fixture.IncrementBlocks(1);

    CUint160 claimId;
    int lastTakeover;
    BOOST_CHECK(fixture.getLastTakeoverForName(name, claimId, lastTakeover));
    BOOST_CHECK_EQUAL(lastTakeover, height + 1);
    BOOST_CHECK_EQUAL(ClaimIdHash(tx1.GetHash(), 0), uint160(claimId));

    fixture.Spend(s1);
    fixture.Spend(s2);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1, name, value, ClaimIdHash(tx1.GetHash(), 0), 3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.getLastTakeoverForName(name, claimId, lastTakeover));
    BOOST_CHECK_EQUAL(lastTakeover, height + 1);
}

BOOST_AUTO_TEST_SUITE_END()
