// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <test/claimtriefixture.h>
#include <validation.h>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(claimtrienormalization_tests, RegTestingSetup)

/*
    normalization
        test normalization function indpendent from rest of the code
*/

BOOST_AUTO_TEST_CASE(normalization_only)
{
    auto ccache = ::ClaimtrieCache();

    // basic ASCII casing tests
    BOOST_CHECK_EQUAL("test", ccache.normalizeClaimName("TESt", true));
    BOOST_CHECK_EQUAL("test", ccache.normalizeClaimName("tesT", true));
    BOOST_CHECK_EQUAL("test", ccache.normalizeClaimName("TesT", true));
    BOOST_CHECK_EQUAL("test", ccache.normalizeClaimName("test", true));
    BOOST_CHECK_EQUAL("test this", ccache.normalizeClaimName("Test This", true));

    // test invalid utf8 bytes are returned as is
    BOOST_CHECK_EQUAL("\xFF", ccache.normalizeClaimName("\xFF", true));
    BOOST_CHECK_EQUAL("\xC3\x28", ccache.normalizeClaimName("\xC3\x28", true));

    // ohm sign unicode code point \x2126 should be transformed to equivalent
    // unicode code point \x03C9 , greek small letter omega
    BOOST_CHECK_EQUAL("\xCF\x89", ccache.normalizeClaimName("\xE2\x84\xA6", true));

    // cyrillic capital ef code point \x0424 should be transformed to lower case
    // \x0444
    BOOST_CHECK_EQUAL("\xD1\x84", ccache.normalizeClaimName("\xD0\xA4", true));

    // armenian capital ben code point \x0532 should be transformed to lower case
    // \x0562
    BOOST_CHECK_EQUAL("\xD5\xA2", ccache.normalizeClaimName("\xD4\xB2", true));

    // japanese pbu code point \x3076 should be transformed by NFD decomposition
    // into \x3075 and \x3099
    BOOST_CHECK_EQUAL("\xE3\x81\xB5\xE3\x82\x99",
                          ccache.normalizeClaimName("\xE3\x81\xB6", true));

    // hangul ggwalg unicode code point \xAF51 should be transformed by NFD
    // decomposition into unicode code points \x1101 \x116A \x11B0
    // source: http://unicode.org/L2/L2009/09052-tr47.html
    BOOST_CHECK_EQUAL("\xE1\x84\x81\xE1\x85\xAA\xE1\x86\xB0",
                          ccache.normalizeClaimName("\xEA\xBD\x91", true));
}

/*
    normalization
        check claim name normalization before the fork
        check claim name normalization after the fork
*/
BOOST_AUTO_TEST_CASE(claimtriebranching_normalization)
{
    ClaimTrieChainFixture fixture;

    // check claim names are not normalized
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "normalizeTest", "one", 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("normalizeTest", tx1));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.getTotalNamesInTrie() == 0);
    fixture.CommitTx(tx1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("normalizeTest", tx1));

    CMutableTransaction tx2a = fixture.MakeClaim(fixture.GetCoinbase(), "Normalizetest", "one_a", 2);
    CMutableTransaction tx2 = fixture.MakeUpdate(tx2a, "Normalizetest", "one", ClaimIdHash(tx2a.GetHash(), 0), 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("normalizeTest", tx1));
    BOOST_CHECK(fixture.is_best_claim("Normalizetest", tx2));

    // Activate the fork (which rebuilds the existing claimtrie and
    // cache), flattening all previously existing name clashes due to
    // the normalization
    fixture.setNormalizationForkHeight(2);

    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("normalizeTest", tx1));
    BOOST_CHECK(fixture.is_best_claim("Normalizetest", tx2));

    fixture.IncrementBlocks(1, true);

    // Post-fork, tx1 (the previous winning claim) assumes all name
    // variants of what it originally was ...
    BOOST_CHECK(fixture.is_best_claim("normalizetest", tx1));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("normalizetest", 3));

    // Check equivalence of normalized claim names
    BOOST_CHECK(fixture.is_best_claim("normalizetest", tx1)); // collapsed tx2
    fixture.IncrementBlocks(1);

    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "NORMALIZETEST", "one", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("normalizetest", tx3));

    CMutableTransaction s1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, "NoRmAlIzEtEsT", 2);
    fixture.IncrementBlocks(1);

    // Ensure that supports work for normalized claim names
    BOOST_CHECK(fixture.is_best_claim("normalizetest", tx1)); // effective amount is 5
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("normalizetest", 5));

    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "foo", "bar", 1);
    CMutableTransaction s2 = fixture.MakeSupport(fixture.GetCoinbase(), tx4, "Foo", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("foo", tx4));

    CMutableTransaction u1 = fixture.MakeUpdate(tx4, "foo", "baz", ClaimIdHash(tx4.GetHash(), 0), 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("foo", u1));

    CMutableTransaction u2 = fixture.MakeUpdate(tx1, "nOrmalIzEtEst", "two", ClaimIdHash(tx1.GetHash(), 0), 3);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.is_best_claim("normalizetest", u2));

    // Add another set of unicode claims that will collapse after the fork
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), "Ame\u0301lie", "amelie", 2);
    fixture.IncrementBlocks(1);
    CClaimValue nval1;
    fixture.getInfoForName("amélie", nval1);
    BOOST_CHECK(nval1.claimId == ClaimIdHash(tx5.GetHash(), 0));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("amélie", 2));

    // Check equivalence of normalized claim names
    BOOST_CHECK(fixture.is_best_claim("amélie", tx5));

    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), "あてはまる", "jn1", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("あてはまる", tx7));

    CMutableTransaction tx8 = fixture.MakeClaim(fixture.GetCoinbase(), "AÑEJO", "es1", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("añejo", tx8));

    // Rewind to 1 block before the fork and be sure that the fork is no longer active
    fixture.DecrementBlocks();

    // Now check that our old (non-normalized) claims are 'alive' again
    BOOST_CHECK(fixture.is_best_claim("normalizeTest", tx1));
    BOOST_CHECK(!fixture.is_best_claim("Normalizetest", tx1)); // no longer equivalent
    BOOST_CHECK(fixture.is_best_claim("Normalizetest", tx2));

    // Create new claim
    CMutableTransaction tx9 = fixture.MakeClaim(fixture.GetCoinbase(), "blah", "blah", 1);
    std::string invalidUtf8("\xFF\xFF");
    CMutableTransaction tx10 = fixture.MakeClaim(fixture.GetCoinbase(), invalidUtf8, "blah", 1); // invalid UTF8

    // Roll forward to fork height again and check again that we're normalized
    fixture.IncrementBlocks(1);
    BOOST_CHECK(::ChainActive().Height() == Params().GetConsensus().nNormalizedNameForkHeight);
    BOOST_CHECK(fixture.is_best_claim("normalizetest", tx1)); // collapsed tx2
    BOOST_CHECK(fixture.is_best_claim(invalidUtf8, tx10));

    // Rewind to 1 block before the fork and be sure that the fork is
    // no longer active
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("Normalizetest", tx2));

    // Roll forward to fork height again and check again that we're normalized
    fixture.IncrementBlocks(1);
    BOOST_CHECK(::ChainActive().Height() == Params().GetConsensus().nNormalizedNameForkHeight);
    BOOST_CHECK(fixture.is_best_claim("normalizetest", tx1)); // collapsed tx2
}

BOOST_AUTO_TEST_CASE(claimtriecache_normalization)
{
    ClaimTrieChainFixture fixture;

    std::string name = "Ame\u0301lie";

    std::string name_upper = "Amélie";
    std::string name_normd = "amélie"; // this accented e is not actually the same as the one above; this has been "normalized"

    BOOST_CHECK(name != name_upper);

    // Add another set of unicode claims that will collapse after the fork
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, "amilie", 2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), name_upper, "amelie", 2);
    fixture.MakeClaim(fixture.GetCoinbase(), "amelie1", "amelie", 2);
    fixture.IncrementBlocks(1);

    CClaimValue lookupClaim;
    std::string lookupName;
    BOOST_CHECK(fixture.getClaimById(ClaimIdHash(tx2.GetHash(), 0), lookupName, lookupClaim));
    CClaimValue nval1;
    BOOST_CHECK(fixture.getInfoForName("amelie1", nval1));
    // amélie is not found cause normalization still not appear
    BOOST_CHECK(!fixture.getInfoForName(name_normd, nval1));

    // Activate the fork (which rebuilds the existing claimtrie and
    // cache), flattening all previously existing name clashes due to
    // the normalization
    fixture.setNormalizationForkHeight(1);
    int currentHeight = ::ChainActive().Height();

    fixture.IncrementBlocks(1);
    // Ok normalization fix the name problem
    BOOST_CHECK(fixture.getInfoForName(name_normd, nval1));
    BOOST_CHECK(nval1.nHeight == currentHeight);
    BOOST_CHECK(lookupClaim == nval1);

    auto trieCache = ::ClaimtrieCache();
    CBlockIndex* pindex = ::ChainActive().Tip();
    CCoinsViewCache coins(&::ChainstateActive().CoinsTip());

    CBlock block;
    int amelieValidHeight;
    std::string removed;
    BOOST_CHECK(trieCache.shouldNormalize());
    BOOST_CHECK(ReadBlockFromDisk(block, pindex, Params().GetConsensus()));
    BOOST_CHECK(::ChainstateActive().DisconnectBlock(block, pindex, coins, trieCache) == DisconnectResult::DISCONNECT_OK);
    BOOST_CHECK(!trieCache.shouldNormalize());
    BOOST_CHECK(trieCache.removeClaim(ClaimIdHash(tx2.GetHash(), 0), COutPoint(tx2.GetHash(), 0), removed, amelieValidHeight));

    BOOST_CHECK(trieCache.getInfoForName(name, nval1));
    BOOST_CHECK_EQUAL(nval1.claimId, ClaimIdHash(tx1.GetHash(), 0));
    BOOST_CHECK(trieCache.incrementBlock());
    BOOST_CHECK(trieCache.shouldNormalize());
}

BOOST_AUTO_TEST_CASE(undo_normalization_does_not_kill_claim_order)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(5);
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "a", "3", 3);
    fixture.IncrementBlocks(1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "2", 2);
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    fixture.IncrementBlocks(3, true);
    BOOST_CHECK(fixture.is_best_claim("a", tx3));
    fixture.DecrementBlocks();
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
}

BOOST_AUTO_TEST_CASE(normalized_activations_fall_through)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(5);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "AB", "1", 1);
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.proportionalDelayFactor() == 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "Ab", "2", 4);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "aB", "2", 3);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "ab", "2", 2);
    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.is_best_claim("AB", tx1));
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("ab", tx2));
    BOOST_CHECK(fixture.getClaimsForName("ab").claimsNsupports.size() == 4U);
    fixture.DecrementBlocks(3);
    fixture.Spend(tx1);
    fixture.Spend(tx2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("ab", tx3));
    BOOST_CHECK(fixture.getClaimsForName("ab").claimsNsupports.size() == 2U);
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("AB", tx1));
    fixture.Spend(tx1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("ab", tx2));

    for (int i = 0; i < 7; ++i) {
        fixture.IncrementBlocks(i, true); // well into normalized teritory
        CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), "CD", "a", 1 + i);
        fixture.IncrementBlocks(3);
        CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(), "Cd", "b", 2 + i);
        CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), "cD", "c", 3 + i);
        fixture.IncrementBlocks(1);
        BOOST_CHECK(fixture.is_best_claim("cd", tx5));
        fixture.Spend(tx5);
        fixture.IncrementBlocks(1);
        BOOST_CHECK(fixture.is_best_claim("cd", tx7));
        fixture.DecrementBlocks();
    }
}

BOOST_AUTO_TEST_CASE(normalization_removal_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(2);
    fixture.IncrementBlocks(3);
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "AB", "1", 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "Ab", "2", 2);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "aB", "3", 3);
    CMutableTransaction sx1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, "AB", 1);
    CMutableTransaction sx2 = fixture.MakeSupport(fixture.GetCoinbase(), tx2, "Ab", 1);

    auto cache = ::ClaimtrieCache();
    int height = ::ChainActive().Height() + 1;
    cache.addClaim("AB", COutPoint(tx1.GetHash(), 0), ClaimIdHash(tx1.GetHash(), 0), 1, height);
    cache.addClaim("Ab", COutPoint(tx2.GetHash(), 0), ClaimIdHash(tx2.GetHash(), 0), 2, height);
    cache.addClaim("aB", COutPoint(tx3.GetHash(), 0), ClaimIdHash(tx3.GetHash(), 0), 3, height);
    cache.addSupport("AB", COutPoint(sx1.GetHash(), 0), ClaimIdHash(tx1.GetHash(), 0), 1, height);
    cache.addSupport("Ab", COutPoint(sx2.GetHash(), 0), ClaimIdHash(tx2.GetHash(), 0), 1, height);
    BOOST_CHECK(cache.incrementBlock());
    BOOST_CHECK(cache.getClaimsForName("ab").claimsNsupports.size() == 3U);
    BOOST_CHECK(cache.getClaimsForName("ab").claimsNsupports[0].supports.size() == 1U);
    BOOST_CHECK(cache.getClaimsForName("ab").claimsNsupports[1].supports.size() == 0U);
    BOOST_CHECK(cache.getClaimsForName("ab").claimsNsupports[2].supports.size() == 1U);
    BOOST_CHECK(cache.decrementBlock());
    BOOST_CHECK(cache.finalizeDecrement());
    std::string unused;
    BOOST_CHECK(cache.removeSupport(COutPoint(sx1.GetHash(), 0), unused, height));
    BOOST_CHECK(cache.removeSupport(COutPoint(sx2.GetHash(), 0), unused, height));
    BOOST_CHECK(cache.removeClaim(ClaimIdHash(tx1.GetHash(), 0), COutPoint(tx1.GetHash(), 0), unused, height));
    BOOST_CHECK(cache.removeClaim(ClaimIdHash(tx2.GetHash(), 0), COutPoint(tx2.GetHash(), 0), unused, height));
    BOOST_CHECK(cache.removeClaim(ClaimIdHash(tx3.GetHash(), 0), COutPoint(tx3.GetHash(), 0), unused, height));
    BOOST_CHECK(cache.getClaimsForName("ab").claimsNsupports.size() == 0U);
}

BOOST_AUTO_TEST_CASE(normalization_does_not_kill_supports)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(3);
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    fixture.MakeSupport(fixture.GetCoinbase(), tx1, "A", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 2));
    fixture.MakeSupport(fixture.GetCoinbase(), tx1, "A", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 3));
    fixture.MakeSupport(fixture.GetCoinbase(), tx1, "A", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("a", 4));
    fixture.MakeSupport(fixture.GetCoinbase(), tx1, "A", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("a", 5));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("a", 4));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 3));
    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 2));
    fixture.IncrementBlocks(5);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("a", 2));
}

BOOST_AUTO_TEST_CASE(normalization_does_not_fail_on_spend)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(2);

    std::string sName1("testN");
    std::string sName2("testn");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, "1", 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim(sName1, tx1));
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, "2", 2);
    CMutableTransaction tx1s = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName1, 2);
    fixture.IncrementBlocks(2, true);
    BOOST_CHECK(fixture.is_best_claim(sName2, tx1));

    CMutableTransaction tx3 = fixture.Spend(tx1); // abandon the claim
    CMutableTransaction tx3s = fixture.Spend(tx1s);
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim(sName2, tx2));
    fixture.DecrementBlocks();
    BOOST_CHECK(fixture.is_best_claim(sName1, tx1));
}

BOOST_AUTO_TEST_CASE(unnormalized_expires_before_fork)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(4);
    fixture.setExpirationForkHeight(1, 3, 3);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    fixture.IncrementBlocks(3);
    CMutableTransaction sp1 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, "A", 1);
    BOOST_CHECK(fixture.is_best_claim("A", tx1));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("A", tx1));
    BOOST_CHECK(!fixture.is_best_claim("a", tx1));
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("A", tx1));
    BOOST_CHECK(!fixture.is_best_claim("a", tx1));
    fixture.DecrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("A", tx1));
}

BOOST_AUTO_TEST_CASE(unnormalized_activates_on_fork)
{
    // this was an unfortunate bug; the normalization fork should not have activated anything
    // alas, it's now part of our history; we hereby test it to keep it that way
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(4);
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    fixture.IncrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("A", tx1));
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "2", 2);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("a", tx2));
    fixture.IncrementBlocks(2);
    BOOST_CHECK(fixture.is_best_claim("a", tx2));
    fixture.DecrementBlocks(3);
    BOOST_CHECK(fixture.is_best_claim("A", tx1));
}

BOOST_AUTO_TEST_CASE(normalization_does_not_kill_sort_order)
{
    ClaimTrieChainFixture fixture;
    fixture.setNormalizationForkHeight(2);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "2", 2);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "a", "3", 3);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    BOOST_CHECK(fixture.is_best_claim("a", tx3));

    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.is_best_claim("A", tx2));
    BOOST_CHECK(fixture.is_best_claim("a", tx3));
    BOOST_CHECK(fixture.getClaimsForName("a").claimsNsupports.size() == 3U);

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.is_best_claim("A", tx2));
    BOOST_CHECK(fixture.is_best_claim("a", tx3));
}

BOOST_AUTO_TEST_CASE(normalization_does_not_kill_expirations)
{
    ClaimTrieChainFixture fixture;
    auto& consensus = Params().GetConsensus();
    fixture.setExpirationForkHeight(consensus.nExtendedClaimExpirationForkHeight, 3, consensus.nExtendedClaimExpirationTime);
    fixture.setNormalizationForkHeight(4);
    // need to see that claims expiring on the frame when we normalize aren't kept
    // need to see that supports expiring on the frame when we normalize aren't kept
    // need to see that claims & supports carried through the normalization fork do expire
    // and that they come back correctly when we roll backwards

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "A", "1", 1);
    fixture.MakeSupport(fixture.GetCoinbase(), tx1, "A", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 2));

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), "B", "1", 1);
    fixture.MakeSupport(fixture.GetCoinbase(), tx2, "B", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("B", 2));

    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), "C", "1", 1);
    fixture.MakeSupport(fixture.GetCoinbase(), tx3, "C", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("B", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("C", 2));

    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), "D", "1", 1);
    fixture.MakeSupport(fixture.GetCoinbase(), tx4, "D", 1);
    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("d", 2));

    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("d", 2));

    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("d", 2));

    fixture.IncrementBlocks(1);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("d", 2));

    fixture.DecrementBlocks(2);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("d", 2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("d", 2));

    fixture.DecrementBlocks(1);
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("A", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("B", 2));
    BOOST_CHECK(fixture.best_claim_effective_amount_equals("C", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("d", 2));

    fixture.IncrementBlocks(3);
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("a", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("b", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("c", 2));
    BOOST_CHECK(!fixture.best_claim_effective_amount_equals("d", 2)); // (not re-added)
}

BOOST_AUTO_TEST_SUITE_END()
