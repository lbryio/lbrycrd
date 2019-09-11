#include <claimtrie.h>
#include <nameclaim.h>
#include <uint256.h>
#include <validation.h>

#include <test/claimtriefixture.h>
#include <test/test_bitcoin.h>
#include <boost/test/unit_test.hpp>
#include <boost/scope_exit.hpp>

using namespace std;

class CClaimTrieCacheTest : public CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheTest(CClaimTrie* base): CClaimTrieCacheBase(base)
    {
    }

    using CClaimTrieCacheBase::insertSupportIntoMap;
    using CClaimTrieCacheBase::removeSupportFromMap;
    using CClaimTrieCacheBase::insertClaimIntoTrie;
    using CClaimTrieCacheBase::removeClaimFromTrie;

    void insert(const std::string& key, CClaimTrieData&& data)
    {
        nodesToAddOrUpdate.insert(key, std::move(data));
    }

    bool erase(const std::string& key)
    {
        return nodesToAddOrUpdate.erase(key);
    }

    int cacheSize()
    {
        return nodesToAddOrUpdate.height();
    }

    CClaimPrefixTrie::iterator getCache(const std::string& key)
    {
        return nodesToAddOrUpdate.find(key);
    }
};

BOOST_FIXTURE_TEST_SUITE(claimtriecache_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(merkle_hash_single_test)
{
    // check empty trie
    uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CClaimTrieCacheTest cc(pclaimTrie);
    BOOST_CHECK_EQUAL(one, cc.getMerkleHash());

    // we cannot have leaf root node
    auto it = cc.getCache("");
    BOOST_CHECK(!it);
}

BOOST_AUTO_TEST_CASE(merkle_hash_multiple_test)
{
    CClaimValue unused;
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    uint160 hash160;
    CMutableTransaction tx1 = BuildTransaction(hash0);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    COutPoint tx2OutPoint(tx2.GetHash(), 0);
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    COutPoint tx3OutPoint(tx3.GetHash(), 0);
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    COutPoint tx4OutPoint(tx4.GetHash(), 0);
    CMutableTransaction tx5 = BuildTransaction(tx4.GetHash());
    COutPoint tx5OutPoint(tx5.GetHash(), 0);
    CMutableTransaction tx6 = BuildTransaction(tx5.GetHash());
    COutPoint tx6OutPoint(tx6.GetHash(), 0);

    uint256 hash1;
    hash1.SetHex("71c7b8d35b9a3d7ad9a1272b68972979bbd18589f1efe6f27b0bf260a6ba78fa");

    uint256 hash2;
    hash2.SetHex("c4fc0e2ad56562a636a0a237a96a5f250ef53495c2cb5edd531f087a8de83722");

    uint256 hash3;
    hash3.SetHex("baf52472bd7da19fe1e35116cfb3bd180d8770ffbe3ae9243df1fb58a14b0975");

    uint256 hash4;
    hash4.SetHex("c73232a755bf015f22eaa611b283ff38100f2a23fb6222e86eca363452ba0c51");

    BOOST_CHECK(pclaimTrie->empty());

    CClaimTrieCacheTest ntState(pclaimTrie);
    ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1OutPoint, hash160, 50, 100, 200), true);
    ntState.insertClaimIntoTrie(std::string("test2"), CClaimValue(tx2OutPoint, hash160, 50, 100, 200), true);

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!ntState.empty());
    BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash1);

    ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3OutPoint, hash160, 50, 101, 201), true);
    BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash1);
    ntState.insertClaimIntoTrie(std::string("tes"), CClaimValue(tx4OutPoint, hash160, 50, 100, 200), true);
    BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash2);
    ntState.insertClaimIntoTrie(std::string("testtesttesttest"), CClaimValue(tx5OutPoint, hash160, 50, 100, 200), true);
    ntState.removeClaimFromTrie(std::string("testtesttesttest"), tx5OutPoint, unused, true);
    BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash2);
    ntState.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash2));

    CClaimTrieCacheTest ntState1(pclaimTrie);
    ntState1.removeClaimFromTrie(std::string("test"), tx1OutPoint, unused, true);
    ntState1.removeClaimFromTrie(std::string("test2"), tx2OutPoint, unused, true);
    ntState1.removeClaimFromTrie(std::string("test"), tx3OutPoint, unused, true);
    ntState1.removeClaimFromTrie(std::string("tes"), tx4OutPoint, unused, true);

    BOOST_CHECK_EQUAL(ntState1.getMerkleHash(), hash0);

    CClaimTrieCacheTest ntState2(pclaimTrie);
    ntState2.insertClaimIntoTrie(std::string("abab"), CClaimValue(tx6OutPoint, hash160, 50, 100, 200), true);
    ntState2.removeClaimFromTrie(std::string("test"), tx1OutPoint, unused, true);

    BOOST_CHECK_EQUAL(ntState2.getMerkleHash(), hash3);

    ntState2.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState2.getMerkleHash(), hash3);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash3));

    CClaimTrieCacheTest ntState3(pclaimTrie);
    ntState3.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1OutPoint, hash160, 50, 100, 200), true);
    BOOST_CHECK_EQUAL(ntState3.getMerkleHash(), hash4);
    ntState3.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState3.getMerkleHash(), hash4);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash4));

    CClaimTrieCacheTest ntState4(pclaimTrie);
    ntState4.removeClaimFromTrie(std::string("abab"), tx6OutPoint, unused, true);
    BOOST_CHECK_EQUAL(ntState4.getMerkleHash(), hash2);
    ntState4.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState4.getMerkleHash(), hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash2));

    CClaimTrieCacheTest ntState5(pclaimTrie);
    ntState5.removeClaimFromTrie(std::string("test"), tx3OutPoint, unused, true);

    BOOST_CHECK_EQUAL(ntState5.getMerkleHash(), hash2);
    ntState5.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState5.getMerkleHash(), hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash2));

    CClaimTrieCacheTest ntState6(pclaimTrie);
    ntState6.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3OutPoint, hash160, 50, 101, 201), true);

    BOOST_CHECK_EQUAL(ntState6.getMerkleHash(), hash2);
    ntState6.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState6.getMerkleHash(), hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash2));

    CClaimTrieCacheTest ntState7(pclaimTrie);
    ntState7.removeClaimFromTrie(std::string("test"), tx3OutPoint, unused, true);
    ntState7.removeClaimFromTrie(std::string("test"), tx1OutPoint, unused, true);
    ntState7.removeClaimFromTrie(std::string("tes"), tx4OutPoint, unused, true);
    ntState7.removeClaimFromTrie(std::string("test2"), tx2OutPoint, unused, true);

    BOOST_CHECK_EQUAL(ntState7.getMerkleHash(), hash0);
    ntState7.flush();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK_EQUAL(ntState7.getMerkleHash(), hash0);
    BOOST_CHECK(pclaimTrie->checkConsistency(hash0));
}

BOOST_AUTO_TEST_CASE(basic_insertion_info_test)
{
    // test basic claim insertions and that get methods retreives information properly
    BOOST_CHECK(pclaimTrie->empty());
    CClaimTrieCacheTest ctc(pclaimTrie);

    // create and insert claim
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    COutPoint claimOutPoint(tx1.GetHash(), 0);
    CAmount amount(10);
    int height = 0;
    int validHeight = 0;
    CClaimValue claimVal(claimOutPoint, claimId, amount, height, validHeight);
    ctc.insertClaimIntoTrie("test", claimVal, true);

    // try getClaimsForName, effectiveAmount, getInfoForName
    auto res = ctc.getClaimsForName("test");
    BOOST_CHECK_EQUAL(res.claimsNsupports.size(), 1);
    BOOST_CHECK_EQUAL(res.claimsNsupports[0].claim, claimVal);
    BOOST_CHECK_EQUAL(res.claimsNsupports[0].supports.size(), 0);

    BOOST_CHECK_EQUAL(10, res.claimsNsupports[0].effectiveAmount);

    CClaimValue claim;
    BOOST_CHECK(ctc.getInfoForName("test", claim));
    BOOST_CHECK_EQUAL(claim, claimVal);

    // insert a support
    CAmount supportAmount(10);
    uint256 hash1(uint256S("0000000000000000000000000000000000000000000000000000000000000002"));
    CMutableTransaction tx2 = BuildTransaction(hash1);
    COutPoint supportOutPoint(tx2.GetHash(), 0);

    CSupportValue support(supportOutPoint, claimId, supportAmount, height, validHeight);
    ctc.insertSupportIntoMap("test", support, false);

    auto res1 = ctc.getClaimsForName("test");
    BOOST_CHECK_EQUAL(res1.claimsNsupports.size(), 1);
    BOOST_CHECK_EQUAL(res1.claimsNsupports[0].supports.size(), 1);

    // try getEffectiveAmount
    BOOST_CHECK_EQUAL(20, res1.claimsNsupports[0].effectiveAmount);
}

BOOST_AUTO_TEST_CASE(recursive_prune_test)
{
    CClaimTrieCacheTest cc(pclaimTrie);
    BOOST_CHECK_EQUAL(0, cc.cacheSize());

    COutPoint outpoint;
    uint160 claimId;
    CAmount amount(20);
    int height = 0;
    int validAtHeight = 0;
    CClaimValue test_claim(outpoint, claimId, amount, height, validAtHeight);

    CClaimTrieData data;
    // base node has a claim, so it should not be pruned
    data.insertClaim(test_claim);
    cc.insert("", std::move(data));

    // node 1 has a claim so it should not be pruned
    data.insertClaim(test_claim);
    // set this just to make sure we get the right CClaimTrieNode back
    data.nHeightOfLastTakeover = 10;
    cc.insert("t", std::move(data));

    //node 2 does not have a claim so it should be pruned
    // thus we should find pruned node 1 in cache
    cc.insert("te", CClaimTrieData{});

    BOOST_CHECK(cc.erase("te"));
    BOOST_CHECK_EQUAL(2, cc.cacheSize());
    auto it = cc.getCache("t");
    BOOST_CHECK_EQUAL(10, it->nHeightOfLastTakeover);
    BOOST_CHECK_EQUAL(1, it->claims.size());
    BOOST_CHECK_EQUAL(2, cc.cacheSize());

    cc.insert("te", CClaimTrieData{});
    // erasing "t" will make it weak
    BOOST_CHECK(cc.erase("t"));
    // so now we erase "e" as well as "t"
    BOOST_CHECK(cc.erase("te"));
    // we have claim in root
    BOOST_CHECK_EQUAL(1, cc.cacheSize());
    BOOST_CHECK(cc.erase(""));
    BOOST_CHECK_EQUAL(0, cc.cacheSize());
}

BOOST_AUTO_TEST_CASE(iteratetrie_test)
{
    BOOST_CHECK(pclaimTrie->empty());
    CClaimTrieCacheTest ctc(pclaimTrie);

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);

    const uint256 txhash = tx1.GetHash();
    CClaimValue claimVal(COutPoint(txhash, 0), ClaimIdHash(txhash, 0), CAmount(10), 0, 0);
    ctc.insertClaimIntoTrie("test", claimVal, true);
    BOOST_CHECK(ctc.flush());

    CClaimTrieDataNode node;
    BOOST_CHECK(pclaimTrie->find("", node));
    BOOST_CHECK_EQUAL(node.children.size(), 1U);
    BOOST_CHECK(pclaimTrie->find("test", node));
    BOOST_CHECK_EQUAL(node.children.size(), 0U);
    CClaimTrieData data;
    BOOST_CHECK(pclaimTrie->find("test", data));
    BOOST_CHECK_EQUAL(data.claims.size(), 1);
}

BOOST_AUTO_TEST_CASE(trie_stays_consistent_test)
{
    std::vector<std::string> names {
        "goodness", "goodnight", "goodnatured", "goods", "go", "goody", "goo"
    };

    CClaimTrie trie(true, false, 1);
    CClaimTrieCacheTest cache(&trie);
    CClaimValue value;

    for (auto& name: names)
        BOOST_CHECK(cache.insertClaimIntoTrie(name, value, false));

    cache.flush();
    BOOST_CHECK(trie.checkConsistency(cache.getMerkleHash()));

    for (auto& name: names) {
        CClaimValue temp;
        BOOST_CHECK(cache.removeClaimFromTrie(name, COutPoint(), temp, false));
        cache.flush();
        BOOST_CHECK(trie.checkConsistency(cache.getMerkleHash()));
    }
    BOOST_CHECK(trie.empty());
}

BOOST_AUTO_TEST_CASE(takeover_workaround_triggers)
{
    auto& consensus = const_cast<Consensus::Params&>(Params().GetConsensus());
    auto currentMax = consensus.nMaxTakeoverWorkaroundHeight;
    consensus.nMaxTakeoverWorkaroundHeight = 10000;
    BOOST_SCOPE_EXIT(&consensus, currentMax) { consensus.nMaxTakeoverWorkaroundHeight = currentMax; }
    BOOST_SCOPE_EXIT_END

    CClaimTrie trie(true, false, 1);
    CClaimTrieCacheTest cache(&trie);

    insertUndoType icu, isu; claimQueueRowType ecu; supportQueueRowType esu;
    std::vector<std::pair<std::string, int>> thu;
    BOOST_CHECK(cache.incrementBlock(icu, ecu, isu, esu, thu));

    CClaimValue value;
    value.nHeight = 1;

    BOOST_CHECK(cache.insertClaimIntoTrie("a", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("b", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("c", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("aa", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("bb", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("cc", value, true));
    BOOST_CHECK(cache.insertSupportIntoMap("aa", CSupportValue(), false));

    BOOST_CHECK(cache.incrementBlock(icu, ecu, isu, esu, thu));
    BOOST_CHECK(cache.flush());
    BOOST_CHECK(cache.incrementBlock(icu, ecu, isu, esu, thu));
    BOOST_CHECK_EQUAL(0, cache.cacheSize());

    CSupportValue temp;
    BOOST_CHECK(cache.insertSupportIntoMap("bb", temp, false));
    BOOST_CHECK(!cache.getCache("aa"));
    BOOST_CHECK(cache.removeSupportFromMap("aa", COutPoint(), temp, false));

    BOOST_CHECK(cache.removeClaimFromTrie("aa", COutPoint(), value, false));
    BOOST_CHECK(cache.removeClaimFromTrie("bb", COutPoint(), value, false));
    BOOST_CHECK(cache.removeClaimFromTrie("cc", COutPoint(), value, false));

    BOOST_CHECK(cache.insertClaimIntoTrie("aa", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("bb", value, true));
    BOOST_CHECK(cache.insertClaimIntoTrie("cc", value, true));

    BOOST_CHECK(cache.incrementBlock(icu, ecu, isu, esu, thu));

    BOOST_CHECK_EQUAL(3, cache.getCache("aa")->nHeightOfLastTakeover);
    BOOST_CHECK_EQUAL(3, cache.getCache("bb")->nHeightOfLastTakeover);
    BOOST_CHECK_EQUAL(1, cache.getCache("cc")->nHeightOfLastTakeover);
}

BOOST_AUTO_TEST_CASE(verify_basic_serialization)
{
    CClaimValue cv;
    cv.outPoint = COutPoint(uint256S("123"), 2);
    cv.nHeight = 3;
    cv.claimId.SetHex("4567");
    cv.nEffectiveAmount = 4;
    cv.nAmount = 5;
    cv.nValidAtHeight = 6;

    CDataStream ssData(SER_NETWORK, PROTOCOL_VERSION);
    ssData << cv;

    CClaimValue cv2;
    ssData >> cv2;

    BOOST_CHECK_EQUAL(cv, cv2);
    BOOST_CHECK_EQUAL(cv.nEffectiveAmount, cv2.nEffectiveAmount);
}

BOOST_AUTO_TEST_CASE(claimtrienode_serialize_unserialize)
{
    CDataStream ss(SER_DISK, 0);

    uint160 hash160;

    CClaimTrieData n1;
    CClaimTrieData n2;
    CClaimValue throwaway;

    ss << n1;
    ss >> n2;
    BOOST_CHECK_EQUAL(n1, n2);

    CClaimValue v1(COutPoint(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0), hash160, 50, 0, 100);
    CClaimValue v2(COutPoint(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1), hash160, 100, 1, 101);

    n1.insertClaim(v1);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK_EQUAL(n1, n2);

    n1.insertClaim(v2);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK_EQUAL(n1, n2);

    n1.removeClaim(v1.outPoint, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK_EQUAL(n1, n2);

    n1.removeClaim(v2.outPoint, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK_EQUAL(n1, n2);
}

BOOST_AUTO_TEST_CASE(claimtrienode_remove_invalid_claim)
{
    uint160 hash160;

    CClaimTrieData n1;
    CClaimTrieData n2;
    CClaimValue throwaway;

    CClaimValue v1(COutPoint(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0), hash160, 50, 0, 100);
    CClaimValue v2(COutPoint(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1), hash160, 100, 1, 101);

    n1.insertClaim(v1);
    n2.insertClaim(v2);

    bool invalidClaim = n2.removeClaim(v1.outPoint, throwaway);
    BOOST_CHECK_EQUAL(invalidClaim, false);

    invalidClaim = n1.removeClaim(v2.outPoint, throwaway);
    BOOST_CHECK_EQUAL(invalidClaim, false);
}

BOOST_AUTO_TEST_SUITE_END()
