#include <claimtrie.h>
#include <coins.h>
#include <core_io.h>
#include <nameclaim.h>
#include <rpc/server.h>
#include <shutdown.h>
#include <txdb.h>
#include <txmempool.h>
#include <univalue.h>
#include <validation.h>

#include <boost/thread.hpp>
#include <cmath>

uint160 ParseClaimtrieId(const UniValue& v, const std::string& strName)
{
    static constexpr size_t claimIdHexLength = 40;

    std::string strHex;
    if (v.isStr())
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName + " must be a 20-character hexadecimal string (not '" + strHex + "')");
    if (strHex.length() != claimIdHexLength)
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("%s must be of length %d (not %d)", strName, claimIdHexLength, strHex.length()));

    uint160 result;
    result.SetHex(strHex);
    return result;
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
}

static UniValue getclaimsintrie(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getclaimsintrie\n"
            "Return all claims in the name trie.\n"
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

    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (!request.params.empty()) {
        CBlockIndex* blockIndex = BlockHashIndex(ParseHashV(request.params[0], "blockhash (optional parameter 1)"));
        RollBackTo(blockIndex, coinsCache, trieCache);
    }

    class CClaimsCallback : public CNodeCallback
    {
    public:
        CClaimsCallback(UniValue& ret, const CCoinsViewCache& coinsCache) : nodes(ret), coinsCache(coinsCache)
        {
        }

        void visit(const std::string& name, const CClaimTrieNode* node)
        {
            if (ShutdownRequested())
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Shutdown requested");

            boost::this_thread::interruption_point();

            if (node->claims.empty())
                return;

            UniValue claims(UniValue::VARR);
            for (std::vector<CClaimValue>::const_iterator itClaims = node->claims.begin(); itClaims != node->claims.end(); ++itClaims) {
                UniValue claim(UniValue::VOBJ);
                claim.pushKV("claimId", itClaims->claimId.GetHex());
                claim.pushKV("txid", itClaims->outPoint.hash.GetHex());
                claim.pushKV("n", (int)itClaims->outPoint.n);
                claim.pushKV("amount", ValueFromAmount(itClaims->nAmount));
                claim.pushKV("height", itClaims->nHeight);
                const Coin& coin = coinsCache.AccessCoin(itClaims->outPoint);
                if (coin.IsSpent())
                {
                    LogPrintf("%s: the specified txout of %s appears to have been spent\n", __func__, itClaims->outPoint.hash.GetHex());
                    claim.pushKV("error", "Txout spent");
                }
                else
                {
                    int op;
                    std::vector<std::vector<unsigned char> > vvchParams;
                    if (!DecodeClaimScript(coin.out.scriptPubKey, op, vvchParams))
                    {
                        LogPrintf("%s: the specified txout of %s does not have an claim command\n", __func__, itClaims->outPoint.hash.GetHex());
                    }
                    claim.pushKV("value", HexStr(vvchParams[1].begin(), vvchParams[1].end()));
                }
                std::string targetName;
                CClaimValue targetClaim;
                if (pclaimTrie->getClaimById(itClaims->claimId, targetName, targetClaim))
                    claim.push_back(Pair("name", targetName));

                claims.push_back(claim);
            }

            UniValue nodeObj(UniValue::VOBJ);
            nodeObj.pushKV("normalized_name", name);
            nodeObj.pushKV("claims", claims);
            nodes.push_back(nodeObj);
        }

    private:
        UniValue& nodes;
        const CCoinsViewCache& coinsCache;
    };

    UniValue ret(UniValue::VARR);
    CClaimsCallback claimsCallback(ret, coinsCache);
    trieCache.iterateTrie(claimsCallback);
    return ret;
}

static UniValue getclaimtrie(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getclaimtrie\n"
            "DEPRECATED. Return the entire claim trie.\n"
            "Arguments:\n"
            "1. \"blockhash\"     (string, optional) get claim in the trie\n"
            "                                        at the block specified\n"
            "                                        by this block hash.\n"
            "                                        If none is given,\n"
            "                                        the latest active\n"
            "                                        block will be used.\n"
            "Result: \n"
            "[\n"
            " {\n"
            "  \"name\"           (string) the name of the node\n"
            "  \"hash\"           (string) the hash of the node\n"
            "  \"txid\"           (string) (if value exists) the hash of the transaction which has successfully claimed this name\n"
            "  \"n\"              (numeric) (if value exists) vout value\n"
            "  \"value\"          (numeric) (if value exists) txout value\n"
            "  \"height\"         (numeric) (if value exists) the height of the block in which this transaction is located\n"
            " }\n"
            "]\n");

    LOCK(cs_main);

    CCoinsViewCache coinsCache(pcoinsTip.get());
    CClaimTrieCache trieCache(pclaimTrie);

    if (!request.params.empty()) {
        CBlockIndex* blockIndex = BlockHashIndex(ParseHashV(request.params[0], "blockhash (optional parameter 1)"));
        RollBackTo(blockIndex, coinsCache, trieCache);
    }

    class CClaimCallback : public CNodeCallback
    {
    public:
        CClaimCallback(UniValue& ret) : nodes(ret)
        {
        }

        void visit(const std::string& name, const CClaimTrieNode* node)
        {
            if (ShutdownRequested())
                throw JSONRPCError(RPC_INTERNAL_ERROR, "Shutdown requested");

            boost::this_thread::interruption_point();

            UniValue nodeObj(UniValue::VOBJ);
            nodeObj.pushKV("name", name);
            nodeObj.pushKV("hash", node->hash.GetHex());
            CClaimValue claim;
            if (node->getBestClaim(claim)) {
                nodeObj.pushKV("txid", claim.outPoint.hash.GetHex());
                nodeObj.pushKV("n", (int)claim.outPoint.n);
                nodeObj.pushKV("value", ::ValueFromAmount(claim.nAmount));
                nodeObj.pushKV("height", claim.nHeight);
            }
            nodes.push_back(nodeObj);
        }

    private:
        UniValue& nodes;
    };

    UniValue ret(UniValue::VARR);
    CClaimCallback claimCallback(ret);
    trieCache.iterateTrie(claimCallback);
    return ret;
}

static bool getValueForOutPoint(const CCoinsViewCache& coinsCache, const COutPoint& out, std::string& sValue)
{
    const Coin& coin = coinsCache.AccessCoin(out);
    if (coin.IsSpent())
    {
        LogPrintf("%s: the specified txout of %s appears to have been spent\n", __func__, out.hash.GetHex());
        return true;
    }
    
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    if (!DecodeClaimScript(coin.out.scriptPubKey, op, vvchParams))
    {
        LogPrintf("%s: the specified txout of %s does not have a name claim command\n", __func__, out.hash.GetHex());
        return false;
    }
    if (op == OP_CLAIM_NAME)
    {
        sValue = HexStr(vvchParams[1].begin(), vvchParams[1].end());
        return true;
    }
    else if (vvchParams.size() > 2) // both UPDATE and SUPPORT
    {
        sValue = HexStr(vvchParams[2].begin(), vvchParams[2].end());
        return true;
    }
    return false;
}


static UniValue getvalueforname(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getvalueforname \"name\"\n"
            "Return the winning value associated with a name, if one exists\n"
            "Arguments:\n"
            "1. \"name\"          (string) the name to look up\n"
            "2. \"blockhash\"     (string, optional) get the value\n"
            "                                        associated with the name\n"
            "                                        at the block specified\n"
            "                                        by this block hash.\n"
            "                                        If none is given,\n"
            "                                        the latest active\n"
            "                                        block will be used.\n"
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

    const auto& name = request.params[0].get_str();
    UniValue ret(UniValue::VOBJ);

    CClaimValue claim;
    if (!trieCache.getInfoForName(name, claim))
        return ret; // they may have asked for a name that doesn't exist (which is not an error)

    std::string sValue;
    if (!getValueForOutPoint(coinsCache, claim.outPoint, sValue))
        return ret;

    const auto nEffectiveAmount = trieCache.getEffectiveAmountForClaim(name, claim.claimId);

    ret.pushKV("value", sValue);
    ret.pushKV("claimId", claim.claimId.GetHex());
    ret.pushKV("txid", claim.outPoint.hash.GetHex());
    ret.pushKV("n", (int)claim.outPoint.n);
    ret.pushKV("amount", claim.nAmount);
    ret.pushKV("effective amount", nEffectiveAmount);
    ret.pushKV("height", claim.nHeight);

    std::string targetName;
    CClaimValue targetClaim;
    if (pclaimTrie->getClaimById(claim.claimId, targetName, targetClaim))
        ret.push_back(Pair("name", targetName));

    return ret;
}

typedef std::pair<CClaimValue, std::vector<CSupportValue> > claimAndSupportsType;
typedef std::map<uint160, claimAndSupportsType> claimSupportMapType;

UniValue supportToJSON(const CCoinsViewCache& coinsCache, const CSupportValue& support)
{
    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("txid", support.outPoint.hash.GetHex()));
    ret.push_back(Pair("n", (int)support.outPoint.n));
    ret.push_back(Pair("nHeight", support.nHeight));
    ret.push_back(Pair("nValidAtHeight", support.nValidAtHeight));
    ret.push_back(Pair("nAmount", support.nAmount));
    std::string value;
    if (getValueForOutPoint(coinsCache, support.outPoint, value))
        ret.push_back(Pair("value", value));
    return ret;
}

UniValue claimAndSupportsToJSON(const CCoinsViewCache& coinsCache, CAmount nEffectiveAmount, claimSupportMapType::const_iterator itClaimsAndSupports)
{
    const CClaimValue& claim = itClaimsAndSupports->second.first;
    const std::vector<CSupportValue>& supports = itClaimsAndSupports->second.second;

    UniValue supportObjs(UniValue::VARR);
    for (const auto& support: supports)
        supportObjs.push_back(supportToJSON(coinsCache, support));

    UniValue result(UniValue::VOBJ);
    result.pushKV("claimId", itClaimsAndSupports->first.GetHex());
    result.pushKV("txid", claim.outPoint.hash.GetHex());
    result.pushKV("n", (int)claim.outPoint.n);
    result.pushKV("nHeight", claim.nHeight);
    result.pushKV("nValidAtHeight", claim.nValidAtHeight);
    result.pushKV("nAmount", claim.nAmount);
    std::string sValue;
    if (getValueForOutPoint(coinsCache, claim.outPoint, sValue))
        result.pushKV("value", sValue);
    result.pushKV("nEffectiveAmount", nEffectiveAmount);
    result.pushKV("supports", supportObjs);

    std::string targetName;
    CClaimValue targetClaim;
    if (pclaimTrie->getClaimById(claim.claimId, targetName, targetClaim))
        result.push_back(Pair("name", targetName));

    return result;
}

UniValue getclaimsforname(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
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
    claimsForNameType claimsForName = trieCache.getClaimsForName(name);

    UniValue claimObjs(UniValue::VARR);
    claimSupportMapType claimSupportMap;
    UniValue unmatchedSupports(UniValue::VARR);

    for (auto itClaims = claimsForName.claims.begin(); itClaims != claimsForName.claims.end(); ++itClaims)
    {
        claimAndSupportsType claimAndSupports = std::make_pair(*itClaims, std::vector<CSupportValue>());
        claimSupportMap.emplace(itClaims->claimId, claimAndSupports);
    }

    for (auto itSupports = claimsForName.supports.begin(); itSupports != claimsForName.supports.end(); ++itSupports)
    {
        auto itClaimAndSupports = claimSupportMap.find(itSupports->supportedClaimId);
        if (itClaimAndSupports == claimSupportMap.end())
            unmatchedSupports.push_back(supportToJSON(coinsCache, *itSupports));
        else
            itClaimAndSupports->second.second.push_back(*itSupports);
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("nLastTakeoverHeight", claimsForName.nLastTakeoverHeight);
    result.pushKV("normalized_name", claimsForName.name);

    for (auto itClaims = claimsForName.claims.begin(); itClaims != claimsForName.claims.end(); ++itClaims)
    {
        auto itClaimsAndSupports = claimSupportMap.find(itClaims->claimId);
        const auto nEffectiveAmount = trieCache.getEffectiveAmountForClaim(claimsForName, itClaimsAndSupports->first);
        UniValue claimObj = claimAndSupportsToJSON(coinsCache, nEffectiveAmount, itClaimsAndSupports);
        claimObjs.push_back(claimObj);
    }

    result.pushKV("claims", claimObjs);
    result.pushKV("supports without claims", unmatchedSupports);
    return result;
}

UniValue getclaimbyid(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getclaimbyid\n"
            "Get a claim by claim id\n"
            "Arguments: \n"
            "1.  \"claimId\"           (string) the claimId of this claim\n"
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
    uint160 claimId = ParseClaimtrieId(request.params[0], "Claim-id (parameter 1)");
    UniValue claim(UniValue::VOBJ);
    std::string name;
    CClaimValue claimValue;
    pclaimTrie->getClaimById(claimId, name, claimValue);
    if (claimValue.claimId == claimId)
    {
        std::vector<CSupportValue> supports;
        CClaimTrieCache trieCache(pclaimTrie);
        CAmount effectiveAmount = trieCache.getEffectiveAmountForClaim(name, claimValue.claimId, &supports);

        std::string sValue;
        claim.pushKV("name", name);
        if (trieCache.shouldNormalize())
            claim.push_back(Pair("normalized_name", trieCache.normalizeClaimName(name, true)));
        CCoinsViewCache coinsCache(pcoinsTip.get());
        if (getValueForOutPoint(coinsCache, claimValue.outPoint, sValue))
            claim.pushKV("value", sValue);
        claim.pushKV("claimId", claimValue.claimId.GetHex());
        claim.pushKV("txid", claimValue.outPoint.hash.GetHex());
        claim.pushKV("n", (int) claimValue.outPoint.n);
        claim.pushKV("amount", claimValue.nAmount);
        claim.pushKV("effective amount", effectiveAmount);
        UniValue supportList(UniValue::VARR);
        for(const CSupportValue& support: supports) {
            UniValue supportEntry(UniValue::VOBJ);
            supportEntry.pushKV("txid", support.outPoint.hash.GetHex());
            supportEntry.pushKV("n", (int)support.outPoint.n);
            supportEntry.pushKV("height", support.nHeight);
            supportEntry.pushKV("valid at height", support.nValidAtHeight);
            supportEntry.pushKV("amount", support.nAmount);
            if (getValueForOutPoint(coinsCache, support.outPoint, sValue))
                claim.pushKV("value", sValue);
            supportList.pushKVs(supportEntry);
        }
        claim.pushKV("supports", supportList);
        claim.pushKV("height", claimValue.nHeight);
        claim.pushKV("valid at height", claimValue.nValidAtHeight);
    }
    return claim;
}

UniValue gettotalclaimednames(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
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
    if (!pclaimTrie)
    {
        return -1;
    }
    unsigned int num_names = pclaimTrie->getTotalNamesInTrie();
    return int(num_names);
}

UniValue gettotalclaims(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "gettotalclaims\n"
            "Return the total number of active claims in the trie\n"
            "Arguments:\n"
            "Result:\n"
            "\"total claims\"             (numeric) the total number\n"
            "                                       of active claims\n"
        );
    LOCK(cs_main);
    if (!pclaimTrie)
    {
        return -1;
    }
    unsigned int num_claims = pclaimTrie->getTotalClaimsInTrie();
    return int(num_claims);
}

UniValue gettotalvalueofclaims(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
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
    if (!pclaimTrie)
    {
        return -1;
    }
    bool controlling_only = false;
    if (request.params.size() == 1)
        controlling_only = request.params[0].get_bool();
    CAmount total_amount = pclaimTrie->getTotalValueOfClaimsInTrie(controlling_only);
    return ValueFromAmount(total_amount);
}

UniValue getclaimsfortx(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
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
                o.pushKV("name", sName);
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
                        bool inClaimTrie = pclaimTrie->haveClaim(sName, COutPoint(hash, i));
                        o.pushKV("in claim trie", inClaimTrie);
                        if (inClaimTrie)
                        {
                            CClaimValue claim;
                            if (!pclaimTrie->getInfoForName(sName, claim))
                            {
                                LogPrintf("HaveClaim was true but getInfoForName returned false.");
                            }
                            o.pushKV("is controlling", (claim.outPoint.hash == hash && claim.outPoint.n == i));
                        }
                        else
                        {
                            int nValidAtHeight;
                            if (pclaimTrie->haveClaimInQueue(sName, COutPoint(hash, i), nValidAtHeight))
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
                        bool inSupportMap = pclaimTrie->haveSupport(sName, COutPoint(hash, i));
                        o.pushKV("in support map", inSupportMap);
                        if (!inSupportMap)
                        {
                            int nValidAtHeight;
                            if (pclaimTrie->haveSupportInQueue(sName, COutPoint(hash, i), nValidAtHeight))
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
    for (std::vector<CClaimTrieProofNode>::const_iterator itNode = proof.nodes.begin(); itNode != proof.nodes.end(); ++itNode)
    {
        UniValue node(UniValue::VOBJ);
        UniValue children(UniValue::VARR);
        for (std::vector<std::pair<unsigned char, uint256> >::const_iterator itChildren = itNode->children.begin(); itChildren != itNode->children.end(); ++itChildren)
        {
            UniValue child(UniValue::VOBJ);
            child.pushKV("character", itChildren->first);
            if (!itChildren->second.IsNull())
            {
                child.pushKV("nodeHash", itChildren->second.GetHex());
            }
            children.push_back(child);
        }
        node.pushKV("children", children);
        if (itNode->hasValue && !itNode->valHash.IsNull())
        {
            node.pushKV("valueHash", itNode->valHash.GetHex());
        }
        nodes.push_back(node);
    }
    result.pushKV("nodes", nodes);
    if (proof.hasValue)
    {
        result.pushKV("txhash", proof.outPoint.hash.GetHex());
        result.pushKV("nOut", (int)proof.outPoint.n);
        result.pushKV("last takeover height", (int)proof.nHeightOfLastTakeover);
    }
    return result;
}

UniValue getnameproof(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 1 && request.params.size() != 2))
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
            "Result: \n"
            "{\n"
            "  \"nodes\" : [       (array of object) full nodes (i.e.\n"
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

    if (request.params.size() == 2) {
        CBlockIndex* pblockIndex = BlockHashIndex(ParseHashV(request.params[1], "blockhash (optional parameter 2)"));
        RollBackTo(pblockIndex, coinsCache, trieCache);
    }

    CClaimTrieProof proof;
    std::string name = request.params[0].get_str();
    if (!trieCache.getProofForName(name, proof))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate proof");

    return proofToJSON(proof);
}

UniValue checknormalization(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
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
    { "Claimtrie",          "getclaimtrie",                 &getclaimtrie,              { "blockhash" } },
    { "Claimtrie",          "getvalueforname",              &getvalueforname,           { "name","blockhash" } },
    { "Claimtrie",          "getclaimsforname",             &getclaimsforname,          { "name","blockhash" } },
    { "Claimtrie",          "gettotalclaimednames",         &gettotalclaimednames,      { "" } },
    { "Claimtrie",          "gettotalclaims",               &gettotalclaims,            { "" } },
    { "Claimtrie",          "gettotalvalueofclaims",        &gettotalvalueofclaims,     { "controlling_only" } },
    { "Claimtrie",          "getclaimsfortx",               &getclaimsfortx,            { "txid" } },
    { "Claimtrie",          "getnameproof",                 &getnameproof,              { "name","blockhash"} },
    { "Claimtrie",          "getclaimbyid",                 &getclaimbyid,              { "claimId" } },
    { "Claimtrie",          "checknormalization",           &checknormalization,        { "name" }},
};

void RegisterClaimTrieRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
