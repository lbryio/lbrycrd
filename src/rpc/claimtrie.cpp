#include "main.h"
#include "nameclaim.h"
#include "rpc/server.h"
#include "univalue.h"
#include "txmempool.h"

// Maximum block decrement that is allowed from rpc calls
const int MAX_RPC_BLOCK_DECREMENTS = 50;

UniValue getclaimsintrie(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            "getclaimsintrie\n"
            "Return all claims in the name trie.\n"
            "Arguments:\n"
            "None\n"
            "Result: \n"
            "[\n"
            "  {\n"
            "    \"name\"          (string) the name claimed\n"
            "    \"claims\": [      (array of object) the claims for this name\n"
            "      {\n"
            "        \"txid\"    (string) the txid of the claim\n"
            "        \"n\"       (numeric) the vout value of the claim\n"
            "        \"amount\"  (numeric) txout amount\n"
            "        \"height\"  (numeric) the height of the block in which this transaction is located\n"
            "        \"value\"   (string) the value of this claim\n"
            "      }\n"
            "    ]\n"
            "  }\n"
            "]\n"
        );
    
    LOCK(cs_main);
    UniValue ret(UniValue::VARR);

    CCoinsViewCache view(pcoinsTip);
    std::vector<namedNodeType> nodes = pclaimTrie->flattenTrie();

    for (std::vector<namedNodeType>::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        if (!it->second.claims.empty())
        {
            UniValue node(UniValue::VOBJ);
            node.push_back(Pair("name", it->first));
            UniValue claims(UniValue::VARR);
            for (std::vector<CClaimValue>::iterator itClaims = it->second.claims.begin(); itClaims != it->second.claims.end(); ++itClaims)
            {
                UniValue claim(UniValue::VOBJ);
                claim.push_back(Pair("txid", itClaims->outPoint.hash.GetHex()));
                claim.push_back(Pair("n", (int)itClaims->outPoint.n));
                claim.push_back(Pair("amount", ValueFromAmount(itClaims->nAmount)));
                claim.push_back(Pair("height", itClaims->nHeight));
                const CCoins* coin = view.AccessCoins(itClaims->outPoint.hash);
                if (!coin)
                {
                    LogPrintf("%s: %s does not exist in the coins view, despite being associated with a name\n",
                              __func__, itClaims->outPoint.hash.GetHex());
                    claim.push_back(Pair("error", "No value found for claim"));
                }
                else if (coin->vout.size() < itClaims->outPoint.n || coin->vout[itClaims->outPoint.n].IsNull())
                {
                    LogPrintf("%s: the specified txout of %s appears to have been spent\n", __func__, itClaims->outPoint.hash.GetHex());
                    claim.push_back(Pair("error", "Txout spent"));
                }
                else
                {
                    int op;
                    std::vector<std::vector<unsigned char> > vvchParams;
                    if (!DecodeClaimScript(coin->vout[itClaims->outPoint.n].scriptPubKey, op, vvchParams))
                    {
                        LogPrintf("%s: the specified txout of %s does not have an claim command\n", __func__, itClaims->outPoint.hash.GetHex());
                    }
                    std::string sValue(vvchParams[1].begin(), vvchParams[1].end());
                    claim.push_back(Pair("value", sValue));
                }
                claims.push_back(claim);
            }
            node.push_back(Pair("claims", claims));
            ret.push_back(node);
        }
    }
    return ret;
}

UniValue getclaimtrie(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            "getclaimtrie\n"
            "Return the entire name trie.\n"
            "Arguments:\n"
            "None\n"
            "Result: \n"
            "{\n"
            "  \"name\"           (string) the name of the node\n"
            "  \"hash\"           (string) the hash of the node\n"
            "  \"txid\"           (string) (if value exists) the hash of the transaction which has successfully claimed this name\n"
            "  \"n\"              (numeric) (if value exists) vout value\n"
            "  \"value\"          (numeric) (if value exists) txout value\n"
            "  \"height\"         (numeric) (if value exists) the height of the block in which this transaction is located\n"
            "}\n"
        );

    LOCK(cs_main);
    UniValue ret(UniValue::VARR);

    std::vector<namedNodeType> nodes = pclaimTrie->flattenTrie();
    for (std::vector<namedNodeType>::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        UniValue node(UniValue::VOBJ);
        node.push_back(Pair("name", it->first));                                                       
        node.push_back(Pair("hash", it->second.hash.GetHex()));
        CClaimValue claim;
        if (it->second.getBestClaim(claim))
        {
            node.push_back(Pair("txid", claim.outPoint.hash.GetHex()));                                    
            node.push_back(Pair("n", (int)claim.outPoint.n));                                             
            node.push_back(Pair("value", ValueFromAmount(claim.nAmount)));                                           
            node.push_back(Pair("height", claim.nHeight));                                          
        }
        ret.push_back(node);
    }
    return ret;
}

bool getValueForClaim(const COutPoint& out, std::string& sValue)
{
    CCoinsViewCache view(pcoinsTip);
    const CCoins* coin = view.AccessCoins(out.hash);
    if (!coin)
    {
        LogPrintf("%s: %s does not exist in the coins view, despite being associated with a name\n",
                  __func__, out.hash.GetHex());
        return true;
    }
    if (coin->vout.size() < out.n || coin->vout[out.n].IsNull())
    {
        LogPrintf("%s: the specified txout of %s appears to have been spent\n", __func__, out.hash.GetHex());
        return true;
    }
    
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    if (!DecodeClaimScript(coin->vout[out.n].scriptPubKey, op, vvchParams))
    {
        LogPrintf("%s: the specified txout of %s does not have a name claim command\n", __func__, out.hash.GetHex());
        return false;
    }
    if (op == OP_CLAIM_NAME)
    {
        sValue = std::string(vvchParams[1].begin(), vvchParams[1].end());
    }
    else if (op == OP_UPDATE_CLAIM)
    {
        sValue = std::string(vvchParams[2].begin(), vvchParams[2].end());
    }
    return true;
}


UniValue getvalueforname(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getvalueforname \"name\"\n"
            "Return the value associated with a name, if one exists\n"
            "Arguments:\n"
            "1. \"name\"             (string) the name to look up\n"
            "Result: \n"
            "\"value\"               (string) the value of the name, if it exists\n"
            "\"txid\"                (string) the hash of the transaction which successfully claimed the name\n"
            "\"n\"                   (numeric) vout value\n"
            "\"amount\"              (numeric) txout amount\n"
            "\"effective amount\"    (numeric) txout amount plus amount from all supports associated with the claim\n"
            "\"height\"              (numeric) the height of the block in which this transaction is located\n"
        );
    LOCK(cs_main);
    std::string name = params[0].get_str();
    CClaimValue claim;
    UniValue ret(UniValue::VOBJ);
    if (!pclaimTrie->getInfoForName(name, claim))
        return ret;
    std::string sValue;
    if (!getValueForClaim(claim.outPoint, sValue))
        return ret;
    ret.push_back(Pair("value", sValue));
    ret.push_back(Pair("txid", claim.outPoint.hash.GetHex()));
    ret.push_back(Pair("n", (int)claim.outPoint.n));
    ret.push_back(Pair("amount", claim.nAmount));
    ret.push_back(Pair("effective amount", claim.nEffectiveAmount)); 
    ret.push_back(Pair("height", claim.nHeight));
    return ret;
}

typedef std::pair<CClaimValue, std::vector<CSupportValue> > claimAndSupportsType;
typedef std::map<uint160, claimAndSupportsType> claimSupportMapType;
typedef std::map<uint160, std::vector<CSupportValue> > supportsWithoutClaimsMapType;

UniValue claimsAndSupportsToJSON(claimSupportMapType::const_iterator itClaimsAndSupports, int nCurrentHeight)
{
    UniValue ret(UniValue::VOBJ);
    const CClaimValue claim = itClaimsAndSupports->second.first;
    const std::vector<CSupportValue> supports = itClaimsAndSupports->second.second;
    CAmount nEffectiveAmount = 0;
    UniValue supportObjs(UniValue::VARR);
    for (std::vector<CSupportValue>::const_iterator itSupports = supports.begin(); itSupports != supports.end(); ++itSupports)
    {
        UniValue supportObj(UniValue::VOBJ);
        supportObj.push_back(Pair("txid", itSupports->outPoint.hash.GetHex()));
        supportObj.push_back(Pair("n", (int)itSupports->outPoint.n));
        supportObj.push_back(Pair("nHeight", itSupports->nHeight));
        supportObj.push_back(Pair("nValidAtHeight", itSupports->nValidAtHeight));
        if (itSupports->nValidAtHeight < nCurrentHeight)
        {
            nEffectiveAmount += itSupports->nAmount;
        }
        supportObj.push_back(Pair("nAmount", itSupports->nAmount));
        supportObjs.push_back(supportObj);
    }
    ret.push_back(Pair("claimId", itClaimsAndSupports->first.GetHex()));
    ret.push_back(Pair("txid", claim.outPoint.hash.GetHex()));
    ret.push_back(Pair("n", (int)claim.outPoint.n));
    ret.push_back(Pair("nHeight", claim.nHeight));
    ret.push_back(Pair("nValidAtHeight", claim.nValidAtHeight));
    if (claim.nValidAtHeight < nCurrentHeight)
    {
        nEffectiveAmount += claim.nAmount;
    }
    ret.push_back(Pair("nAmount", claim.nAmount));
    std::string sValue;
    if (getValueForClaim(claim.outPoint, sValue))
    {
        ret.push_back(Pair("value", sValue));
    }
    ret.push_back(Pair("nEffectiveAmount", nEffectiveAmount));
    ret.push_back(Pair("supports", supportObjs));

    return ret;
}

UniValue supportsWithoutClaimsToJSON(supportsWithoutClaimsMapType::const_iterator itSupportsWithoutClaims, int nCurrentHeight)
{
    const std::vector<CSupportValue> supports = itSupportsWithoutClaims->second;
    UniValue ret(UniValue::VOBJ);
    UniValue supportObjs(UniValue::VARR);
    ret.push_back(Pair("claimId (no matching claim)", itSupportsWithoutClaims->first.GetHex()));
    for (std::vector<CSupportValue>::const_iterator itSupports = supports.begin(); itSupports != supports.end(); ++itSupports)
    {
        UniValue supportObj(UniValue::VOBJ);
        supportObj.push_back(Pair("txid", itSupports->outPoint.hash.GetHex()));
        supportObj.push_back(Pair("n", (int)itSupports->outPoint.n));
        supportObj.push_back(Pair("nHeight", itSupports->nHeight));
        supportObj.push_back(Pair("nValidAtHeight", itSupports->nValidAtHeight));
        supportObj.push_back(Pair("nAmount", itSupports->nAmount));
        supportObjs.push_back(supportObj);
    }
    ret.push_back(Pair("supports", supportObjs));
    return ret;
}

UniValue getclaimsforname(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getclaimsforname\n"
            "Return all claims and supports for a name\n"
            "Arguments: \n"
            "1.  \"name\"       (string) the name for which to get claims and supports\n"
            "Result:\n"
            "{\n"
            "  \"nLastTakeoverheight\" (numeric) the last height at which ownership of the name changed\n"
            "  \"claims\": [    (array of object) claims for this name\n"
            "    {\n"
            "      \"claimId\"    (string) the claimId of this claim\n"
            "      \"txid\"       (string) the txid of this claim\n"
            "      \"n\"          (numeric) the index of the claim in the transaction's list of outputs\n"
            "      \"nHeight\"    (numeric) the height at which the claim was included in the blockchain\n"
            "      \"nValidAtHeight\" (numeric) the height at which the claim became/becomes valid\n"
            "      \"nAmount\"    (numeric) the amount of the claim\n"
            "      \"nEffectiveAmount\" (numeric) the total effective amount of the claim, taking into effect whether the claim or support has reached its nValidAtHeight\n"
            "      \"supports\" : [ (array of object) supports for this claim\n"
            "        \"txid\"     (string) the txid of the support\n"
            "        \"n\"        (numeric) the index of the support in the transaction's list of outputs\n"
            "        \"nHeight\"  (numeric) the height at which the support was included in the blockchain\n"
            "        \"nValidAtHeight\" (numeric) the height at which the support became/becomes valid\n"
            "        \"nAmount\"  (numeric) the amount of the support\n"
            "      ]\n"
            "    }\n"
            "  ],\n"
            "  \"unmatched supports\": [ (array of object) supports that did not match a claim for this name\n"
            "    {\n"
            "      \"txid\"     (string) the txid of the support\n"
            "      \"n\"        (numeric) the index of the support in the transaction's list of outputs\n"
            "      \"nHeight\"  (numeric) the height at which the support was included in the blockchain\n"
            "      \"nValidAtHeight\" (numeric) the height at which the support became/becomes valid\n"
            "      \"nAmount\"  (numeric) the amount of the support\n"
            "    }\n"
            "  ]\n"
            "}\n"   
        );

    LOCK(cs_main);
    std::string name = params[0].get_str();
    claimsForNameType claimsForName = pclaimTrie->getClaimsForName(name);
    int nCurrentHeight = chainActive.Height();

    claimSupportMapType claimSupportMap;
    supportsWithoutClaimsMapType supportsWithoutClaims;
    for (std::vector<CClaimValue>::const_iterator itClaims = claimsForName.claims.begin(); itClaims != claimsForName.claims.end(); ++itClaims)
    {
        claimAndSupportsType claimAndSupports = std::make_pair(*itClaims, std::vector<CSupportValue>());
        claimSupportMap.insert(std::pair<uint160, claimAndSupportsType>(itClaims->claimId, claimAndSupports));
    }
    for (std::vector<CSupportValue>::const_iterator itSupports = claimsForName.supports.begin(); itSupports != claimsForName.supports.end(); ++itSupports)
    {
        claimSupportMapType::iterator itClaimAndSupports = claimSupportMap.find(itSupports->supportedClaimId);
        if (itClaimAndSupports == claimSupportMap.end())
        {
            std::pair<supportsWithoutClaimsMapType::iterator, bool> ret = supportsWithoutClaims.insert(std::pair<uint160, std::vector<CSupportValue> >(itSupports->supportedClaimId, std::vector<CSupportValue>()));
            ret.first->second.push_back(*itSupports);
        }
        else
        {
            itClaimAndSupports->second.second.push_back(*itSupports);
        }
    }
    UniValue ret(UniValue::VOBJ);
    UniValue claimObjs(UniValue::VARR);
    ret.push_back(Pair("nLastTakeoverHeight", claimsForName.nLastTakeoverHeight));
    for (claimSupportMapType::const_iterator itClaimsAndSupports = claimSupportMap.begin(); itClaimsAndSupports != claimSupportMap.end(); ++itClaimsAndSupports)
    {
        UniValue claimAndSupportsObj = claimsAndSupportsToJSON(itClaimsAndSupports, nCurrentHeight);
        claimObjs.push_back(claimAndSupportsObj);
    }
    ret.push_back(Pair("claims", claimObjs));
    UniValue unmatchedSupports(UniValue::VARR);
    for (supportsWithoutClaimsMapType::const_iterator itSupportsWithoutClaims = supportsWithoutClaims.begin(); itSupportsWithoutClaims != supportsWithoutClaims.end(); ++itSupportsWithoutClaims)
    {
        UniValue supportsObj = supportsWithoutClaimsToJSON(itSupportsWithoutClaims, nCurrentHeight);
        unmatchedSupports.push_back(supportsObj);
    }
    ret.push_back(Pair("supports without claims", unmatchedSupports));
    return ret;
}


UniValue gettotalclaimednames(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
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

UniValue gettotalclaims(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
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

UniValue gettotalvalueofclaims(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
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
    if (params.size() == 1)
        controlling_only = params[0].get_bool();
    CAmount total_amount = pclaimTrie->getTotalValueOfClaimsInTrie(controlling_only);
    return ValueFromAmount(total_amount);
}

UniValue getclaimsfortx(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "getclaimsfortx\n"
            "Return any claims or supports found in a transaction\n"
            "Arguments:\n"
            "1.  \"txid\"         (string) the txid of the transaction to check for unspent claims\n"
            "Result:\n"
            "[\n"
            "  {\n"
            "    \"nOut\"                   (numeric) the index of the claim or support in the transaction's list out outputs\n"
            "    \"claim type\"             (string) 'claim' or 'support'\n"
            "    \"name\"                   (string) the name claimed or supported\n"
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

    uint256 hash;
    hash.SetHex(params[0].get_str());

    UniValue ret(UniValue::VARR);

    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    
    CCoinsViewCache view(pcoinsTip);
    const CCoins* coin = view.AccessCoins(hash);
    std::vector<CTxOut> txouts;
    int nHeight = 0;
    if (!coin)
    {
        CTransaction tx;
        if (!mempool.lookup(hash, tx))
        {
            return NullUniValue;
        }
        else
        {
            txouts = tx.vout;
        }
    }
    else
    {
        txouts = coin->vout;
        nHeight = coin->nHeight;
    }

    for (unsigned int i = 0; i < txouts.size(); ++i)
    {
        if (!txouts[i].IsNull())
        {
            vvchParams.clear();
            const CTxOut& txout = txouts[i];
            UniValue o(UniValue::VOBJ);
            if (DecodeClaimScript(txout.scriptPubKey, op, vvchParams))
            {
                o.push_back(Pair("nOut", (int64_t)i));
                std::string sName(vvchParams[0].begin(), vvchParams[0].end());
                o.push_back(Pair("name", sName));
                if (op == OP_CLAIM_NAME)
                {
                    std::string sValue(vvchParams[1].begin(), vvchParams[1].end());
                    uint160 claimId = ClaimIdHash(hash, i);
                    o.push_back(Pair("claimId", claimId.GetHex()));
                    o.push_back(Pair("value", sValue));
                }
                else if (op == OP_UPDATE_CLAIM)
                {
                    uint160 claimId(vvchParams[1]);
                    std::string sValue(vvchParams[2].begin(), vvchParams[2].end());
                    o.push_back(Pair("claimId", claimId.GetHex()));
                    o.push_back(Pair("value", sValue));
                }
                else if (op == OP_SUPPORT_CLAIM)
                {
                    uint160 supportedClaimId(vvchParams[1]);
                    o.push_back(Pair("supported claimId", supportedClaimId.GetHex()));
                }
                if (nHeight > 0)
                {
                    o.push_back(Pair("depth", chainActive.Height() - nHeight));
                    if (op == OP_CLAIM_NAME || op == OP_UPDATE_CLAIM)
                    {
                        bool inClaimTrie = pclaimTrie->haveClaim(sName, COutPoint(hash, i));
                        o.push_back(Pair("in claim trie", inClaimTrie));
                        if (inClaimTrie)
                        {
                            CClaimValue claim;
                            if (!pclaimTrie->getInfoForName(sName, claim))
                            {
                                LogPrintf("HaveClaim was true but getInfoForName returned false.");
                            }
                            o.push_back(Pair("is controlling", (claim.outPoint.hash == hash && claim.outPoint.n == i)));
                        }
                        else
                        {
                            int nValidAtHeight;
                            if (pclaimTrie->haveClaimInQueue(sName, COutPoint(hash, i), nValidAtHeight))
                            {
                                o.push_back(Pair("in queue", true));
                                o.push_back(Pair("blocks to valid", nValidAtHeight - chainActive.Height()));
                            }
                            else
                            {
                                o.push_back(Pair("in queue", false));
                            }
                        }
                    }
                    else if (op == OP_SUPPORT_CLAIM)
                    {
                        bool inSupportMap = pclaimTrie->haveSupport(sName, COutPoint(hash, i));
                        o.push_back(Pair("in support map", inSupportMap));
                        if (!inSupportMap)
                        {
                            int nValidAtHeight;
                            if (pclaimTrie->haveSupportInQueue(sName, COutPoint(hash, i), nValidAtHeight))
                            {
                                o.push_back(Pair("in queue", true));
                                o.push_back(Pair("blocks to valid", nValidAtHeight - chainActive.Height()));
                            }
                            else
                            {
                                o.push_back(Pair("in queue", false));
                            }
                        }
                    }
                }
                else
                {
                    o.push_back(Pair("depth", 0));
                    if (op == OP_CLAIM_NAME || op == OP_UPDATE_CLAIM)
                    {
                        o.push_back(Pair("in claim trie", false));
                    }
                    else if (op == OP_SUPPORT_CLAIM)
                    {
                        o.push_back(Pair("in support map", false));
                    }
                    o.push_back(Pair("in queue", false));
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
            child.push_back(Pair("character", itChildren->first));
            if (!itChildren->second.IsNull())
            {
                child.push_back(Pair("nodeHash", itChildren->second.GetHex()));
            }
            children.push_back(child);
        }
        node.push_back(Pair("children", children));
        if (itNode->hasValue && !itNode->valHash.IsNull())
        {
            node.push_back(Pair("valueHash", itNode->valHash.GetHex()));
        }
        nodes.push_back(node);
    }
    result.push_back(Pair("nodes", nodes));
    if (proof.hasValue)
    {
        result.push_back(Pair("txhash", proof.outPoint.hash.GetHex()));
        result.push_back(Pair("nOut", (int)proof.outPoint.n));
        result.push_back(Pair("last takeover height", (int)proof.nHeightOfLastTakeover));
    }
    return result;
}

UniValue getnameproof(const UniValue& params, bool fHelp)
{
    if (fHelp || (params.size() != 1 && params.size() != 2))
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
    std::string strName = params[0].get_str();
    uint256 blockHash;
    if (params.size() == 2)
    {
        std::string strBlockHash = params[1].get_str();
        blockHash = uint256S(strBlockHash);
    }
    else
    {
        blockHash = chainActive.Tip()->GetBlockHash();
    }

    if (mapBlockIndex.count(blockHash) == 0)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");

    CBlockIndex* pblockIndex = mapBlockIndex[blockHash];
    if (!chainActive.Contains(pblockIndex))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not in main chain");

    if (chainActive.Tip()->nHeight > (pblockIndex->nHeight + MAX_RPC_BLOCK_DECREMENTS))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block too deep to generate proof");

    CClaimTrieProof proof;
    if (!GetProofForName(pblockIndex, strName, proof))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate proof");

    return proofToJSON(proof);
}

static const CRPCCommand commands[] =
{ //  category              name                           actor (function)        okSafeMode
  //  --------------------- ------------------------     -----------------------  ----------
    { "Claimtrie",             "getclaimsintrie",         &getclaimsintrie,         true  },
    { "Claimtrie",             "getclaimtrie",            &getclaimtrie,            true  },
    { "Claimtrie",             "getvalueforname",         &getvalueforname,         true  },
    { "Claimtrie",             "getclaimsforname",        &getclaimsforname,        true  },
    { "Claimtrie",             "gettotalclaimednames",    &gettotalclaimednames,    true  },
    { "Claimtrie",             "gettotalclaims",          &gettotalclaims,          true  },
    { "Claimtrie",             "gettotalvalueofclaims",   &gettotalvalueofclaims,   true  },
    { "Claimtrie",             "getclaimsfortx",          &getclaimsfortx,          true  },
    { "Claimtrie",             "getnameproof",            &getnameproof,            true  },
};

void RegisterClaimTrieRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
