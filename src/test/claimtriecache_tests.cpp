
#include <nameclaim.h>
#include <uint256.h>
#include <validation.h>

#include <test/claimtriefixture.h>
#include <boost/test/unit_test.hpp>
#include <boost/scope_exit.hpp>

using namespace std;

class CClaimTrieCacheTest : public CClaimTrieCacheBase
{
public:
    explicit CClaimTrieCacheTest(CClaimTrie* base): CClaimTrieCacheBase(base)
    {
        nNextHeight = 2;
    }

    bool insertClaimIntoTrie(const std::string& key, const CClaimValue& value)
    {
        auto p = value.outPoint;
        if (p.hash.IsNull())
            p.hash = Hash(key.begin(), key.end());
        auto c = value.claimId;
        if (c.IsNull())
            c = ClaimIdHash(uint256(p.hash), p.n);

        return addClaim(key, p, c, value.nAmount, value.nHeight);
    }

    bool removeClaimFromTrie(const std::string& key, const COutPoint& outPoint) {
        int validHeight;
        std::string nodeName;

        auto p = outPoint;
        if (p.hash.IsNull())
            p.hash = Hash(key.begin(), key.end());

        auto ret = removeClaim(ClaimIdHash(uint256(p.hash), p.n), p, nodeName, validHeight);
        assert(!ret || nodeName == key);
        return ret;
    }

    bool insertSupportIntoMap(const std::string& key, const CSupportValue& value) {
        auto p = value.outPoint;
        if (p.hash.IsNull())
            p.hash = Hash(key.begin(), key.end());

        return addSupport(key, p, value.supportedClaimId, value.nAmount, value.nHeight);
    }

    bool removeSupportFromMap(const std::string& key, const COutPoint& outPoint) {
        int validHeight;
        std::string nodeName;

        auto p = outPoint;
        if (p.hash.IsNull())
            p.hash = Hash(key.begin(), key.end());

        auto ret = removeSupport(p, nodeName, validHeight);
        assert(!ret || nodeName == key);
        return ret;
    }
};

BOOST_FIXTURE_TEST_SUITE(claimtriecache_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(merkle_hash_single_test)
{
    // check empty trie
    auto one = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    CClaimTrieCacheTest cc(&::Claimtrie());
    BOOST_CHECK_EQUAL(one, cc.getMerkleHash());

    // we cannot have leaf root node
    auto it = cc.getClaimsForName("");
    BOOST_CHECK_EQUAL(it.claimsNsupports.size(), 0U);
}

BOOST_AUTO_TEST_CASE(merkle_hash_multiple_test)
{
    auto hash0 = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
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

    {
        CClaimTrieCacheTest ntState(&::Claimtrie());
        ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1OutPoint, {}, 50, 0, 0));
        ntState.insertClaimIntoTrie(std::string("test2"), CClaimValue(tx2OutPoint, {}, 50, 0, 0));

        BOOST_CHECK(::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState.getTotalClaimsInTrie(), 2U);
        BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash1);

        ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3OutPoint, {}, 50, 1, 1));
        BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash1);
        ntState.insertClaimIntoTrie(std::string("tes"), CClaimValue(tx4OutPoint, {}, 50, 0, 0));
        BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash2);
        ntState.insertClaimIntoTrie(std::string("testtesttesttest"),
                                    CClaimValue(tx5OutPoint, {}, 50, 0, 0));
        ntState.removeClaimFromTrie(std::string("testtesttesttest"), tx5OutPoint);
        BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash2);
        ntState.flush();

        BOOST_CHECK(!::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState.getMerkleHash(), hash2);
        BOOST_CHECK(ntState.checkConsistency());
    }
    {
        CClaimTrieCacheTest ntState1(&::Claimtrie());
        ntState1.removeClaimFromTrie(std::string("test"), tx1OutPoint);
        ntState1.removeClaimFromTrie(std::string("test2"), tx2OutPoint);
        ntState1.removeClaimFromTrie(std::string("test"), tx3OutPoint);
        ntState1.removeClaimFromTrie(std::string("tes"), tx4OutPoint);

        BOOST_CHECK_EQUAL(ntState1.getMerkleHash(), hash0);
    }
    {
        CClaimTrieCacheTest ntState2(&::Claimtrie());
        ntState2.insertClaimIntoTrie(std::string("abab"), CClaimValue(tx6OutPoint, {}, 50, 0, 200));
        ntState2.removeClaimFromTrie(std::string("test"), tx1OutPoint);

        BOOST_CHECK_EQUAL(ntState2.getMerkleHash(), hash3);

        ntState2.flush();

        BOOST_CHECK(!::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState2.getMerkleHash(), hash3);
        BOOST_CHECK(ntState2.checkConsistency());
    }
    {
        CClaimTrieCacheTest ntState3(&::Claimtrie());
        ntState3.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1OutPoint, {}, 50, 0, 0));
        BOOST_CHECK_EQUAL(ntState3.getMerkleHash(), hash4);
        ntState3.flush();
        BOOST_CHECK(!::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState3.getMerkleHash(), hash4);
        BOOST_CHECK(ntState3.checkConsistency());
    }
    {
        CClaimTrieCacheTest ntState4(&::Claimtrie());
        ntState4.removeClaimFromTrie(std::string("abab"), tx6OutPoint);
        BOOST_CHECK_EQUAL(ntState4.getMerkleHash(), hash2);
        ntState4.flush();
        BOOST_CHECK(!::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState4.getMerkleHash(), hash2);
        BOOST_CHECK(ntState4.checkConsistency());
    }
    {
        CClaimTrieCacheTest ntState5(&::Claimtrie());
        ntState5.removeClaimFromTrie(std::string("test"), tx3OutPoint);

        BOOST_CHECK_EQUAL(ntState5.getMerkleHash(), hash2);
        ntState5.flush();
        BOOST_CHECK(!::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState5.getMerkleHash(), hash2);
        BOOST_CHECK(ntState5.checkConsistency());
    }
    {
        CClaimTrieCacheTest ntState6(&::Claimtrie());
        ntState6.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3OutPoint, {}, 50, 1, 1));

        BOOST_CHECK_EQUAL(ntState6.getMerkleHash(), hash2);
        ntState6.flush();
        BOOST_CHECK(!::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState6.getMerkleHash(), hash2);
        BOOST_CHECK(ntState6.checkConsistency());
    }
    {
        CClaimTrieCacheTest ntState7(&::Claimtrie());
        ntState7.removeClaimFromTrie(std::string("test"), tx3OutPoint);
        ntState7.removeClaimFromTrie(std::string("test"), tx1OutPoint);
        ntState7.removeClaimFromTrie(std::string("tes"), tx4OutPoint);
        ntState7.removeClaimFromTrie(std::string("test2"), tx2OutPoint);

        BOOST_CHECK_EQUAL(ntState7.getMerkleHash(), hash0);
        ntState7.flush();
        BOOST_CHECK(::Claimtrie().empty());
        BOOST_CHECK_EQUAL(ntState7.getMerkleHash(), hash0);
        BOOST_CHECK(ntState7.checkConsistency());
    }
}

BOOST_AUTO_TEST_CASE(basic_insertion_info_test)
{
    // test basic claim insertions and that get methods retreives information properly
    BOOST_CHECK(::Claimtrie().empty());
    CClaimTrieCacheTest ctc(&::Claimtrie());

    // create and insert claim
    auto hash0 = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
    CMutableTransaction tx1 = BuildTransaction(hash0);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);
    COutPoint claimOutPoint(tx1.GetHash(), 0);
    CAmount amount(10);
    int height = 0;
    int validHeight = 0;
    CClaimValue claimVal(claimOutPoint, claimId, amount, height, validHeight);
    ctc.insertClaimIntoTrie("test", claimVal);

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
    auto hash1 = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    CMutableTransaction tx2 = BuildTransaction(hash1);
    COutPoint supportOutPoint(tx2.GetHash(), 0);

    CSupportValue support(supportOutPoint, claimId, supportAmount, height, validHeight);
    ctc.insertSupportIntoMap("test", support);

    auto res1 = ctc.getClaimsForName("test");
    BOOST_CHECK_EQUAL(res1.claimsNsupports.size(), 1);
    BOOST_CHECK_EQUAL(res1.claimsNsupports[0].supports.size(), 1);

    // try getEffectiveAmount
    BOOST_CHECK_EQUAL(20, res1.claimsNsupports[0].effectiveAmount);
}

//BOOST_AUTO_TEST_CASE(recursive_prune_test)
//{
//    CClaimTrieCacheTest cc(&::Claimtrie());
//    BOOST_CHECK_EQUAL(0, cc.getTotalClaimsInTrie());
//
//    COutPoint outpoint;
//    uint160 claimId;
//    CAmount amount(20);
//    int height = 0;
//    int validAtHeight = 0;
//    CClaimValue test_claim(outpoint, claimId, amount, height, validAtHeight);
//
//    CClaimTrieData data;
//    // base node has a claim, so it should not be pruned
//    data.insertClaim(test_claim);
//    cc.insert("", std::move(data));
//
//    // node 1 has a claim so it should not be pruned
//    data.insertClaim(test_claim);
//    // set this just to make sure we get the right CClaimTrieNode back
//    data.nHeightOfLastTakeover = 10;
//    cc.insert("t", std::move(data));
//
//    //node 2 does not have a claim so it should be pruned
//    // thus we should find pruned node 1 in cache
//    cc.insert("te", CClaimTrieData{});
//
//    BOOST_CHECK(cc.erase("te"));
//    BOOST_CHECK_EQUAL(2, cc.cacheSize());
//    auto it = cc.getCache("t");
//    BOOST_CHECK_EQUAL(10, it->nHeightOfLastTakeover);
//    BOOST_CHECK_EQUAL(1, it->claims.size());
//    BOOST_CHECK_EQUAL(2, cc.cacheSize());
//
//    cc.insert("te", CClaimTrieData{});
//    // erasing "t" will make it weak
//    BOOST_CHECK(cc.erase("t"));
//    // so now we erase "e" as well as "t"
//    BOOST_CHECK(cc.erase("te"));
//    // we have claim in root
//    BOOST_CHECK_EQUAL(1, cc.cacheSize());
//    BOOST_CHECK(cc.erase(""));
//    BOOST_CHECK_EQUAL(0, cc.cacheSize());
//}

BOOST_AUTO_TEST_CASE(trie_stays_consistent_test)
{
    std::vector<std::string> names {
        "goodness", "goodnight", "goodnatured", "goods", "go", "goody", "goo"
    };

    CClaimTrieCacheTest cache(&::Claimtrie());
    CClaimValue value;

    for (auto& name: names) {
        value.outPoint.hash = Hash(name.begin(), name.end());
        value.outPoint.n = 0;
        value.claimId = ClaimIdHash(uint256(value.outPoint.hash), 0);
        BOOST_CHECK(cache.insertClaimIntoTrie(name, value));
    }

    cache.flush();
    BOOST_CHECK(cache.checkConsistency());

    for (auto& name: names) {
        auto hash = Hash(name.begin(), name.end());
        BOOST_CHECK(cache.removeClaimFromTrie(name, COutPoint(hash, 0)));
        cache.flush();
        BOOST_CHECK(cache.checkConsistency());
    }
    BOOST_CHECK(::Claimtrie().empty());
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
}

BOOST_AUTO_TEST_SUITE_END()
