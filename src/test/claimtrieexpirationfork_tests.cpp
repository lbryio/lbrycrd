// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <test/claimtriefixture.h>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(claimtrieexpirationfork_tests, RegTestingSetup)

/*
    expiration
        check claims expire and loses claim
        check claims expire and is not updateable (may be changed in future soft fork)
        check supports expire and can cause supported bid to lose claim
*/
BOOST_AUTO_TEST_CASE(claimtrie_expire_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setExpirationForkHeight(1000000, 5, 1000000);

    // check claims expire and loses claim
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test", "one", 2);
    fixture.IncrementBlocks(fixture.expirationTime());
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test", "one", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    fixture.DecrementBlocks(fixture.expirationTime());

    // check claims expire and is not updateable (may be changed in future soft fork)
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test", "one", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx3));
    fixture.IncrementBlocks(fixture.expirationTime());
    CMutableTransaction u1 = fixture.MakeUpdate(tx3, "test", "two", ClaimIdHash(tx3.GetHash(), 0), 2);
    BOOST_CHECK(!fixture.is_best_claim("test",u1));

    fixture.DecrementBlocks(fixture.expirationTime());
    BOOST_CHECK(fixture.is_best_claim("test", tx3));
    fixture.DecrementBlocks(1);

    // check supports expire and can cause supported bid to lose claim
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx4, "test", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx4));
    CMutableTransaction u2 = fixture.MakeUpdate(tx4, "test", "two", ClaimIdHash(tx4.GetHash(),0), 1);
    CMutableTransaction u3 = fixture.MakeUpdate(tx5, "test", "two", ClaimIdHash(tx5.GetHash(),0), 2);
    fixture.IncrementBlocks(fixture.expirationTime());
    BOOST_CHECK(fixture.is_best_claim("test", u3));
    fixture.DecrementBlocks(fixture.expirationTime());
    BOOST_CHECK(fixture.is_best_claim("test", tx4));
    fixture.DecrementBlocks(1);

    // check updated claims will extend expiration
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", tx6));
    CMutableTransaction u4 = fixture.MakeUpdate(tx6, "test", "two", ClaimIdHash(tx6.GetHash(), 0), 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", u4));
    fixture.IncrementBlocks(fixture.expirationTime()-1);
    BOOST_CHECK(fixture.is_best_claim("test", u4));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test", u4));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test", u4));
    fixture.DecrementBlocks(fixture.expirationTime());
    BOOST_CHECK(fixture.is_best_claim("test", tx6));
}

/*
    claim expiration for hard fork
        check claims do not expire post ExpirationForkHeight
        check supports work post ExpirationForkHeight
*/
BOOST_AUTO_TEST_CASE(hardfork_claim_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setExpirationForkHeight(7, 3, 6);

    // First create a claim and make sure it expires pre-fork
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",3);
    fixture.IncrementBlocks(4);
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));
    fixture.DecrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));
    fixture.IncrementBlocks(3);
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));

    // Create a claim 1 block before the fork height that will expire after the fork height
    fixture.IncrementBlocks(1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test2","one",3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 3);

    // Disable future expirations and fast-forward past the fork height
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 6);
    // make sure decrementing to before the fork height will apppropriately set back the
    // expiration time to the original expiraiton time
    fixture.DecrementBlocks(1);
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 6);

    // make sure that claim created 1 block before the fork expires as expected
    // at the extended expiration times
    BOOST_CHECK(fixture.is_best_claim("test2", tx2));
    fixture.IncrementBlocks(5);
    BOOST_CHECK(!fixture.is_best_claim("test2", tx2));
    fixture.DecrementBlocks(5);
    BOOST_CHECK(fixture.is_best_claim("test2", tx2));

    // This first claim is still expired since it's pre-fork, even
    // after fork activation
    BOOST_CHECK(!fixture.is_best_claim("test", tx1));

    // This new claim created at the fork height cannot expire at original expiration
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",1);
    fixture.IncrementBlocks(1);
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));
    fixture.DecrementBlocks(3);

    // but it expires at the extended expiration, and not a single block below
    fixture.IncrementBlocks(6);
    BOOST_CHECK(!fixture.is_best_claim("test",tx3));
    fixture.DecrementBlocks(6);
    fixture.IncrementBlocks(5);
    BOOST_CHECK(fixture.is_best_claim("test",tx3));
    fixture.DecrementBlocks(5);

    // Ensure that we cannot update the original pre-fork expired claim
    CMutableTransaction u1 = fixture.MakeUpdate(tx1,"test","two",ClaimIdHash(tx1.GetHash(),0), 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("test",u1));

    // Ensure that supports for the expired claim don't support it
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(),u1,"test",10);
    BOOST_CHECK(!fixture.is_best_claim("test",u1));

    // Ensure that we can update the new post-fork claim
    CMutableTransaction u2 = fixture.MakeUpdate(tx3,"test","two",ClaimIdHash(tx3.GetHash(),0), 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",u2));

    // Ensure that supports for the new post-fork claim
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),u2,"test",3);
    BOOST_CHECK(fixture.is_best_claim("test",u2));
}

/*
    support expiration for hard fork
*/
BOOST_AUTO_TEST_CASE(hardfork_support_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setExpirationForkHeight(2, 2, 4);

    // Create claim and support it before the fork height
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, "test", 2);
    // this claim will win without the support
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test","one",2);
    fixture.IncrementBlocks(2);

    // check that the claim expires as expected at the extended time, as does the support
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",3));
    fixture.DecrementBlocks(2);
    fixture.IncrementBlocks(3);
    BOOST_CHECK(!fixture.is_best_claim("test",tx1));
    fixture.DecrementBlocks(3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("test",tx1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",3));
    fixture.DecrementBlocks(1);

    // update the claims at fork
    fixture.DecrementBlocks(1);
    CMutableTransaction u1 = fixture.MakeUpdate(tx1, "test", "two", ClaimIdHash(tx1.GetHash(),0), 1);
    CMutableTransaction u2 = fixture.MakeUpdate(tx2, "test", "two", ClaimIdHash(tx2.GetHash(),0), 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(Params().GetConsensus().nExtendedClaimExpirationForkHeight, chainActive.Height());

    BOOST_CHECK(fixture.is_best_claim("test", u1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",3));
    BOOST_CHECK(!fixture.is_claim_in_queue("test", tx1));
    BOOST_CHECK(!fixture.is_claim_in_queue("test", tx2));

    // check that the support expires as expected
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("test", u2));
    fixture.DecrementBlocks(3);
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("test",u1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",3));
}

/*
    activation_fall_through and supports_fall_through
        Tests for where claims/supports in queues would be undone properly in a decrement.
        See https://github.com/lbryio/lbrycrd/issues/243 for more details
*/

BOOST_AUTO_TEST_CASE(activations_fall_through)
{
    ClaimTrieChainFixture fixture;

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    fixture.IncrementBlocks(3);
    BOOST_CHECK_EQUAL(fixture.proportionalDelayFactor(), 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "2", 3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.is_best_claim("A", tx1));
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    fixture.DecrementBlocks(3);
    fixture.Spend(tx1); // this will trigger early activation on tx2 claim
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    fixture.DecrementBlocks(1); //reorg the early activation
    BOOST_CHECK(fixture.is_best_claim("A", tx1));
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1); // this should not cause tx2 to activate again and crash
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
}

BOOST_AUTO_TEST_CASE(supports_fall_through)
{
    ClaimTrieChainFixture fixture;

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 3);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "2", 1);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "3", 2);
    fixture.IncrementBlocks(3);
    BOOST_CHECK_EQUAL(fixture.proportionalDelayFactor(), 1);
    CMutableTransaction sx2 = fixture.MakeSupport(fixture.GetCoinbase(), tx2, "A", 3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.is_best_claim("A", tx1));
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    fixture.DecrementBlocks(3);
    fixture.Spend(tx1); // this will trigger early activation
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    fixture.DecrementBlocks(1); // reorg the early activation
    BOOST_CHECK(fixture.is_best_claim("A", tx1));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx1)); //tx2 support should not be active
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx1)); //tx2 support should not be active
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx2)); //tx2 support should be active now
}

/*
    claim/support expiration for hard fork, but with checks for disk procedures
*/
BOOST_AUTO_TEST_CASE(hardfork_disk_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setExpirationForkHeight(7, 3, 6);

    // Check that incrementing to fork height, reseting to disk will get proper expiration time
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 3);
    fixture.IncrementBlocks(7, true);
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 6);

    // Create a claim and support 1 block before the fork height that will expire after the fork height.
    // Reset to disk, increment past the fork height and make sure we get
    // proper behavior
    fixture.DecrementBlocks(2);
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, "test", 1);
    fixture.IncrementBlocks(1);

    BOOST_CHECK_EQUAL(fixture.expirationTime(), 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(fixture.expirationTime(), 6);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",2));
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("test", tx1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test",2));
    fixture.DecrementBlocks(2);
    fixture.IncrementBlocks(5);
    BOOST_CHECK(!fixture.is_best_claim("test", tx1));

    // Create a claim and support before the fork height, reset to disk, update the claim
    // increment past the fork height and make sure we get proper behavior
    fixture.DecrementBlocks();
    fixture.setExpirationForkHeight(3, 5, 6);

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(),"test2","one",1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(),tx2,"test2",1);
    fixture.IncrementBlocks(1);

    CMutableTransaction u2 = fixture.MakeUpdate(tx2, "test2", "two", ClaimIdHash(tx2.GetHash(), 0), 1);
    // increment to fork
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("test2", u2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test2",2));
    // increment to original expiration, should not be expired
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("test2", u2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test2", 2));
    fixture.DecrementBlocks(2);
    // increment to extended expiration, should be expired and not one block before
    fixture.IncrementBlocks(5);
    BOOST_CHECK(!fixture.is_best_claim("test2", u2));
    fixture.DecrementBlocks(5);
    fixture.IncrementBlocks(4);
    BOOST_CHECK(fixture.is_best_claim("test2", u2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("test2", 1)); // the support expires one block before
}

BOOST_AUTO_TEST_CASE(claim_expiration_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue("testa");

    int nThrowaway;

    // set expiration time to 80 blocks after the block is created
    fixture.setExpirationForkHeight(1000000, 80, 1000000);

    // create a claim. verify the expiration event has been scheduled.
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue, 10);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);
    fixture.IncrementBlocks(1, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    fixture.IncrementBlocks(79); // 80

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    fixture.IncrementBlocks(1); // 81

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // roll forward a bit and then roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    fixture.IncrementBlocks(20); // 101

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    fixture.DecrementBlocks(21); // 80

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    fixture.IncrementBlocks(1); // 81

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    fixture.DecrementBlocks(2); // 79

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // roll back some more.
    fixture.DecrementBlocks(39); // 40

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // spend the claim. verify the expiration event is removed.
    CMutableTransaction tx2 = fixture.Spend(tx1);
    fixture.IncrementBlocks(1); // 41

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // roll back the spend. verify the expiration event is returned.
    fixture.DecrementBlocks(1); // 40

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // advance until the expiration event occurs. verify the event occurs on time.
    fixture.IncrementBlocks(40); // 80

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    fixture.IncrementBlocks(1); // 81

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // spend the expired claim
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 82

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // undo the spend. verify everything remains empty.
    fixture.DecrementBlocks(1); // 81

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    fixture.DecrementBlocks(1); // 80

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // verify the expiration event happens at the right time again
    fixture.IncrementBlocks(1); // 81

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // roll back to before the expiration event. verify it gets reinserted and expiration gets scheduled.
    fixture.DecrementBlocks(1); // 80

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // roll all the way back. verify the claim is removed and the expiration event is removed.
    fixture.DecrementBlocks(); // 0

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());

    // Make sure that when a claim expires, a lesser claim for the same name takes over

    CClaimValue val;

    // create one claim for the name
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1, true); // 1

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // advance a little while and insert the second claim
    fixture.IncrementBlocks(4); // 5
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue, 5);
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    fixture.IncrementBlocks(1); // 6

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    // advance until tx3 is valid, ensure tx1 is winning
    fixture.IncrementBlocks(4); // 10

    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(fixture.haveClaimInQueue(sName, tx3OutPoint, nThrowaway));

    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);
    auto tx1MerkleHash = fixture.getMerkleHash();

    // roll back to before tx3 is valid
    fixture.DecrementBlocks(1); // 10

    // advance again until tx is valid
    fixture.IncrementBlocks(1); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);
    BOOST_CHECK_EQUAL(tx1MerkleHash, fixture.getMerkleHash());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    fixture.IncrementBlocks(69, true); // 80

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());

    fixture.IncrementBlocks(1); // 81

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx3OutPoint);
    BOOST_CHECK(tx1MerkleHash != fixture.getMerkleHash());

    // spend tx1
    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1); // 82

    // roll back to when tx1 and tx3 are in the trie and tx1 is winning
    fixture.DecrementBlocks(); // 11

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.expirationQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);
    BOOST_CHECK_EQUAL(tx1MerkleHash, fixture.getMerkleHash());

    // roll all the way back
    fixture.DecrementBlocks();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.expirationQueueEmpty());
}

BOOST_AUTO_TEST_CASE(expiring_supports_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    fixture.setExpirationForkHeight(1000000, 80, 1000000);

    // to be active bid must have: a higher block number and current block >= (current height - block number) / 32

    // Verify that supports expire

    // Create a 1 LBC claim (tx1)
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);
    fixture.IncrementBlocks(1); // 1, expires at 81

    BOOST_CHECK(pcoinsTip->HaveCoin(COutPoint(tx1.GetHash(), 0)));
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // Create a 5 LBC support (tx3)
    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName, 5);
    fixture.IncrementBlocks(1); // 2, expires at 82

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());

    // Advance some, then insert 5 LBC claim (tx2)
    fixture.IncrementBlocks(19); // 21

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue2, 5);
    fixture.IncrementBlocks(1); // 22, activating in (22 - 2) / 1 = 20block (but not then active because support still holds tx1 up)

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    auto rootMerkleHash = fixture.getMerkleHash();

    // Advance until tx2 is valid
    fixture.IncrementBlocks(20); // 42

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK_EQUAL(rootMerkleHash, fixture.getMerkleHash());

    fixture.IncrementBlocks(1); // 43

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx1.GetHash());
    rootMerkleHash = fixture.getMerkleHash();

    // Update tx1 so that it expires after tx3 expires
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    CMutableTransaction tx4 = fixture.MakeUpdate(tx1, sName, sValue1, claimId, tx1.vout[0].nValue);

    fixture.IncrementBlocks(1); // 104

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx4.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());

    // Advance until the support expires
    fixture.IncrementBlocks(37); // 81

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    rootMerkleHash = fixture.getMerkleHash();

    fixture.IncrementBlocks(1); // 82

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());
    rootMerkleHash = fixture.getMerkleHash();

    // undo the block, make sure control goes back
    fixture.DecrementBlocks(1); // 81

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx4.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());

    // redo the block, make sure it expires again
    fixture.IncrementBlocks(1); // 82

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());
    rootMerkleHash = fixture.getMerkleHash();

    // roll back some, spend the support, and make sure nothing unexpected
    // happens at the time the support should have expired

    fixture.DecrementBlocks(19); // 63

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx4.GetHash());
    BOOST_CHECK(rootMerkleHash != fixture.getMerkleHash());

    fixture.Spend(tx3);
    fixture.IncrementBlocks(1); // 64

    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    fixture.IncrementBlocks(20); // 84

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    //undo the spend, and make sure it still expires on time

    fixture.DecrementBlocks(21); // 63

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx4.GetHash());

    fixture.IncrementBlocks(18); // 81

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(!fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx4.GetHash());

    fixture.IncrementBlocks(1); // 82

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName, val));
    BOOST_CHECK_EQUAL(val.outPoint.hash, tx2.GetHash());

    // roll all the way back
    fixture.DecrementBlocks(82);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.supportEmpty());
    BOOST_CHECK(fixture.supportQueueEmpty());
}

BOOST_AUTO_TEST_CASE(get_claim_by_id_test_3)
{
    ClaimTrieChainFixture fixture;
    fixture.setExpirationForkHeight(1000000, 5, 1000000);
    const std::string name = "test";
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);

    fixture.IncrementBlocks(1);

    CClaimValue claimValue;
    std::string claimName;
    BOOST_CHECK(fixture.getClaimById(claimId, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId);
    // make second claim with activation delay 1
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 2);
    uint160 claimId2 = ClaimIdHash(tx2.GetHash(), 0);

    fixture.IncrementBlocks(1);
    // second claim is not activated yet, but can still access by claim id
    BOOST_CHECK(fixture.is_best_claim(name, tx1));
    BOOST_CHECK(fixture.getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId2);

    fixture.IncrementBlocks(1);
    // second claim has activated
    BOOST_CHECK(fixture.is_best_claim(name, tx2));
    BOOST_CHECK(fixture.getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId2);


    fixture.DecrementBlocks(1);
    // second claim has been deactivated via decrement
    // should still be accesible via claim id
    BOOST_CHECK(fixture.is_best_claim(name, tx1));
    BOOST_CHECK(fixture.getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId2);

    fixture.IncrementBlocks(1);
    // second claim should have been re activated via increment
    BOOST_CHECK(fixture.is_best_claim(name, tx2));
    BOOST_CHECK(fixture.getClaimById(claimId2, claimName, claimValue));
    BOOST_CHECK_EQUAL(claimName, name);
    BOOST_CHECK_EQUAL(claimValue.claimId, claimId2);
}

BOOST_AUTO_TEST_SUITE_END()
