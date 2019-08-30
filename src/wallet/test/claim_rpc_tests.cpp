#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

#include <consensus/validation.h>
#include <core_io.h>
#include <key_io.h>
#include <rpc/server.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/test/wallet_test_fixture.h>
#include <policy/policy.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

struct CatchWalletTestSetup: public TestingSetup {
    CatchWalletTestSetup() : TestingSetup(CBaseChainParams::REGTEST) {
        RegisterWalletRPCCommands(tableRPC);

        rpcfn_type rpc_method = tableRPC["createwallet"]->actor;
        JSONRPCRequest req;
        req.params = UniValue(UniValue::VARR);
        req.params.push_back("tester_wallet");
        UniValue results = rpc_method(req);
        BOOST_CHECK_EQUAL(results["name"].get_str(), "tester_wallet");
    }
    ~CatchWalletTestSetup() override {
        rpcfn_type rpc_method = tableRPC["unloadwallet"]->actor;
        JSONRPCRequest req;
        req.URI = "/wallet/tester_wallet";
        req.params = UniValue(UniValue::VARR);
        rpc_method(req);
    }
};

BOOST_FIXTURE_TEST_SUITE(claim_rpc_tests, CatchWalletTestSetup)

double AvailableBalance() {
    rpcfn_type rpc_method = tableRPC["getbalance"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    UniValue results = rpc_method(req);
    return results.get_real();
}

uint256 ClaimAName(const std::string& name, const std::string& data, const std::string& price, bool isUpdate = false) {
    // pass a txid as name for update
    rpcfn_type rpc_method = tableRPC[isUpdate ? "updateclaim" : "claimname"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(name);
    req.params.push_back(data);
    req.params.push_back(price);

    UniValue results = rpc_method(req);
    auto txid = results.get_str();
    uint256 ret;
    ret.SetHex(txid);
    return ret;
}

uint256 SupportAName(const std::string& name, const std::string& claimId, const std::string& price) {
    // pass a txid as name for update
    rpcfn_type rpc_method = tableRPC["supportclaim"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(name);
    req.params.push_back(claimId);
    req.params.push_back(price);

    UniValue results = rpc_method(req);
    auto txid = results.get_str();
    uint256 ret;
    ret.SetHex(txid);
    return ret;
}

UniValue LookupAllNames() {
    rpcfn_type rpc_method = tableRPC["listnameclaims"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    return rpc_method(req);
}

std::vector<uint256> generateBlock(int blocks = 1) {
    rpcfn_type rpc_method = tableRPC["generate"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(blocks);
    UniValue results = rpc_method(req);

    std::vector<uint256> ret;
    for (auto i = 0U; i < results.size(); ++i) {
        auto txid = results[i].get_str();
        uint256 id;
        id.SetHex(txid);
        ret.push_back(id);
    }
    return ret;
}

void rollbackBlock(const std::vector<uint256>& ids) {
    rpcfn_type rpc_method = tableRPC["invalidateblock"]->actor;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        JSONRPCRequest req;
        req.URI = "/wallet/tester_wallet";
        req.params = UniValue(UniValue::VARR);
        req.params.push_back(it->GetHex());
        rpc_method(req);
    }
    // totally weird that invalidateblock is async
    while (GetMainSignals().CallbacksPending())
        usleep(5000);
}

uint256 AbandonAClaim(const uint256& txid, bool isSupport = false) {
    rpcfn_type pre_rpc_method = tableRPC["getrawchangeaddress"]->actor;
    JSONRPCRequest pre_req;
    pre_req.URI = "/wallet/tester_wallet";
    pre_req.params = UniValue(UniValue::VARR);
    pre_req.params.push_back("legacy");
    UniValue adr_hash = pre_rpc_method(pre_req);

    // pass a txid as name for update
    rpcfn_type rpc_method = tableRPC[isSupport ? "abandonsupport" : "abandonclaim"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(txid.GetHex());
    req.params.push_back(adr_hash.get_str());

    try {
        UniValue results = rpc_method(req);
        uint256 ret;
        ret.SetHex(results.get_str());
        return ret;
    }
    catch(...) {
        return {};
    }
}

void AddClaimSupportThenRemove() {
    generateBlock(155);
    BOOST_CHECK_EQUAL(AvailableBalance(), 55.0);

    // ops for test: claimname, updateclaim, abandonclaim, listnameclaims, supportclaim, abandonsupport
    // order of ops:
    // claim a name
    // udpate it
    // support it
    // abandon support
    // abandon claim
    // all above in reverse

    // claim a name
    auto txid = ClaimAName("tester", "deadbeef", "1.0");
    auto g1 = generateBlock();
    auto looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef");
    BOOST_CHECK_EQUAL(looked[0]["txid"].get_str(), txid.GetHex());

    // update it
    auto txid2 = ClaimAName(txid.GetHex(), "deadbeef02", "1.0", true);
    auto g2 = generateBlock();
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");

    // support it
    auto clid = looked[0]["claimId"].get_str();
    auto spid = SupportAName("tester", clid, "0.5");
    auto g3 = generateBlock();
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");
    BOOST_CHECK_EQUAL(looked[0]["amount"].get_real(), 1.0);
    BOOST_CHECK_EQUAL(looked[1]["claimtype"].get_str(), "SUPPORT");
    BOOST_CHECK_EQUAL(looked[1]["name"].get_str(), "tester");
    BOOST_CHECK_EQUAL(looked[1]["amount"].get_real(), 0.5);
    BOOST_CHECK_EQUAL(looked[1]["txid"].get_str(), spid.GetHex());
    BOOST_CHECK_EQUAL(looked[1]["supported_claimid"].get_str(), clid);

    // abandon support
    auto aid1 = AbandonAClaim(spid, true);
    BOOST_CHECK(!aid1.IsNull());
    auto g4 = generateBlock();
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 1);
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");
    BOOST_CHECK_EQUAL(looked[0]["claimtype"].get_str(), "CLAIM");

    // abandon claim
    auto aid2 = AbandonAClaim(txid);
    BOOST_CHECK(aid2.IsNull());
    aid2 = AbandonAClaim(txid2);
    BOOST_CHECK(!aid2.IsNull());
    auto g5 = generateBlock();
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 0);

    // all above in reverse
    // except that it doesn't work
    // TODO: understand why it doesn't work

    /*
    // re-add claim
    rollbackBlock(g5);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 1);
    BOOST_CHECK_EQUAL(looked[0]["name"].get_str(), "tester");
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");

    // re-add support
    rollbackBlock(g4);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");
    BOOST_CHECK_EQUAL(looked[0]["amount"].get_real(), 1.0);
    BOOST_CHECK_EQUAL(looked[1]["claimtype"].get_str(), "SUPPORT");
    BOOST_CHECK_EQUAL(looked[1]["name"].get_str(), "tester");
    BOOST_CHECK_EQUAL(looked[1]["amount"].get_real(), 0.5);
    BOOST_CHECK_EQUAL(looked[1]["txid"].get_str(), spid.GetHex());
    BOOST_CHECK_EQUAL(looked[1]["supported_claimid"].get_str(), clid);

    // remove support
    rollbackBlock(g3);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");

    // remove update
    rollbackBlock(g2);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef");

    // remove claim
    rollbackBlock(g1);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 0);

    generateBlock();
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 0);
    */
}

BOOST_AUTO_TEST_CASE(claim_op_runthrough_legacy)
{
    for (auto& wallet: GetWallets())
        wallet->m_default_address_type = OutputType::LEGACY;
    AddClaimSupportThenRemove();
}

BOOST_AUTO_TEST_CASE(claim_op_runthrough_p2sh)
{
    for (auto& wallet: GetWallets()) {
        BOOST_CHECK(DEFAULT_ADDRESS_TYPE == wallet->m_default_address_type); // BOOST_CHECK_EQUAL no compile here
        wallet->m_default_address_type = OutputType::P2SH_SEGWIT;
    }
    AddClaimSupportThenRemove();
}

BOOST_AUTO_TEST_CASE(claim_op_runthrough_bech32)
{
    for (auto& wallet: GetWallets()) {
        BOOST_CHECK(DEFAULT_ADDRESS_TYPE == wallet->m_default_address_type); // BOOST_CHECK_EQUAL no compile here
        wallet->m_default_address_type = OutputType::BECH32;
    }
    AddClaimSupportThenRemove();
}

std::vector<COutPoint> SpendableCoins() {
    rpcfn_type rpc_method = tableRPC["listunspent"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    UniValue unspents = rpc_method(req);
    std::vector<COutPoint> ret;
    for (uint32_t i = 0; i < unspents.size(); ++i)
        ret.emplace_back(uint256S(unspents[i]["txid"].get_str()), unspents[i]["vout"].get_int());
    return ret;
}

CTxDestination NewAddress(OutputType t) {
    rpcfn_type rpc_method = tableRPC["getnewaddress"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back("");
    req.params.push_back(FormatOutputType(t));
    UniValue address = rpc_method(req);
    return DecodeDestination(address.get_str());
}

std::string SignRawTransaction(const std::string& tx) {
    rpcfn_type rpc_method = tableRPC["signrawtransactionwithwallet"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(tx);
    UniValue results = rpc_method(req);
    BOOST_CHECK(results["complete"].get_bool());
    BOOST_CHECK(!results.exists("errors"));
    return results["hex"].get_str();
}

std::string SendRawTransaction(const std::string& tx) {
    rpcfn_type rpc_method = tableRPC["sendrawtransaction"]->actor;
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(tx);
    req.params.push_back(UniValue(true)); // allow high fees
    UniValue results = rpc_method(req);
    return results.get_str();
}


BOOST_AUTO_TEST_CASE(can_sign_all_addr)
{
    generateBlock(110);
    BOOST_CHECK_EQUAL(AvailableBalance(), 10.0);
    auto coins = SpendableCoins();

    std::vector<std::string> txids;
    for (std::size_t i = 0; i < 3; ++i) {
        CMutableTransaction rawTx;
        CTxIn in(coins[i], CScript(), 0);
        rawTx.vin.push_back(in);
        auto destination = NewAddress(OutputType(i));
        CScript scriptPubKey = ClaimNameScript("name" + std::to_string(i), "value", false)
                + GetScriptForDestination(destination);
        CTxOut out(100000, scriptPubKey);
        rawTx.vout.push_back(out);
        auto hex = EncodeHexTx(rawTx);
        hex = SignRawTransaction(hex);
        txids.push_back(SendRawTransaction(hex));
    }
    generateBlock(1);
    auto looked = LookupAllNames().get_array();

    for (std::size_t i = 0; i < 3; ++i) {
        BOOST_CHECK_EQUAL(looked[i]["name"].get_str(), "name" + std::to_string(i));
        BOOST_CHECK_EQUAL(looked[i]["value"].get_str(), HexStr(std::string("value")));
        BOOST_CHECK_EQUAL(looked[i]["txid"].get_str(), txids[i]);
    }
}

BOOST_AUTO_TEST_SUITE_END()
