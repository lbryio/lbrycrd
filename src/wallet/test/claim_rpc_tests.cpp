#include <utility>
#include <vector>

#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <interfaces/chain.h>
#include <key_io.h>
#include <psbt.h>
#include <rpc/util.h>
#include <rpc/server.h>
#include <test/setup_common.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/test/wallet_test_fixture.h>
#include <policy/policy.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

static std::string address;
static const std::string wallet_name = "tester_wallet";
static const std::string wallet_uri = "/wallet/" + wallet_name;

struct CatchWalletTestSetup: public WalletTestingSetup {
    CatchWalletTestSetup() : WalletTestingSetup(CBaseChainParams::REGTEST) {
        interfaces.chain = interfaces::MakeChain();
        g_rpc_interfaces = &interfaces;
        auto rpc_method = tableRPC["createwallet"];
        JSONRPCRequest req;
        req.params = UniValue(UniValue::VARR);
        req.params.push_back(wallet_name);
        UniValue results = rpc_method(req);
        BOOST_CHECK_EQUAL(results["name"].get_str(), wallet_name);
        auto rpc_address = tableRPC["getnewaddress"];
        req.params = UniValue(UniValue::VARR);
        address = rpc_address(req).get_str();
    }
    ~CatchWalletTestSetup() override {
        auto rpc_method = tableRPC["unloadwallet"];
        JSONRPCRequest req;
        req.URI = wallet_uri;
        req.params = UniValue(UniValue::VARR);
        rpc_method(req);
        g_rpc_interfaces = nullptr;
    }
    InitInterfaces interfaces;
};

BOOST_FIXTURE_TEST_SUITE(claim_rpc_tests, CatchWalletTestSetup)

double AvailableBalance() {
    auto rpc_method = tableRPC["getbalance"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    UniValue results = rpc_method(req);
    return results.get_real();
}

uint256 ClaimAName(const std::string& name, const std::string& data, const std::string& price, bool isUpdate = false) {
    // pass a txid as name for update
    auto rpc_method = tableRPC[isUpdate ? "updateclaim" : "claimname"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(name);
    req.params.push_back(data);
    req.params.push_back(price);

    UniValue results = rpc_method(req);
    auto txid = results.get_str();
    uint256 ret;
    if (txid.find(' ') != std::string::npos) {
        fprintf(stderr, "Error creating claim: %s\n", txid.c_str());
        return ret;
    }
    ret.SetHex(txid);
    return ret;
}

uint256 SupportAName(const std::string& name, const std::string& claimId, const std::string& price) {
    // pass a txid as name for update
    auto rpc_method = tableRPC["supportclaim"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(name);
    req.params.push_back(claimId);
    req.params.push_back(price);

    UniValue results = rpc_method(req);
    auto txid = results["txId"].get_str();
    uint256 ret;
    ret.SetHex(txid);
    return ret;
}

UniValue LookupAllNames() {
    auto rpc_method = tableRPC["listnameclaims"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    return rpc_method(req);
}

UniValue PruneAbandonFunds(const uint256& txid) {
    auto rpc_method = tableRPC["removeprunedfunds"];
    JSONRPCRequest req;
    req.URI = "/wallet/tester_wallet";
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(txid.GetHex());
    return rpc_method(req);
}

std::vector<uint256> generateBlock(int blocks = 1) {
    auto rpc_method = tableRPC["generatetoaddress"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(blocks);
    req.params.push_back(address);
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
    auto rpc_method = tableRPC["invalidateblock"];
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        JSONRPCRequest req;
        req.URI = wallet_uri;
        req.params = UniValue(UniValue::VARR);
        req.params.push_back(it->GetHex());
        rpc_method(req);
    }
    // totally weird that invalidateblock is async
    while (GetMainSignals().CallbacksPending())
        MilliSleep(5);
}

uint256 AbandonAClaim(const uint256& txid, bool isSupport = false) {
    auto pre_rpc_method = tableRPC["getrawchangeaddress"];
    JSONRPCRequest pre_req;
    pre_req.URI = wallet_uri;
    pre_req.params = UniValue(UniValue::VARR);
    pre_req.params.push_back("legacy");
    UniValue adr_hash = pre_rpc_method(pre_req);

    // pass a txid as name for update
    auto rpc_method = tableRPC[isSupport ? "abandonsupport" : "abandonclaim"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
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

void ValidateBalance(double claims, double supports) {
    auto rpc_method = tableRPC["getwalletinfo"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    UniValue results = rpc_method(req);
    BOOST_CHECK_EQUAL(claims, results["staked_claim_balance"].get_real());
    BOOST_CHECK_EQUAL(supports, results["staked_support_balance"].get_real());
    BOOST_CHECK_EQUAL(claims + supports, results["balance"].get_real() - results["available_balance"].get_real());
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
    BOOST_CHECK_EQUAL(looked.size(), 1);
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
    BOOST_CHECK_EQUAL(looked.size(), 2);
    BOOST_CHECK_EQUAL(looked[0]["value"].get_str(), "deadbeef02");
    BOOST_CHECK_EQUAL(looked[0]["amount"].get_real(), 1.0);
    BOOST_CHECK_EQUAL(looked[1]["claimtype"].get_str(), "SUPPORT");
    BOOST_CHECK_EQUAL(looked[1]["name"].get_str(), "tester");
    BOOST_CHECK_EQUAL(looked[1]["amount"].get_real(), 0.5);
    BOOST_CHECK_EQUAL(looked[1]["txid"].get_str(), spid.GetHex());
    BOOST_CHECK_EQUAL(looked[1]["supported_claimid"].get_str(), clid);

    ValidateBalance(1.0, 0.5);

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
    auto rpc_method = tableRPC["listunspent"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    UniValue unspents = rpc_method(req);
    std::vector<COutPoint> ret;
    for (uint32_t i = 0; i < unspents.size(); ++i)
        ret.emplace_back(uint256S(unspents[i]["txid"].get_str()), unspents[i]["vout"].get_int());
    return ret;
}

CTxDestination NewAddress(OutputType t) {
    auto rpc_method = tableRPC["getnewaddress"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back("");
    req.params.push_back(FormatOutputType(t));
    UniValue address = rpc_method(req);
    return DecodeDestination(address.get_str());
}

std::string SignRawTransaction(const std::string& tx) {
    auto rpc_method = tableRPC["signrawtransactionwithwallet"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(tx);
    UniValue results = rpc_method(req);
    BOOST_CHECK(results["complete"].get_bool());
    BOOST_CHECK(!results.exists("errors"));
    if (results.exists("errors"))
        fprintf(stderr, "Errors: %s\n", results.write(2).c_str());
    return results["hex"].get_str();
}

std::string SendRawTransaction(const std::string& tx) {
    auto rpc_method = tableRPC["sendrawtransaction"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(tx);
    req.params.push_back("10000000");
    UniValue results = rpc_method(req);
    return results.get_str();
}

std::string FundRawTransaction(const std::string& tx) {
    auto rpc_method = tableRPC["fundrawtransaction"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(tx);
    UniValue results = rpc_method(req);
    return results["hex"].get_str();
}

std::string ProcessPsbt(const PartiallySignedTransaction& pst) {
    BOOST_CHECK(pst.IsSane());
    auto rpc_method = tableRPC["walletprocesspsbt"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << pst;
    auto base64 = EncodeBase64((unsigned char*)ssTx.data(), ssTx.size());
    req.params.push_back(base64);
    UniValue results = rpc_method(req);
    BOOST_CHECK(results["complete"].get_bool());
    return results["psbt"].get_str();
}

std::string FinalizePsbt(const std::string& pst) {
    auto rpc_method = tableRPC["finalizepsbt"];
    JSONRPCRequest req;
    req.URI = wallet_uri;
    req.params = UniValue(UniValue::VARR);
    req.params.push_back(pst);
    req.params.push_back(UniValue(true));
    UniValue results = rpc_method(req);
    BOOST_CHECK(results["complete"].get_bool());
    return results["hex"].get_str();
}

BOOST_AUTO_TEST_CASE(can_sign_all_addr)
{
    generateBlock(110);
    BOOST_CHECK_EQUAL(AvailableBalance(), 10.0);
    auto coins = SpendableCoins();

    std::vector<std::string> txids;
    std::vector<std::string> spends;
    for (uint32_t i = 0; i < 3; ++i) {
        CMutableTransaction claimTx, spendTx;
        CTxIn in(coins[i]);
        claimTx.vin.push_back(in);
        auto destination = NewAddress(OutputType(i));
        CScript scriptPubKey = ClaimNameScript("name" + std::to_string(i), "value", false)
                + GetScriptForDestination(destination);
        CTxOut out(100000, scriptPubKey);
        claimTx.vout.push_back(out);
        auto hex = EncodeHexTx(CTransaction(claimTx));
        hex = SignRawTransaction(hex);
        txids.push_back(SendRawTransaction(hex));
        CTxIn spendIn(uint256S(txids.back()), 0);
        auto destination2 = NewAddress(OutputType(i));
        CTxOut spendOut(90000, GetScriptForDestination(destination2));
        spendTx.vin.push_back(spendIn);
        spendTx.vout.push_back(spendOut);
        hex = EncodeHexTx(CTransaction(spendTx));
        spends.push_back(hex);
    }
    generateBlock(1);
    auto looked = LookupAllNames().get_array();

    for (std::size_t i = 0; i < looked.size(); ++i) {
        BOOST_CHECK_EQUAL(looked[i]["name"].get_str(), "name" + std::to_string(i));
        BOOST_CHECK_EQUAL(looked[i]["value"].get_str(), HexStr(std::string("value")));
        BOOST_CHECK_EQUAL(looked[i]["txid"].get_str(), txids[i]);
    }
    for (auto& s: spends) {
        auto hex = FundRawTransaction(s); // redundant
        hex = SignRawTransaction(hex);
        txids.push_back(SendRawTransaction(hex));
    }
    generateBlock(1);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 0U);
}

BOOST_AUTO_TEST_CASE(can_sign_all_pbst)
{
    generateBlock(110);
    BOOST_CHECK_EQUAL(AvailableBalance(), 10.0);
    auto coins = SpendableCoins();

    std::vector<std::string> txids;
    std::vector<PartiallySignedTransaction> spends;
    for (uint32_t i = 0; i < 3; ++i) {
        CMutableTransaction claimTx, spendTx;
        CTxIn in(coins[i]);
        claimTx.vin.push_back(in);
        auto destination = NewAddress(OutputType(i));
        CScript scriptPubKey = ClaimNameScript("name" + std::to_string(i), "value", false)
                               + GetScriptForDestination(destination);
        CTxOut out(100000, scriptPubKey);
        claimTx.vout.push_back(out);
        PartiallySignedTransaction pst(claimTx);
        auto result = ProcessPsbt(pst);
        auto hex = FinalizePsbt(result);
        txids.push_back(SendRawTransaction(hex));
        CTxIn spendIn(uint256S(txids.back()), 0);
        auto destination2 = NewAddress(OutputType(i));
        CTxOut spendOut(90000, GetScriptForDestination(destination2));
        spendTx.vin.push_back(spendIn);
        spendTx.vout.push_back(spendOut);
        spends.push_back(PartiallySignedTransaction(spendTx));
    }
    generateBlock(1);
    auto looked = LookupAllNames().get_array();

    for (std::size_t i = 0; i < 3; ++i) {
        BOOST_CHECK_EQUAL(looked[i]["name"].get_str(), "name" + std::to_string(i));
        BOOST_CHECK_EQUAL(looked[i]["value"].get_str(), HexStr(std::string("value")));
        BOOST_CHECK_EQUAL(looked[i]["txid"].get_str(), txids[i]);
    }
    for (auto& s: spends) {
        auto result = ProcessPsbt(s);
        auto hex = FinalizePsbt(result);
        txids.push_back(SendRawTransaction(hex));
    }
    generateBlock(1);
    looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 0U);
}

BOOST_AUTO_TEST_CASE(can_claim_after_each_fork)
{
    generateBlock(140);
    auto txid = ClaimAName("tester", "deadbeef", "1.0");
    BOOST_CHECK(!txid.IsNull());
    generateBlock(100);
    txid = ClaimAName("tester2", "deadbeef", "1.0");
    BOOST_CHECK(!txid.IsNull());
    generateBlock(100);
    txid = ClaimAName("tester3", "deadbeef", "10.0");
    BOOST_CHECK(!txid.IsNull());
    generateBlock(15);
    txid = ClaimAName("tester4", "deadbeef", "10.0");
    BOOST_CHECK(!txid.IsNull());
    generateBlock(1);
    auto looked = LookupAllNames().get_array();
    BOOST_CHECK_EQUAL(looked.size(), 4U);
}

BOOST_AUTO_TEST_CASE(remove_pruned_funds)
{
    generateBlock(140);
    // claim a name
    auto txid = ClaimAName("tester", "deadbeef", "1.0");
    generateBlock();
    // abandon claim
    AbandonAClaim(txid);
    generateBlock();
    PruneAbandonFunds(txid);
}

class HasMessage {
public:
    explicit HasMessage(const std::string& reason) : m_reason(reason) {}
    bool operator() (const UniValue& e) const {
        return e["message"].get_str().find(m_reason) != std::string::npos;
    };
private:
    const std::string m_reason;
};

BOOST_AUTO_TEST_CASE(invalid_utf8_claim_name)
{
    // nClaimInfoInMerkleForkHeight = 1350
    generateBlock(1350);
    // disallow non UTF8 strings
    BOOST_CHECK_EXCEPTION(ClaimAName("\xFF\xFF", "deadbeef", "1.0"), UniValue, HasMessage("Claim name is not valid UTF8 string"));
    // disallow \0 in string
    BOOST_CHECK_EXCEPTION(ClaimAName(std::string("test\0ab", 7), "deadbeef", "1.0"), UniValue, HasMessage("Claim name contains invalid symbol"));
    // allow ^ in string
    ClaimAName("test^ab", "deadbeef", "1.0");
}

BOOST_AUTO_TEST_SUITE_END()

