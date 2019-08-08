#include <claimtrie.h>
#include <coins.h>
#include <core_io.h>
#include <logging.h>
#include <nameclaim.h>
#include <rpc/server.h>
#include <shutdown.h>
#include <txdb.h>
#include <txmempool.h>
#include <univalue.h>
#include <validation.h>

#include <boost/locale.hpp>
#include <boost/locale/conversion.hpp>
#include <boost/thread.hpp>
#include <cmath>

void ParseClaimtrieId(const UniValue& v, std::string& partialId, uint160& claimId, const std::string& strName)
{
    static constexpr size_t claimIdHexLength = 40;

    std::string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName + " must be a hexadecimal string");
    if (strHex.length() == claimIdHexLength)
        claimId.SetHex(strHex);
    else
        partialId = strHex;
}

static CBlockIndex* BlockHashIndex(const uint256& blockHash)
{
    AssertLockHeld(cs_main);

    if (mapBlockIndex.count(blockHash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pblockIndex = mapBlockIndex[blockHash];
    if (!chainActive.Contains(pblockIndex))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not in main chain");

    return pblockIndex;
}

#define MAX_RPC_BLOCK_DECREMENTS 500

extern CChainState g_chainstate;
void RollBackTo(const CBlockIndex* targetIndex, CCoinsViewCache& coinsCache, CClaimTrieCache& trieCache)
{
    AssertLockHeld(cs_main);

    const CBlockIndex* activeIndex = chainActive.Tip();

    if (activeIndex->nHeight > (targetIndex->nHeight + MAX_RPC_BLOCK_DECREMENTS))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block is too deep");

    const size_t currentMemoryUsage = pcoinsTip->DynamicMemoryUsage();

    for (; activeIndex && activeIndex != targetIndex; activeIndex = activeIndex->pprev) {
        boost::this_thread::interruption_point();

        CBlock block;
        if (!ReadBlockFromDisk(block, activeIndex, Params().GetConsensus()))
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Failed to read %s", activeIndex->ToString()));

        if (coinsCache.DynamicMemoryUsage() + currentMemoryUsage > nCoinCacheUsage)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Out of memory, you may want to increase dbcache size");

        if (ShutdownRequested())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Shutdown requested");

        if (g_chainstate.DisconnectBlock(block, activeIndex, coinsCache, trieCache) != DisconnectResult::DISCONNECT_OK)
            throw JSONRPCError(RPC_INTERNAL_ERROR, strprintf("Failed to disconnect %s", block.ToString()));
    }
    trieCache.getMerkleHash(); // update the hash tree
}

std::string escapeNonUtf8(const std::string& name)
{
    using namespace boost::locale::conv;
    try {
        return to_utf<char>(name, "UTF-8", stop);
    } catch (const conversion_error&) {
        std::string result;
        result.reserve(name.size() * 2);
        for (uint8_t ch : name) {
            if (ch < 0x08 || (ch >= 0x0e && ch <= 0x1f) || ch >= 0x7f)
                result += tfm::format("\\u%04x", ch);
            else if (ch == 0x08) result += "\\b";
            else if (ch == 0x09) result += "\\t";
            else if (ch == 0x0a) result += "\\n";
            else if (ch == 0x0c) result += "\\f";
            else if (ch == 0x0d) result += "\\r";
            else if (ch == 0x22) result += "\\\"";
            else if (ch == 0x5c) result += "\\\\";
            else result += ch;
        }
        return result;
    }
}

static bool getValueForOutPoint(const CCoinsViewCache& coinsCache, const COutPoint& out, std::string& sValue)
{
    const Coin& coin = coinsCache.AccessCoin(out);
    if (coin.IsSpent())
        return false;

    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    if (!DecodeClaimScript(coin.out.scriptPubKey, op, vvchParams))
        return false;

    if (op == OP_CLAIM_NAME)
        sValue = HexStr(vvchParams[1].begin(), vvchParams[1].end());
    else if (vvchParams.size() > 2) // both UPDATE and SUPPORT
        sValue = HexStr(vvchParams[2].begin(), vvchParams[2].end());
    else
        return false;
    return true;
}

bool getClaimById(const uint160& claimId, std::string& name, CClaimValue* claim = nullptr)
{
    if (claimId.IsNull())
        return false;

    CClaimIndexElement element;
    if (!pclaimTrie->db->Read(std::make_pair(CLAIM_BY_ID, claimId), element))
        return false;
    if (element.claim.claimId == claimId) {
        name = element.name;
        if (claim)
            *claim = element.claim;
        return true;
    }
    return false;
}

// name can be setted explicitly
bool getClaimById(const std::string& partialId, std::string& name, CClaimValue* claim = nullptr)
{
    if (partialId.empty())
        return false;

    std::unique_ptr<CDBIterator> pcursor(pclaimTrie->db->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<uint8_t, uint160> key;
        if (!pcursor->GetKey(key) || key.first != CLAIM_BY_ID)
            continue;

        if (key.second.GetHex().find(partialId) != 0)
            continue;

        CClaimIndexElement element;
        if (pcursor->GetValue(element)) {
            if (!name.empty() && name != element.name)
                continue;
            name = element.name;
            if (claim)
                *claim = element.claim;
            return true;
        }
    }
    return false;
}

UniValue claimToJSON(const CCoinsViewCache& coinsCache, const CClaimValue& claim)
{
    UniValue result(UniValue::VOBJ);

    std::string targetName;
    if (getClaimById(claim.claimId, targetName))
        result.pushKV("name", escapeNonUtf8(targetName));

    std::string sValue;
    if (getValueForOutPoint(coinsCache, claim.outPoint, sValue))
        result.pushKV("value", sValue);

    result.pushKV("claimId", claim.claimId.GetHex());
    result.pushKV("txId", claim.outPoint.hash.GetHex());
    result.pushKV("n", (int)claim.outPoint.n);
    result.pushKV("height", claim.nHeight);
    result.pushKV("validAtHeight", claim.nValidAtHeight);
    result.pushKV("amount", claim.nAmount);

    return result;
}

UniValue supportToJSON(const CCoinsViewCache& coinsCache, const CSupportValue& support)
{
    UniValue ret(UniValue::VOBJ);

    std::string value;
    if (getValueForOutPoint(coinsCache, support.outPoint, value))
        ret.pushKV("value", value);

    ret.pushKV("txId", support.outPoint.hash.GetHex());
    ret.pushKV("n", (int)support.outPoint.n);
    ret.pushKV("height", support.nHeight);
    ret.pushKV("validAtHeight", support.nValidAtHeight);
    ret.pushKV("amount", support.nAmount);

    return ret;
}

UniValue claimAndSupportsToJSON(const CCoinsViewCache& coinsCache, const CClaimNsupports& claimNsupports)
{
    auto& claim = claimNsupports.claim;
    auto& supports = claimNsupports.supports;

    auto result = claimToJSON(coinsCache, claim);
    result.pushKV("effectiveAmount", claimNsupports.effectiveAmount);

    if (claim.nEffectiveAmount > claimNsupports.effectiveAmount)
        result.pushKV("pendingEffectiveAmount", claim.nEffectiveAmount);

    UniValue supportObjs(UniValue::VARR);
    for (auto& support : supports)
        supportObjs.push_back(supportToJSON(coinsCache, support));

    if (!supportObjs.empty())
        result.pushKV("supports", supportObjs);

    return result;
}

bool validParams(const UniValue& params, uint8_t required, uint8_t optional)
{
    auto count = params.size();
    return count >= required && count <= required + optional;
}

static UniValue getclaimsintrie(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getclaimsintrie\n"
            "Return all claims in the name trie. Deprecated.\n"
            "Arguments:\n"
            "1. \"blockhash\"     (string, optional) get claims in the trie\n"
            "                                        at the block specified\n"
            "                                        by this block hash.\n"
            "                                        If none is given,\n"
            "                                        the latest active\n"
            "                                        block will be used.\n"
            "Result: \n"
            "[\n"
            "  {\n"
            "    \"normalized_name\"    (string) the name of these claims (after normalization)\n"
            "    \"claims\": [          (array of object) the claims for this name\n"
            "      {\n"
            "        \"claimId\"        (string) the claimId of the claim\n"
            "        \"txid\"           (string) the txid of the claim\n"
            "        \"n\"              (numeric) the vout value of the claim\n"
            "        \"amount\"         (numeric) txout amount\n"
            "        \"height\"         (numeric) the height of the block in which this transaction is located\n"
            "        \"value\"          (string) the value of this claim\n"
            "        \"name\"           (string) the original name of this claim (before normalization)\n"
            "      }\n"
            "    ]\n"
            "  }\n"
            "]\n");

    if (!IsDeprecatedRPCEnabled("getclaimsintrie")) {
        const auto msg = "getclaimsintrie is deprecated and will be removed in v0.18. To use this command, start with -deprecatedrpc=getclaimsintrie";
        if (request.fHelp) {
            throw std::runtime_error(msg);
        }
        throw JSONRPCError(RPC_METHOD_DEPRECATED, msg);
    }

    UniValue ret(UniValue::VARR);
    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (!request.params.empty()) {
        CBlockIndex *blockIndex = BlockHashIndex(ParseHashV(request.params[0], "blockhash (optional parameter 1)"));
        RollBackTo(blockIndex, coinsCache, trieCache);
    }

    trieCache.recurseNodes({}, [&ret, &trieCache, &coinsCache] (const std::string& name, const CClaimTrieData& data) {
        if (ShutdownRequested())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Shutdown requested");

        boost::this_thread::interruption_point();

        if (data.empty())
            return;

        UniValue claims(UniValue::VARR);
        for (auto& claim : data.claims)
            claims.push_back(claimToJSON(coinsCache, claim));

        UniValue nodeObj(UniValue::VOBJ);
        nodeObj.pushKV("normalized_name", escapeNonUtf8(name));
        nodeObj.pushKV("claims", claims);
        ret.push_back(nodeObj);
    });
    return ret;
}

static UniValue getclaimtrie(const JSONRPCRequest& request)
{
    throw JSONRPCError(RPC_METHOD_DEPRECATED, "getclaimtrie was removed in v0.17.\n"
        "Clients should use getnamesintrie.");
}

static UniValue getnamesintrie(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 0, 1))
        throw std::runtime_error(
            "getnamesintrie\n"
            "Return all claim names in the trie.\n"
            "Arguments:\n"
            "1. \"blockhash\"     (string, optional) get claims in the trie\n"
            "                                        at the block specified\n"
            "                                        by this block hash.\n"
            "                                        If none is given,\n"
            "                                        the latest active\n"
            "                                        block will be used.\n"
            "Result: \n"
            "\"names\"            (array) all names in the trie that have claims\n");

    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (!request.params.empty()) {
        CBlockIndex *blockIndex = BlockHashIndex(ParseHashV(request.params[0], "blockhash (optional parameter 1)"));
        RollBackTo(blockIndex, coinsCache, trieCache);
    }
    UniValue ret(UniValue::VARR);

    trieCache.recurseNodes({}, [&ret](const std::string &name, const CClaimTrieData &data) {
        if (!data.empty())
            ret.push_back(escapeNonUtf8(name));
        if (ShutdownRequested())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Shutdown requested");

        boost::this_thread::interruption_point();
    });

    return ret;
}

static UniValue getvalueforname(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 1, 2))
        throw std::runtime_error(
            "getvalueforname \"name\"\n"
            "Return the winning or specified by claimId value associated with a name\n"
            "Arguments:\n"
            "1. \"name\"          (string) the name to look up\n"
            "2. \"blockhash\"     (string, optional) get the value\n"
            "                                        associated with the name\n"
            "                                        at the block specified\n"
            "                                        by this block hash.\n"
            "                                        If none is given,\n"
            "                                        the latest active\n"
            "                                        block will be used.\n"
            "3. \"claimId\"       (string, optional) can be partial one\n"
            "Result: \n"
            "\"value\"            (string) the value of the name, if it exists\n"
            "\"claimId\"          (string) the claimId for this name claim\n"
            "\"txid\"             (string) the hash of the transaction which successfully claimed the name\n"
            "\"n\"                (numeric) vout value\n"
            "\"amount\"           (numeric) txout amount\n"
            "\"effective amount\" (numeric) txout amount plus amount from all supports associated with the claim\n"
            "\"height\"           (numeric) the height of the block in which this transaction is located\n"
            "\"name\"             (string) the original name of this claim (before normalization)\n");

    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (request.params.size() > 1) {
        CBlockIndex* blockIndex = BlockHashIndex(ParseHashV(request.params[1], "blockhash (optional parameter 2)"));
        RollBackTo(blockIndex, coinsCache, trieCache);
    }

    uint160 claimId;
    std::string partialId;
    if (request.params.size() > 2)
        ParseClaimtrieId(request.params[2], partialId, claimId, "claimId (optional parameter 3)");

    const auto name = request.params[0].get_str();
    UniValue ret(UniValue::VOBJ);

    auto res = trieCache.getClaimsForName(name);
    if (res.claimsNsupports.empty())
        return ret;

    auto& claimNsupports =
        !claimId.IsNull() ? res.find(claimId) :
        !partialId.empty() ? res.find(partialId) : res.claimsNsupports[0];

    if (claimNsupports.IsNull())
        return ret;

    ret.pushKV("normalizedName", escapeNonUtf8(res.name));
    ret.pushKVs(claimAndSupportsToJSON(coinsCache, claimNsupports));
    ret.pushKV("lastTakeoverHeight", res.nLastTakeoverHeight);

    return ret;
}

UniValue getclaimsforname(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 1, 1))
        throw std::runtime_error(
            "getclaimsforname\n"
            "Return all claims and supports for a name\n"
            "Arguments: \n"
            "1.  \"name\"         (string) the name for which to get claims and supports\n"
            "2.  \"blockhash\"    (string, optional) get claims for name\n"
            "                                        at the block specified\n"
            "                                        by this block hash.\n"
            "                                        If none is given,\n"
            "                                        the latest active\n"
            "                                        block will be used.\n"
            "Result:\n"
            "{\n"
            "  \"nLastTakeoverHeight\" (numeric) the last height at which ownership of the name changed\n"
            "  \"normalized_name\"     (string) the name of these claims after normalization\n"
            "  \"claims\": [      (array of object) claims for this name\n"
            "    {\n"
            "      \"claimId\"    (string) the claimId of this claim\n"
            "      \"txid\"       (string) the txid of this claim\n"
            "      \"n\"          (numeric) the index of the claim in the transaction's list of outputs\n"
            "      \"nHeight\"    (numeric) the height at which the claim was included in the blockchain\n"
            "      \"nValidAtHeight\" (numeric) the height at which the claim became/becomes valid\n"
            "      \"nAmount\"    (numeric) the amount of the claim\n"
            "      \"value\"      (string) the metadata of the claim\n"
            "      \"nEffectiveAmount\" (numeric) the total effective amount of the claim, taking into effect whether the claim or support has reached its nValidAtHeight\n"
            "      \"supports\" : [ (array of object) supports for this claim\n"
            "        \"txid\"     (string) the txid of the support\n"
            "        \"n\"        (numeric) the index of the support in the transaction's list of outputs\n"
            "        \"nHeight\"  (numeric) the height at which the support was included in the blockchain\n"
            "        \"nValidAtHeight\" (numeric) the height at which the support became/becomes valid\n"
            "        \"nAmount\"  (numeric) the amount of the support\n"
            "        \"value\"    (string) the metadata of the support if any\n"
            "      ]\n"
            "      \"name\"       (string) the original name of this claim before normalization\n"
            "    }\n"
            "  ],\n"
            "  \"supports without claims\": [ (array of object) supports that did not match a claim for this name\n"
            "    {\n"
            "      \"txid\"     (string) the txid of the support\n"
            "      \"n\"        (numeric) the index of the support in the transaction's list of outputs\n"
            "      \"nHeight\"  (numeric) the height at which the support was included in the blockchain\n"
            "      \"nValidAtHeight\" (numeric) the height at which the support became/becomes valid\n"
            "      \"nAmount\"  (numeric) the amount of the support\n"
            "    }\n"
            "  ]\n"
            "}\n");

    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (request.params.size() > 1) {
        CBlockIndex* blockIndex = BlockHashIndex(ParseHashV(request.params[1], "blockhash (optional parameter 2)"));
        RollBackTo(blockIndex, coinsCache, trieCache);
    }

    std::string name = request.params[0].get_str();
    auto claimsForName = trieCache.getClaimsForName(name);

    UniValue result(UniValue::VOBJ);
    result.pushKV("normalized_name", escapeNonUtf8(claimsForName.name));

    UniValue claimObjs(UniValue::VARR);
    for (auto& claim : claimsForName.claimsNsupports)
        claimObjs.push_back(claimAndSupportsToJSON(coinsCache, claim));

    UniValue unmatchedSupports(UniValue::VARR);
    for (auto& support : claimsForName.unmatchedSupports)
        unmatchedSupports.push_back(supportToJSON(coinsCache, support));

    result.pushKV("claims", claimObjs);
    result.pushKV("lastTakeoverHeight", claimsForName.nLastTakeoverHeight);
    result.pushKV("supportsWithoutClaims", unmatchedSupports);
    return result;
}

UniValue getclaimbyid(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 1, 0))
        throw std::runtime_error(
            "getclaimbyid\n"
            "Get a claim by claim id\n"
            "Arguments: \n"
            "1.  \"claimId\"           (string) the claimId of this claim or patial id (at least 3 chars)\n"
            "Result:\n"
            "{\n"
            "  \"name\"                (string) the original name of the claim (before normalization)\n"
            "  \"normalized_name\"     (string) the name of this claim (after normalization)\n"
            "  \"value\"               (string) metadata of the claim\n"
            "  \"claimId\"             (string) the claimId of this claim\n"
            "  \"txid\"                (string) the hash of the transaction which has successfully claimed this name\n"
            "  \"n\"                   (numeric) vout value\n"
            "  \"amount\"              (numeric) txout value\n"
            "  \"effective amount\"    (numeric) txout amount plus amount from all supports associated with the claim\n"
            "  \"supports\"            (array of object) supports for this claim\n"
            "  [\n"
            "    \"txid\"              (string) the txid of the support\n"
            "    \"n\"                 (numeric) the index of the support in the transaction's list of outputs\n"
            "    \"height\"            (numeric) the height at which the support was included in the blockchain\n"
            "    \"valid at height\"   (numeric) the height at which the support is valid\n"
            "    \"amount\"            (numeric) the amount of the support\n"
            "    \"value\"             (string) the metadata of the support if any\n"
            "  ]\n"
            "  \"height\"              (numeric) the height of the block in which this claim transaction is located\n"
            "  \"valid at height\"     (numeric) the height at which the claim is valid\n"
            "}\n");

    LOCK(cs_main);
    CClaimTrieCache trieCache(pclaimTrie);
    CCoinsViewCache coinsCache(pcoinsTip.get());

    uint160 claimId;
    std::string partialId;
    ParseClaimtrieId(request.params[0], partialId, claimId, "Claim-id (parameter 1)");

    if (claimId.IsNull() && partialId.length() < 3)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Claim-id (parameter 1) should be at least 3 chars");

    UniValue claim(UniValue::VOBJ);
    std::string name;
    CClaimValue claimValue;
    if (getClaimById(claimId, name, &claimValue) || getClaimById(partialId, name, &claimValue)) {
        auto res = trieCache.getClaimsForName(name);
        auto& claimNsupports = !claimId.IsNull() ? res.find(claimId) : res.find(partialId);
        if (!claimNsupports.IsNull()) {
            claim.pushKV("normalizedName", escapeNonUtf8(res.name));
            claim.pushKVs(claimAndSupportsToJSON(coinsCache, claimNsupports));
            claim.pushKV("lastTakeoverHeight", res.nLastTakeoverHeight);
        }
    }
    return claim;
}

UniValue gettotalclaimednames(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 0, 0))
        throw std::runtime_error(
            "gettotalclaimednames\n"
            "Return the total number of names that have been\n"
            "successfully claimed, and therefore exist in the trie\n"
            "Arguments:\n"
            "Result:\n"
            "\"total names\"                (numeric) the total number of\n"
            "                                         names in the trie\n"
        );
    LOCK(cs_main);
    auto num_names = pclaimTrie->getTotalNamesInTrie();
    return int(num_names);
}

UniValue gettotalclaims(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 0, 0))
        throw std::runtime_error(
            "gettotalclaims\n"
            "Return the total number of active claims in the trie\n"
            "Arguments:\n"
            "Result:\n"
            "\"total claims\"             (numeric) the total number\n"
            "                                       of active claims\n"
        );
    LOCK(cs_main);
    auto num_claims = pclaimTrie->getTotalClaimsInTrie();
    return int(num_claims);
}

UniValue gettotalvalueofclaims(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 0, 1))
        throw std::runtime_error(
            "gettotalvalueofclaims\n"
            "Return the total value of the claims in the trie\n"
            "Arguments:\n"
            "1. \"controlling_only\"         (boolean) only include the value\n"
            "                                          of controlling claims\n"
            "Result:\n"
            "\"total value\"                 (numeric) the total value of the\n"
            "                                          claims in the trie\n"
        );
    LOCK(cs_main);
    bool controlling_only = false;
    if (request.params.size() == 1)
        controlling_only = request.params[0].get_bool();
    auto total_amount = pclaimTrie->getTotalValueOfClaimsInTrie(controlling_only);
    return ValueFromAmount(total_amount);
}

UniValue getclaimsfortx(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 1, 0))
        throw std::runtime_error(
            "getclaimsfortx\n"
            "Return any claims or supports found in a transaction\n"
            "Arguments:\n"
            "1.  \"txid\"         (string) the txid of the transaction to check for unspent claims\n"
            "Result:\n"
            "[\n"
            "  {\n"
            "    \"nOut\"                   (numeric) the index of the claim or support in the transaction's list of outputs\n"
            "    \"claim type\"             (string) 'claim' or 'support'\n"
            "    \"name\"                   (string) the name claimed or supported\n"
            "    \"claimId\"                (string) if a claim, its ID\n"
            "    \"value\"                  (string) if a name claim, the value of the claim\n"
            "    \"supported txid\"         (string) if a support, the txid of the supported claim\n"
            "    \"supported nout\"         (numeric) if a support, the index of the supported claim in its transaction\n"
            "    \"depth\"                  (numeric) the depth of the transaction in the main chain\n"
            "    \"in claim trie\"          (boolean) if a name claim, whether the claim is active, i.e. has made it into the trie\n"
            "    \"is controlling\"         (boolean) if a name claim, whether the claim is the current controlling claim for the name\n"
            "    \"in support map\"         (boolean) if a support, whether the support is active, i.e. has made it into the support map\n"
            "    \"in queue\"               (boolean) whether the claim is in a queue waiting to be inserted into the trie or support map\n"
            "    \"blocks to valid\"        (numeric) if in a queue, the number of blocks until it's inserted into the trie or support map\n"
            "  }\n"
            "]\n"
        );

    LOCK(cs_main);
    uint256 hash = ParseHashV(request.params[0], "txid (parameter 1)");
    UniValue ret(UniValue::VARR);

    int op;
    std::vector<std::vector<unsigned char> > vvchParams;

    CClaimTrieCache trieCache(pclaimTrie);
    CCoinsViewCache view(pcoinsTip.get());
    const Coin& coin = AccessByTxid(view, hash);
    std::vector<CTxOut> txouts{ coin.out };
    int nHeight = coin.nHeight;

    for (unsigned int i = 0; i < txouts.size(); ++i)
    {
        if (!txouts[i].IsNull())
        {
            vvchParams.clear();
            const CTxOut& txout = txouts[i];
            UniValue o(UniValue::VOBJ);
            if (DecodeClaimScript(txout.scriptPubKey, op, vvchParams))
            {
                o.pushKV("nOut", static_cast<int64_t>(i));
                std::string sName(vvchParams[0].begin(), vvchParams[0].end());
                o.pushKV("name", escapeNonUtf8(sName));
                if (op == OP_CLAIM_NAME)
                {
                    uint160 claimId = ClaimIdHash(hash, i);
                    o.pushKV("claimId", claimId.GetHex());
                    o.pushKV("value", HexStr(vvchParams[1].begin(), vvchParams[1].end()));
                }
                else if (op == OP_UPDATE_CLAIM)
                {
                    uint160 claimId(vvchParams[1]);
                    o.pushKV("claimId", claimId.GetHex());
                    o.pushKV("value", HexStr(vvchParams[2].begin(), vvchParams[2].end()));
                }
                else if (op == OP_SUPPORT_CLAIM)
                {
                    uint160 supportedClaimId(vvchParams[1]);
                    o.pushKV("supported claimId", supportedClaimId.GetHex());
                    if (vvchParams.size() > 2)
                        o.pushKV("supported value", HexStr(vvchParams[2].begin(), vvchParams[2].end()));
                }
                if (nHeight > 0)
                {
                    o.pushKV("depth", chainActive.Height() - nHeight);
                    if (op == OP_CLAIM_NAME || op == OP_UPDATE_CLAIM)
                    {
                        bool inClaimTrie = trieCache.haveClaim(sName, COutPoint(hash, i));
                        o.pushKV("in claim trie", inClaimTrie);
                        if (inClaimTrie)
                        {
                            CClaimValue claim;
                            if (!trieCache.getInfoForName(sName, claim))
                            {
                                LogPrintf("HaveClaim was true but getInfoForName returned false.");
                            }
                            o.pushKV("is controlling", (claim.outPoint.hash == hash && claim.outPoint.n == i));
                        }
                        else
                        {
                            int nValidAtHeight;
                            if (trieCache.haveClaimInQueue(sName, COutPoint(hash, i), nValidAtHeight))
                            {
                                o.pushKV("in queue", true);
                                o.pushKV("blocks to valid", nValidAtHeight - chainActive.Height());
                            }
                            else
                            {
                                o.pushKV("in queue", false);
                            }
                        }
                    }
                    else if (op == OP_SUPPORT_CLAIM)
                    {
                        bool inSupportMap = trieCache.haveSupport(sName, COutPoint(hash, i));
                        o.pushKV("in support map", inSupportMap);
                        if (!inSupportMap)
                        {
                            int nValidAtHeight;
                            if (trieCache.haveSupportInQueue(sName, COutPoint(hash, i), nValidAtHeight))
                            {
                                o.pushKV("in queue", true);
                                o.pushKV("blocks to valid", nValidAtHeight - chainActive.Height());
                            }
                            else
                            {
                                o.pushKV("in queue", false);
                            }
                        }
                    }
                }
                else
                {
                    o.pushKV("depth", 0);
                    if (op == OP_CLAIM_NAME || op == OP_UPDATE_CLAIM)
                    {
                        o.pushKV("in claim trie", false);
                    }
                    else if (op == OP_SUPPORT_CLAIM)
                    {
                        o.pushKV("in support map", false);
                    }
                    o.pushKV("in queue", false);
                }
                ret.push_back(o);
            }
        }
    }
    return ret;
}

UniValue proofToJSON(const CClaimTrieProof& proof)
{
    UniValue result(UniValue::VOBJ);
    UniValue nodes(UniValue::VARR);

    for (const auto& itNode : proof.nodes) {
        UniValue node(UniValue::VOBJ);
        UniValue children(UniValue::VARR);

        for (const auto& itChildren : itNode.children) {
            UniValue child(UniValue::VOBJ);
            child.pushKV("character", itChildren.first);
            if (!itChildren.second.IsNull())
                child.pushKV("nodeHash", itChildren.second.GetHex());
            children.push_back(child);
        }

        node.pushKV("children", children);

        if (itNode.hasValue && !itNode.valHash.IsNull())
            node.pushKV("valueHash", itNode.valHash.GetHex());

        nodes.push_back(node);
    }

    if (!nodes.empty())
        result.push_back(Pair("nodes", nodes));

    UniValue pairs(UniValue::VARR);

    for (const auto& itPair : proof.pairs) {
        UniValue child(UniValue::VOBJ);
        child.push_back(Pair("odd", itPair.first));
        child.push_back(Pair("hash", itPair.second.GetHex()));
        pairs.push_back(child);
    }

    if (!pairs.empty())
        result.push_back(Pair("pairs", pairs));

    if (proof.hasValue) {
        result.pushKV("txhash", proof.outPoint.hash.GetHex());
        result.pushKV("nOut", (int)proof.outPoint.n);
        result.pushKV("last takeover height", (int)proof.nHeightOfLastTakeover);
    }
    return result;
}

UniValue getnameproof(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 1, 2))
        throw std::runtime_error(
            "getnameproof\n"
            "Return the cryptographic proof that a name maps to a value\n"
            "or doesn't.\n"
            "Arguments:\n"
            "1. \"name\"           (string) the name to get a proof for\n"
            "2. \"blockhash\"      (string, optional) the hash of the block\n"
            "                                            which is the basis\n"
            "                                            of the proof. If\n"
            "                                            none is given, \n"
            "                                            the latest block\n"
            "                                            will be used.\n"
            "3. \"claimId\"        (string, optional, post-fork) for validating a specific claim\n"
            "Result: \n"
            "{\n"
            "  \"nodes\" : [       (array of object, pre-fork) full nodes (i.e.\n"
            "                                        those which lead to\n"
            "                                        the requested name)\n"
            "    \"children\" : [  (array of object) the children of\n"
            "                                       this node\n"
            "      \"child\" : {   (object) a child node, either leaf or\n"
            "                               reference to a full node\n"
            "        \"character\" : \"char\" (string) the character which\n"
            "                                          leads from the parent\n"
            "                                          to this child node\n"
            "        \"nodeHash\" :  \"hash\" (string, if exists) the hash of\n"
            "                                                     the node if\n"
            "                                                     this is a \n"
            "                                                     leaf node\n"
            "        }\n"
            "      ]\n"
            "    \"valueHash\"     (string, if exists) the hash of this\n"
            "                                          node's value, if\n"
            "                                          it has one. If \n"
            "                                          this is the\n"
            "                                          requested name\n"
            "                                          this will not\n"
            "                                          exist whether\n"
            "                                          the node has a\n"
            "                                          value or not\n"
            "    ]\n"
            "  \"pairs\" : [       (array of pairs, post-fork) hash can be validated by \n"
            "                                                  hashing claim from the bottom up\n"
            "                {\n"
            "                    \"odd\"      (boolean) this value goes on the right of hash\n"
            "                    \"hash\"     (boolean) the hash to be mixed in\n"
            "                }\n"
            "              ]\n"
            "  \"txhash\" : \"hash\" (string, if exists) the txid of the\n"
            "                                            claim which controls\n"
            "                                            this name, if there\n"
            "                                            is one.\n"
            "  \"nOut\" : n,         (numeric) the nOut of the claim which\n"
            "                                  controls this name, if there\n"
            "                                  is one.\n"
            "  \"last takeover height\"  (numeric) the most recent height at\n"
            "                                      which the value of a name\n"
            "                                      changed other than through\n"
            "                                      an update to the winning\n"
            "                                      bid\n"
            "  }\n"
            "}\n");

    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (request.params.size() > 1) {
        CBlockIndex* pblockIndex = BlockHashIndex(ParseHashV(request.params[1], "blockhash (optional parameter 2)"));
        RollBackTo(pblockIndex, coinsCache, trieCache);
    }

    uint160 claimId;
    std::string partialId;
    if (request.params.size() > 2)
        ParseClaimtrieId(request.params[2], partialId, claimId, "claimId (optional parameter 3)");

    std::function<bool(const CClaimValue&)> comp;
    if (!claimId.IsNull())
        comp = [&claimId](const CClaimValue& claim) {
            return claim.claimId == claimId;
        };
    else
        comp = [&partialId](const CClaimValue& claim) {
            return claim.claimId.GetHex().find(partialId) == 0;
        };

    CClaimTrieProof proof;
    std::string name = request.params[0].get_str();
    if (!trieCache.getProofForName(name, proof, comp))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate proof");

    return proofToJSON(proof);
}

UniValue checknormalization(const JSONRPCRequest& request)
{
    if (request.fHelp || !validParams(request.params, 1, 0))
        throw std::runtime_error(
            "checknormalization\n"
            "Given an unnormalized name of a claim, return normalized version of it\n"
            "Arguments:\n"
            "1. \"name\"           (string) the name to normalize\n"
            "Result: \n"
            "\"normalized\"        (string) fully normalized name\n");

    const bool force = true;
    const std::string name = request.params[0].get_str();

    CClaimTrieCache triecache(pclaimTrie);
    return triecache.normalizeClaimName(name, force);
}

static const CRPCCommand commands[] =
{ //  category              name                            actor (function)            argNames
  //  --------------------- ------------------------        -----------------------     ----------
    { "Claimtrie",          "getclaimsintrie",              &getclaimsintrie,           { "blockhash" } },
    { "Claimtrie",          "getnamesintrie",               &getnamesintrie,            { "blockhash" } },
    { "hidden",             "getclaimtrie",                 &getclaimtrie,              { } },
    { "Claimtrie",          "getvalueforname",              &getvalueforname,           { "name","blockhash","claimId" } },
    { "Claimtrie",          "getclaimsforname",             &getclaimsforname,          { "name","blockhash" } },
    { "Claimtrie",          "gettotalclaimednames",         &gettotalclaimednames,      { "" } },
    { "Claimtrie",          "gettotalclaims",               &gettotalclaims,            { "" } },
    { "Claimtrie",          "gettotalvalueofclaims",        &gettotalvalueofclaims,     { "controlling_only" } },
    { "Claimtrie",          "getclaimsfortx",               &getclaimsfortx,            { "txid" } },
    { "Claimtrie",          "getnameproof",                 &getnameproof,              { "name","blockhash","claimId"} },
    { "Claimtrie",          "getclaimbyid",                 &getclaimbyid,              { "claimId" } },
    { "Claimtrie",          "checknormalization",           &checknormalization,        { "name" }},
};

void RegisterClaimTrieRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
