
package main

import(
    "testing"
    ."libclaimtrie"
)

func assertEqual(t *testing.T, a interface{}, b interface{}, msg string) {
    if a != b {
        t.Fatalf("%s != %s, %s", a, b, msg)
    }
}

func assertNotEqual(t *testing.T, a interface{}, b interface{}, msg string) {
    if a == b {
        t.Fatalf("%s == %s, %s", a, b, msg)
    }
}

func assertFalse(t *testing.T, a interface{}, msg string) {
    if a != false {
        t.Fatalf("%s != false, %s", a, msg)
    }
}

func assertTrue(t *testing.T, a interface{}, msg string) {
    if a != true {
        t.Fatalf("%s != true, %s", a, msg)
    }
}

func assertClaimEqual(t *testing.T, claim CClaimValue, txo COutPoint, cid Uint160, amount int64, effe int64, height int, validHeight int,  msg string) {
    assertEqual(t, claim.GetOutPoint(), txo, msg)
    assertEqual(t, claim.GetClaimId(), cid, msg)
    assertEqual(t, claim.GetNAmount(), amount, msg)
    assertEqual(t, claim.GetNEffectiveAmount(), effe, msg)
    assertEqual(t, claim.GetNHeight(), height, msg)
    assertEqual(t, claim.GetNValidAtHeight(), validHeight, msg)
}

func assertSupportEqual(t *testing.T, support CSupportValue, txo COutPoint, cid Uint160, amount int64, height int, validHeight int,  msg string) {
    assertEqual(t, support.GetOutPoint(), txo, msg)
    assertEqual(t, support.GetSupportedClaimId(), cid, msg)
    assertEqual(t, support.GetNAmount(), amount, msg)
    assertEqual(t, support.GetNHeight(), height, msg)
    assertEqual(t, support.GetNValidAtHeight(), validHeight, msg)
}

var uint256s = "1234567890987654321012345678909876543210123456789098765432101234"
var uint160 = Uint160S("1234567890987654321012345678909876543210")
var uint256 = Uint256S(uint256s)
var txp = NewCOutPoint(uint256, uint(1))

func Test_uint256(t *testing.T) {
    assertFalse(t, uint256.IsNull(), "incorrect uint256S or CBaseBlob::IsNull")
    assertEqual(t, uint256.GetHex(), uint256s, "incorrect CBaseBlob::GetHex")
    assertEqual(t, uint256.GetHex(), uint256.ToString(), "incorrect CBaseBlob::ToString")
    assertEqual(t, uint256.Size(), 32, "incorrect CBaseBlob::size")
    uint256c := NewUint256()
    assertNotEqual(t, uint256c, uint256, "incorrect CBaseBlob::operator!=")
    assertTrue(t, uint256c.IsNull(), "incorrect CBaseBlob::IsNull")
    uint256c = NewUint256(uint256)
    assertEqual(t, uint256c, uint256, "incorrect CBaseBlob::operator==")
    uint256c.SetNull()
    assertTrue(t, uint256c.IsNull(), "incorrect CBaseBlob::SetNull")
}

func Test_txoupoint(t *testing.T) {
    assertEqual(t, txp.GetHash(), uint256, "incorrect COutPoint::COutPoint")
    assertEqual(t, txp.GetN(), 1, "incorrect COutPoint::COutPoint")
    assertFalse(t, txp.IsNull(), "incorrect COutPoint::IsNull")
    pcopy := NewCOutPoint()
    assertTrue(t, pcopy.IsNull(), "incorrect COutPoint::IsNull")
    assertEqual(t, pcopy.GetHash(), NewUint256(), "incorrect COutPoint::COutPoint")
    assertNotEqual(t, pcopy, txp, "incorrect COutPoint::operator!=")
}

func Test_claim(t *testing.T) {
    assertEqual(t, uint160.Size(), 20, "incorrect CBaseBlob::size")
    claim := NewCClaimValue(txp, uint160, 20, 1, 10)
    assertClaimEqual(t, claim, txp, uint160, 20, 20, 1, 10, "incorrect CClaimValue::CClaimValue")
}

func Test_support(t *testing.T) {
    claim := NewCClaimValue(txp, uint160, 20, 1, 10)
    support := NewCSupportValue(txp, uint160, 20, 1, 10)
    assertSupportEqual(t, support, claim.GetOutPoint(), claim.GetClaimId(), claim.GetNAmount(), claim.GetNHeight(), claim.GetNValidAtHeight(), "incorrect CSupportValue::CSupportValue")
}

func Test_claimtrie(t *testing.T) {
    claim := NewCClaimValue(txp, uint160, 20, 0, 0)
    wipe := true; height := 1; data_dir := "."
    trie := NewCClaimTrie(10*1024*1024, wipe, height, data_dir)
    cache := NewCClaimTrieCache(trie)
    assertTrue(t, trie.Empty(), "incorrect CClaimtrieCache::empty")
    assertTrue(t, cache.AddClaim("test", txp, uint160, 20, 0, 0), "incorrect CClaimtrieCache::addClaim")
    assertTrue(t, cache.HaveClaim("test", txp), "incorrect CClaimtrieCache::haveClaim")
    assertEqual(t, cache.GetTotalNamesInTrie(), 1, "incorrect CClaimtrieCache::getTotalNamesInTrie")
    assertEqual(t, cache.GetTotalClaimsInTrie(), 1, "incorrect CClaimtrieCache::getTotalClaimsInTrie")
    var nValidAtHeight []int
    // add second claim
    txp.SetN(2)
    uint1601 := Uint160S("1234567890987654321012345678909876543211")
    assertTrue(t, cache.AddClaim("test", txp, uint1601, 20, 1, 1), "incorrect CClaimtrieCache::addClaim")
    result := cache.HaveClaimInQueue("test", txp, nValidAtHeight)
    assertTrue(t, result, "incorrect CClaimTrieCache::haveClaimInQueue")
    assertEqual(t, nValidAtHeight[0], 1, "incorrect CClaimTrieCache::haveClaimInQueue, nValidAtHeight")
    claim1 := NewCClaimValue()
    assertTrue(t, cache.GetInfoForName("test", claim1), "incorrect CClaimTrieCache::getInfoForName")
    assertEqual(t, claim, claim1, "incorrect CClaimtrieCache::getInfoForName")
    proof := NewCClaimTrieProof()
    assertTrue(t, cache.GetProofForName("test", uint160, proof), "incorrect CacheProofCallback")
    assertTrue(t, proof.GetHasValue(), "incorrect CClaimTrieCache::getProofForName")
    claimsToName := cache.GetClaimsForName("test")
    claims := claimsToName.GetClaimsNsupports()
    assertEqual(t, claims.Size(), 2, "incorrect CClaimTrieCache::getClaimsForName")
    assertFalse(t, claims.Get(0).IsNull(), "incorrect CClaimNsupports::IsNull")
    assertFalse(t, claims.Get(1).IsNull(), "incorrect CClaimNsupports::IsNull")
}
