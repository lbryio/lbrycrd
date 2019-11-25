
from libclaimtrie import *
import unittest

class CacheIterateCallback(CIterateCallback):
    def __init__(self, names):
        CIterateCallback.__init__(self)
        self.names = names

    def apply(self, name):
        assert(name in self.names), "Incorrect trie names"

class TestClaimTrieTypes(unittest.TestCase):
    def setUp(self):
        self.uint256s = "1234567890987654321012345678909876543210123456789098765432101234"
        self.uint160 = CUint160S("1234567890987654321012345678909876543210")
        self.uint = CUint256S(self.uint256s)
        self.txp = CTxOutPoint(self.uint, 1)

    def assertClaimEqual(self, claim, txo, cid, amount, effe, height, validHeight, msg):
        self.assertEqual(claim.outPoint, txo, msg)
        self.assertEqual(claim.claimId, cid, msg)
        self.assertEqual(claim.nAmount, amount, msg)
        self.assertEqual(claim.nEffectiveAmount, effe, msg)
        self.assertEqual(claim.nHeight, height, msg)
        self.assertEqual(claim.nValidAtHeight, validHeight, msg)

    def assertSupportEqual(self, support, txo, cid, amount, height, validHeight, msg):
        self.assertEqual(support.outPoint, txo, msg)
        self.assertEqual(support.supportedClaimId, cid, msg)
        self.assertEqual(support.nAmount, amount, msg)
        self.assertEqual(support.nHeight, height, msg)
        self.assertEqual(support.nValidAtHeight, validHeight, msg)

    def test_uint256(self):
        uint = self.uint
        self.assertFalse(uint.IsNull(), "incorrect CUint256S or CBaseBlob::IsNull")
        self.assertEqual(uint.GetHex(), self.uint256s, "incorrect CBaseBlob::GetHex")
        self.assertEqual(uint.GetHex(), uint.ToString(), "incorrect CBaseBlob::ToString")
        self.assertEqual(uint.size(), 32, "incorrect CBaseBlob::size")
        copy = CUint256()
        self.assertNotEqual(copy, uint, "incorrect CBaseBlob::operator!=")
        self.assertTrue(copy.IsNull(), "incorrect CBaseBlob::IsNull")
        copy = CUint256(uint)
        self.assertEqual(copy, uint, "incorrect CBaseBlob::operator==")
        copy.SetNull()
        self.assertTrue(copy.IsNull()), "incorrect CBaseBlob::SetNull"

    def test_txoupoint(self):
        txp = self.txp
        uint = self.uint
        self.assertEqual(txp.hash, uint, "incorrect CTxOutPoint::CTxOutPoint")
        self.assertEqual(txp.n, 1, "incorrect CTxOutPoint::CTxOutPoint")
        self.assertFalse(txp.IsNull(), "incorrect CTxOutPoint::IsNull")
        pcopy = CTxOutPoint()
        self.assertTrue(pcopy.IsNull(), "incorrect CTxOutPoint::IsNull")
        self.assertEqual(pcopy.hash, CUint256(), "incorrect CTxOutPoint::CTxOutPoint")
        self.assertNotEqual(pcopy, txp, "incorrect CTxOutPoint::operator!=")
        self.assertIn(uint.ToString()[:10], txp.ToString(), "incorrect CTxOutPoint::ToString")

    def test_claim(self):
        txp = self.txp
        uint160 = self.uint160
        self.assertEqual(uint160.size(), 20, "incorrect CBaseBlob::size")
        claim = CClaimValue(txp, uint160, 20, 1, 10)
        self.assertClaimEqual(claim, txp, uint160, 20, 20, 1, 10, "incorrect CClaimValue::CClaimValue")

    def test_support(self):
        txp = self.txp
        uint160 = self.uint160
        claim = CClaimValue(txp, uint160, 20, 1, 10)
        support = CSupportValue(txp, uint160, 20, 1, 10)
        self.assertSupportEqual(support, claim.outPoint, claim.claimId, claim.nAmount, claim.nHeight, claim.nValidAtHeight, "incorrect CSupportValue::CSupportValue")

    def test_claimtrie(self):
        txp = self.txp
        uint160 = self.uint160
        claim = CClaimValue(txp, uint160, 20, 0, 0)
        wipe = True; height = 1; data_dir = "."
        trie = CClaimTrie(wipe, height, data_dir)
        cache = CClaimTrieCache(trie)
        self.assertTrue(trie.empty(), "incorrect CClaimtrieCache::empty")
        self.assertTrue(cache.addClaim("test", txp, uint160, 20, 0, 0), "incorrect CClaimtrieCache::addClaim")
        self.assertTrue(cache.haveClaim("test", txp), "incorrect CClaimtrieCache::haveClaim")
        self.assertEqual(cache.getTotalNamesInTrie(), 1, "incorrect CClaimtrieCache::getTotalNamesInTrie")
        self.assertEqual(cache.getTotalClaimsInTrie(), 1, "incorrect CClaimtrieCache::getTotalClaimsInTrie")
        getNamesInTrie(cache, CacheIterateCallback(["test"]))
        nValidAtHeight = -1
        # add second claim
        txp.n = 2
        uint1601 = CUint160S("1234567890987654321012345678909876543211")
        self.assertTrue(cache.addClaim("test", txp, uint1601, 20, 1, 1), "incorrect CClaimtrieCache::addClaim")
        result, nValidAtHeight = cache.haveClaimInQueue("test", txp)
        self.assertTrue(result, "incorrect CClaimTrieCache::haveClaimInQueue")
        self.assertEqual(nValidAtHeight, 1, "incorrect CClaimTrieCache::haveClaimInQueue, nValidAtHeight")
        claim1 = CClaimValue()
        self.assertTrue(cache.getInfoForName("test", claim1), "incorrect CClaimTrieCache::getInfoForName")
        self.assertEqual(claim, claim1, "incorrect CClaimtrieCache::getInfoForName")
        proof = CClaimTrieProof()
        self.assertTrue(cache.getProofForName("test", uint160, proof), "incorrect CacheProofCallback")
        self.assertTrue(proof.hasValue, "incorrect CClaimTrieCache::getProofForName")
        claimsToName = cache.getClaimsForName("test")
        claims = claimsToName.claimsNsupports
        self.assertEqual(claims.size(), 2, "incorrect CClaimTrieCache::getClaimsForName")
        self.assertFalse(claims[0].IsNull(), "incorrect CClaimNsupports::IsNull")
        self.assertFalse(claims[1].IsNull(), "incorrect CClaimNsupports::IsNull")

unittest.main()
