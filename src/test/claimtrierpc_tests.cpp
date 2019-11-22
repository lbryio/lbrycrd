// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <test/claimtriefixture.h>

using namespace std;

extern void ValidatePairs(CClaimTrieCache& cache, const std::vector<std::pair<bool, uint256>>& pairs, uint256 claimHash);

BOOST_FIXTURE_TEST_SUITE(claimtrierpc_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(getnamesintrie_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("test");
    std::string sValue1("test");

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 42);
    fixture.IncrementBlocks(1);

    rpcfn_type getnamesintrie = tableRPC["getnamesintrie"]->actor;
    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);

    UniValue results = getnamesintrie(req);
    BOOST_CHECK_EQUAL(results.size(), 1U);

    req.params.push_back(blockHash.GetHex());

    results = getnamesintrie(req);
    BOOST_CHECK_EQUAL(results.size(), 0U);
}

BOOST_AUTO_TEST_CASE(getvalueforname_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("testV");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 2);
    fixture.IncrementBlocks(1);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    fixture.MakeSupport(fixture.GetCoinbase(), tx1, sName1, 3);
    fixture.IncrementBlocks(10);

    rpcfn_type getvalueforname = tableRPC["getvalueforname"]->actor;
    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(sName1));

    UniValue results = getvalueforname(req);
    BOOST_CHECK_EQUAL(results[T_VALUE].get_str(), HexStr(sValue1));
    BOOST_CHECK_EQUAL(results[T_AMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(results[T_EFFECTIVEAMOUNT].get_int(), 5);

    req.params.push_back(blockHash.GetHex());

    results = getvalueforname(req);
    BOOST_CHECK_EQUAL(results[T_AMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(results[T_EFFECTIVEAMOUNT].get_int(), 2);
}

BOOST_AUTO_TEST_CASE(getclaimsforname_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("test1");
    std::string sValue2("test2");

    int height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 2);
    fixture.IncrementBlocks(1);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 3);
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;
    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(sName1));

    UniValue results = getclaimsforname(req);
    UniValue claims = results[T_CLAIMS];
    BOOST_CHECK_EQUAL(claims.size(), 2U);
    BOOST_CHECK_EQUAL(results[T_LASTTAKEOVERHEIGHT].get_int(), height + 1);
    BOOST_CHECK_EQUAL(claims[0][T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(claims[0][T_SUPPORTS].size(), 0U);

    fixture.IncrementBlocks(1);

    results = getclaimsforname(req);
    claims = results[T_CLAIMS];
    BOOST_CHECK_EQUAL(claims.size(), 2U);
    BOOST_CHECK_EQUAL(results[T_LASTTAKEOVERHEIGHT].get_int(), height + 3);
    BOOST_CHECK_EQUAL(claims[0][T_EFFECTIVEAMOUNT].get_int(), 3);
    BOOST_CHECK_EQUAL(claims[1][T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(claims[0][T_SUPPORTS].size(), 0U);
    BOOST_CHECK_EQUAL(claims[1][T_SUPPORTS].size(), 0U);

    req.params.push_back(blockHash.GetHex());

    results = getclaimsforname(req);
    claims = results[T_CLAIMS];
    BOOST_CHECK_EQUAL(claims.size(), 1U);
    BOOST_CHECK_EQUAL(results[T_LASTTAKEOVERHEIGHT].get_int(), height + 1);
    BOOST_CHECK_EQUAL(claims[0][T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(claims[0][T_SUPPORTS].size(), 0U);
}

BOOST_AUTO_TEST_CASE(claim_rpcs_rollback2_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("test1");
    std::string sValue2("test2");

    int height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 1);
    fixture.IncrementBlocks(2);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 2);
    fixture.IncrementBlocks(3);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    CMutableTransaction tx3 = fixture.MakeSupport(fixture.GetCoinbase(), tx1, sValue1, 3);
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;
    rpcfn_type getvalueforname = tableRPC["getvalueforname"]->actor;
    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(sName1));
    req.params.push_back(blockHash.GetHex());

    UniValue claimsResults = getclaimsforname(req);
    BOOST_CHECK_EQUAL(claimsResults[T_LASTTAKEOVERHEIGHT].get_int(), height + 5);
    BOOST_CHECK_EQUAL(claimsResults[T_CLAIMS][0][T_SUPPORTS].size(), 0U);
    BOOST_CHECK_EQUAL(claimsResults[T_CLAIMS][1][T_SUPPORTS].size(), 0U);

    UniValue valueResults = getvalueforname(req);
    BOOST_CHECK_EQUAL(valueResults[T_VALUE].get_str(), HexStr(sValue2));
    BOOST_CHECK_EQUAL(valueResults[T_AMOUNT].get_int(), 2);
}

BOOST_AUTO_TEST_CASE(claim_rpcs_rollback3_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("testN");
    std::string sValue1("test1");
    std::string sValue2("test2");

    int height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 3);
    fixture.IncrementBlocks(1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 2);
    fixture.IncrementBlocks(2);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();

    CMutableTransaction tx3 = fixture.Spend(tx1); // abandon the claim
    fixture.IncrementBlocks(1);

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;
    rpcfn_type getvalueforname = tableRPC["getvalueforname"]->actor;
    rpcfn_type getblocktemplate = tableRPC["getblocktemplate"]->actor;

    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);
    UniValue templateResults = getblocktemplate(req);
    BOOST_CHECK_EQUAL(templateResults["claimtrie"].get_str(), chainActive.Tip()->hashClaimTrie.GetHex());

    req.params.push_back(UniValue(sName1));
    req.params.push_back(blockHash.GetHex());

    UniValue claimsResults = getclaimsforname(req);
    BOOST_CHECK_EQUAL(claimsResults[T_LASTTAKEOVERHEIGHT].get_int(), height + 1);

    UniValue valueResults = getvalueforname(req);
    BOOST_CHECK_EQUAL(valueResults[T_VALUE].get_str(), HexStr(sValue1));
    BOOST_CHECK_EQUAL(valueResults[T_AMOUNT].get_int(), 3);
}

std::vector<std::pair<bool, uint256>> jsonToPairs(const UniValue& jsonPair)
{
    std::vector<std::pair<bool, uint256>> pairs;
    for (std::size_t i = 0; i < jsonPair.size(); ++i) {
        auto& jpair = jsonPair[i];
        pairs.emplace_back(jpair[T_ODD].get_bool(), uint256S(jpair[T_HASH].get_str()));
    }
    return pairs;
}

BOOST_AUTO_TEST_CASE(hash_bid_seq_claim_changes_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setHashForkHeight(2);
    fixture.IncrementBlocks(4);

    std::string name = "toa";
    std::string value1 = "one", value2 = "two", value3 = "tre", value4 = "for";

    auto tx1 = fixture.MakeClaim(fixture.GetCoinbase(), name, value1, 1);
    fixture.IncrementBlocks(1);

    CMutableTransaction tx2 = BuildTransaction(fixture.GetCoinbase(), 0, 2);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME
                                         << std::vector<unsigned char>(name.begin(), name.end())
                                         << std::vector<unsigned char>(value2.begin(), value2.end()) << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 3;
    tx2.vout[1].scriptPubKey = CScript() << OP_CLAIM_NAME
                                         << std::vector<unsigned char>(name.begin(), name.end())
                                         << std::vector<unsigned char>(value3.begin(), value3.end()) << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[1].nValue = 2;

    fixture.CommitTx(tx2);
    fixture.IncrementBlocks(1);

    auto tx3 = fixture.MakeClaim(fixture.GetCoinbase(), name, value4, 4);
    fixture.IncrementBlocks(1);

    int height = chainActive.Height();

    auto claimId1 = ClaimIdHash(tx1.GetHash(), 0);
    auto claimId2 = ClaimIdHash(tx2.GetHash(), 0);
    auto claimId3 = ClaimIdHash(tx2.GetHash(), 1);
    auto claimId4 = ClaimIdHash(tx3.GetHash(), 0);

    int claim1bid = 3, claim1seq = 0;
    int claim2bid = 1, claim2seq = 1;
    int claim3bid = 2, claim3seq = 2;
    int claim4bid = 0, claim4seq = 3;

    auto getclaimsforname = tableRPC["getclaimsforname"]->actor;

    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));

    auto result = getclaimsforname(req);
    auto claims = result[T_CLAIMS];
    BOOST_CHECK_EQUAL(claims.size(), 4U);
    BOOST_CHECK_EQUAL(result[T_LASTTAKEOVERHEIGHT].get_int(), height);
    BOOST_CHECK_EQUAL(claims[0][T_EFFECTIVEAMOUNT].get_int(), 4);
    BOOST_CHECK_EQUAL(claims[0][T_BID].get_int(), claim4bid);
    BOOST_CHECK_EQUAL(claims[0][T_SEQUENCE].get_int(), claim4seq);
    BOOST_CHECK_EQUAL(claims[0][T_CLAIMID].get_str(), claimId4.GetHex());
    BOOST_CHECK_EQUAL(claims[1][T_EFFECTIVEAMOUNT].get_int(), 3);
    BOOST_CHECK_EQUAL(claims[1][T_BID].get_int(), claim2bid);
    BOOST_CHECK_EQUAL(claims[1][T_SEQUENCE].get_int(), claim2seq);
    BOOST_CHECK_EQUAL(claims[1][T_CLAIMID].get_str(), claimId2.GetHex());
    BOOST_CHECK_EQUAL(claims[2][T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(claims[2][T_BID].get_int(), claim3bid);
    BOOST_CHECK_EQUAL(claims[2][T_SEQUENCE].get_int(), claim3seq);
    BOOST_CHECK_EQUAL(claims[2][T_CLAIMID].get_str(), claimId3.GetHex());
    BOOST_CHECK_EQUAL(claims[3][T_EFFECTIVEAMOUNT].get_int(), 1);
    BOOST_CHECK_EQUAL(claims[3][T_BID].get_int(), claim1bid);
    BOOST_CHECK_EQUAL(claims[3][T_SEQUENCE].get_int(), claim1seq);
    BOOST_CHECK_EQUAL(claims[3][T_CLAIMID].get_str(), claimId1.GetHex());

    auto getclaimbybid = tableRPC["getclaimbybid"]->actor;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));
    req.params.push_back(UniValue(claim3bid));

    result = getclaimbybid(req);
    BOOST_CHECK_EQUAL(result[T_LASTTAKEOVERHEIGHT].get_int(), height);
    BOOST_CHECK_EQUAL(result[T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(result[T_BID].get_int(), claim3bid);
    BOOST_CHECK_EQUAL(result[T_SEQUENCE].get_int(), claim3seq);
    BOOST_CHECK_EQUAL(result[T_CLAIMID].get_str(), claimId3.GetHex());

    auto getclaimbyseq = tableRPC["getclaimbyseq"]->actor;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));
    req.params.push_back(UniValue(claim2seq));

    result = getclaimbyseq(req);
    BOOST_CHECK_EQUAL(result[T_LASTTAKEOVERHEIGHT].get_int(), height);
    BOOST_CHECK_EQUAL(result[T_EFFECTIVEAMOUNT].get_int(), 3);
    BOOST_CHECK_EQUAL(result[T_BID].get_int(), claim2bid);
    BOOST_CHECK_EQUAL(result[T_SEQUENCE].get_int(), claim2seq);
    BOOST_CHECK_EQUAL(result[T_CLAIMID].get_str(), claimId2.GetHex());

    auto getclaimbyid = tableRPC["getclaimbyid"]->actor;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(claimId1.GetHex()));

    result = getclaimbyid(req);
    BOOST_CHECK_EQUAL(result[T_LASTTAKEOVERHEIGHT].get_int(), height);
    BOOST_CHECK_EQUAL(result[T_EFFECTIVEAMOUNT].get_int(), 1);
    BOOST_CHECK_EQUAL(result[T_BID].get_int(), claim1bid);
    BOOST_CHECK_EQUAL(result[T_SEQUENCE].get_int(), claim1seq);
    BOOST_CHECK_EQUAL(result[T_CLAIMID].get_str(), claimId1.GetHex());

    // check by partial id (at least 3 chars)
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(claimId3.GetHex().substr(0, 3)));

    result = getclaimbyid(req);
    BOOST_CHECK_EQUAL(result[T_LASTTAKEOVERHEIGHT].get_int(), height);
    BOOST_CHECK_EQUAL(result[T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK_EQUAL(result[T_BID].get_int(), claim3bid);
    BOOST_CHECK_EQUAL(result[T_SEQUENCE].get_int(), claim3seq);
    BOOST_CHECK_EQUAL(result[T_CLAIMID].get_str(), claimId3.GetHex());

    auto blockhash = chainActive.Tip()->GetBlockHash();

    auto getnameproof = tableRPC["getnameproof"]->actor;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));
    req.params.push_back(UniValue(blockhash.GetHex()));
    req.params.push_back(UniValue(claimId3.GetHex()));

    result = getnameproof(req);
    auto claimHash = getValueHash(COutPoint(tx2.GetHash(), 1), result[T_LASTTAKEOVERHEIGHT].get_int());
    ValidatePairs(fixture, jsonToPairs(result[T_PAIRS]), claimHash);

    // check by partial id (can be even 1 char)
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));
    req.params.push_back(UniValue(blockhash.GetHex()));
    req.params.push_back(UniValue(claimId2.GetHex().substr(0, 2)));

    result = getnameproof(req);
    claimHash = getValueHash(COutPoint(tx2.GetHash(), 0), result[T_LASTTAKEOVERHEIGHT].get_int());
    ValidatePairs(fixture, jsonToPairs(result[T_PAIRS]), claimHash);

    auto getclaimproofbybid = tableRPC["getclaimproofbybid"]->actor;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));
    req.params.push_back(UniValue(claim1bid));

    result = getclaimproofbybid(req);
    claimHash = getValueHash(COutPoint(tx1.GetHash(), 0), result[T_LASTTAKEOVERHEIGHT].get_int());
    ValidatePairs(fixture, jsonToPairs(result[T_PAIRS]), claimHash);

    auto getclaimproofbyseq = tableRPC["getclaimproofbyseq"]->actor;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(name));
    req.params.push_back(UniValue(claim4seq));

    result = getclaimproofbyseq(req);
    claimHash = getValueHash(COutPoint(tx3.GetHash(), 0), result[T_LASTTAKEOVERHEIGHT].get_int());
    ValidatePairs(fixture, jsonToPairs(result[T_PAIRS]), claimHash);

    // TODO: repair this or ditch it
//    auto getchangesinblock = tableRPC["getchangesinblock"]->actor;
//    req.params = UniValue(UniValue::VARR);
//    req.params.push_back(UniValue(blockhash.GetHex()));
//
//    result = getchangesinblock(req);
//    BOOST_REQUIRE_EQUAL(result[T_CLAIMSADDEDORUPDATED].size(), 3);
//    BOOST_CHECK_EQUAL(result[T_CLAIMSADDEDORUPDATED][0].get_str(), claimId2.GetHex());
//    BOOST_CHECK_EQUAL(result[T_CLAIMSADDEDORUPDATED][1].get_str(), claimId3.GetHex());
//    BOOST_CHECK_EQUAL(result[T_CLAIMSADDEDORUPDATED][2].get_str(), claimId4.GetHex());
//    BOOST_CHECK_EQUAL(result[T_CLAIMSREMOVED].size(), 0);
//    BOOST_CHECK_EQUAL(result[T_SUPPORTSADDEDORUPDATED].size(), 0);
//    BOOST_CHECK_EQUAL(result[T_SUPPORTSREMOVED].size(), 0);
}

BOOST_AUTO_TEST_CASE(claim_rpc_pending_amount_test)
{
    ClaimTrieChainFixture fixture;
    std::string sName1("test");
    std::string sValue1("test1");
    std::string sValue2("test2");

    rpcfn_type getclaimsforname = tableRPC["getclaimsforname"]->actor;

    JSONRPCRequest req;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(UniValue(sName1));

    fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1, 1);
    fixture.IncrementBlocks(1);

    fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue2, 2);
    fixture.IncrementBlocks(1);

    auto claims = getclaimsforname(req)[T_CLAIMS];
    BOOST_CHECK_EQUAL(claims.size(), 2U);
    BOOST_CHECK_EQUAL(claims[0][T_EFFECTIVEAMOUNT].get_int(), 1);
    BOOST_CHECK(!claims[0].exists(T_PENDINGAMOUNT));
    BOOST_CHECK_EQUAL(claims[1][T_EFFECTIVEAMOUNT].get_int(), 0);
    BOOST_CHECK(claims[1].exists(T_PENDINGAMOUNT));
    BOOST_CHECK_EQUAL(claims[1][T_PENDINGAMOUNT].get_int(), 2);

    fixture.IncrementBlocks(1);

    claims = getclaimsforname(req)[T_CLAIMS];
    BOOST_CHECK_EQUAL(claims.size(), 2U);
    BOOST_CHECK_EQUAL(claims[0][T_EFFECTIVEAMOUNT].get_int(), 2);
    BOOST_CHECK(!claims[0].exists(T_PENDINGAMOUNT));
    BOOST_CHECK_EQUAL(claims[1][T_EFFECTIVEAMOUNT].get_int(), 1);
    BOOST_CHECK(!claims[1].exists(T_PENDINGAMOUNT));
}

BOOST_AUTO_TEST_SUITE_END()
