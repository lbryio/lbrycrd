#include "main.h"
#include "ncc.h"
#include "rpcserver.h"
#include "univalue/univalue.h"

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
    std::vector<namedNodeType> nodes = pnccTrie->flattenTrie();

    for (std::vector<namedNodeType>::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        if (!it->second.values.empty())
        {
            UniValue node(UniValue::VOBJ);
            node.push_back(Pair("name", it->first));
            UniValue claims(UniValue::VARR);
            for (std::vector<CNodeValue>::iterator itClaims = it->second.values.begin(); itClaims != it->second.values.end(); ++itClaims)
            {
                UniValue claim(UniValue::VOBJ);
                claim.push_back(Pair("txid", itClaims->txhash.GetHex()));
                claim.push_back(Pair("n", (int)itClaims->nOut));
                claim.push_back(Pair("amount", itClaims->nAmount));
                claim.push_back(Pair("height", itClaims->nHeight));
                const CCoins* coin = view.AccessCoins(itClaims->txhash);
                if (!coin)
                {
                    LogPrintf("%s: %s does not exist in the coins view, despite being associated with a name\n",
                              __func__, itClaims->txhash.GetHex());
                    claim.push_back(Pair("error", "No value found for claim"));
                }
                else if (coin->vout.size() < itClaims->nOut || coin->vout[itClaims->nOut].IsNull())
                {
                    LogPrintf("%s: the specified txout of %s appears to have been spent\n", __func__, itClaims->txhash.GetHex());
                    claim.push_back(Pair("error", "Txout spent"));
                }
                else
                {
                    int op;
                    std::vector<std::vector<unsigned char> > vvchParams;
                    if (!DecodeNCCScript(coin->vout[itClaims->nOut].scriptPubKey, op, vvchParams))
                    {
                        LogPrintf("%s: the specified txout of %s does not have an NCC command\n", __func__, itClaims->txhash.GetHex());
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

UniValue getnametrie(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            "getnametrie\n"
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

    std::vector<namedNodeType> nodes = pnccTrie->flattenTrie();
    for (std::vector<namedNodeType>::iterator it = nodes.begin(); it != nodes.end(); ++it)
    {
        UniValue node(UniValue::VOBJ);
        node.push_back(Pair("name", it->first));                                                       
        node.push_back(Pair("hash", it->second.hash.GetHex()));
        CNodeValue val;
        if (it->second.getBestValue(val))
        {
            node.push_back(Pair("txid", val.txhash.GetHex()));                                    
            node.push_back(Pair("n", (int)val.nOut));                                             
            node.push_back(Pair("value", ValueFromAmount(val.nAmount)));                                           
            node.push_back(Pair("height", val.nHeight));                                          
        }
        ret.push_back(node);
    }
    return ret;
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
            "\"height\"              (numeric) the height of the block in which this transaction is located\n"
        );
    LOCK(cs_main);
    std::string name = params[0].get_str();
    CNodeValue val;
    UniValue ret(UniValue::VOBJ);
    if (!pnccTrie->getInfoForName(name, val))
        return ret;
    CCoinsViewCache view(pcoinsTip);
    const CCoins* coin = view.AccessCoins(val.txhash);
    if (!coin)
    {
        LogPrintf("%s: %s does not exist in the coins view, despite being associated with a name\n",
                  __func__, val.txhash.GetHex());
        return ret;
    }
    if (coin->vout.size() < val.nOut || coin->vout[val.nOut].IsNull())
    {
        LogPrintf("%s: the specified txout of %s appears to have been spent\n", __func__, val.txhash.GetHex());
        return ret;
    }
    
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    if (!DecodeNCCScript(coin->vout[val.nOut].scriptPubKey, op, vvchParams))
    {
        LogPrintf("%s: the specified txout of %s does not have an NCC command\n", __func__, val.txhash.GetHex());
    }
    std::string sValue(vvchParams[1].begin(), vvchParams[1].end());
    ret.push_back(Pair("value", sValue));
    ret.push_back(Pair("txid", val.txhash.GetHex()));
    ret.push_back(Pair("n", (int)val.nOut));
    ret.push_back(Pair("amount", val.nAmount));
    ret.push_back(Pair("height", val.nHeight));
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
    if (!pnccTrie)
    {
        return -1;
    }       
    unsigned int num_names = pnccTrie->getTotalNamesInTrie();
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
    if (!pnccTrie)
    {
        return -1;
    }
    unsigned int num_claims = pnccTrie->getTotalClaimsInTrie();
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
    if (!pnccTrie)
    {
        return -1;
    }
    bool controlling_only = false;
    if (params.size() == 1)
        controlling_only = params[0].get_bool();
    CAmount total_amount = pnccTrie->getTotalValueOfClaimsInTrie(controlling_only);
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
            "    \"nOut\"                   (numeric) the index of the claim in the transaction's list out outputs\n"
            "    \"name\"                   (string) the name claimed\n"
            "    \"value\"                  (string) the value of the claim\n"
            "    \"depth\"                  (numeric) the depth of the transaction in the main chain\n"
            "    \"in claim trie\"          (boolean) whether the claim has made it into the trie\n"
            "    \"is controlling\"         (boolean) whether the claim is the current controlling claim for the name\n"
            "    \"in queue\"               (boolean) whether the claim is in a queue waiting to be inserted into the trie\n"
            "    \"blocks to valid\"        (numeric) if in a queue, the number of blocks until it's inserted into the trie\n"
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
            if (DecodeNCCScript(txout.scriptPubKey, op, vvchParams))
            {
                o.push_back(Pair("nOut", (int64_t)i));
                std::string sName(vvchParams[0].begin(), vvchParams[0].end());
                o.push_back(Pair("name", sName));
                std::string sValue(vvchParams[1].begin(), vvchParams[1].end());
                o.push_back(Pair("value", sValue));
                if (nHeight > 0)
                {
                    o.push_back(Pair("depth", chainActive.Height() - nHeight));
                    bool inClaimTrie = pnccTrie->haveClaim(sName, hash, i);
                    o.push_back(Pair("in claim trie", inClaimTrie));
                    if (inClaimTrie)
                    {
                        CNodeValue val;
                        if (!pnccTrie->getInfoForName(sName, val))
                        {
                            LogPrintf("HaveClaim was true but getInfoForName returned false.");
                        }
                        o.push_back(Pair("is controlling", (val.txhash == hash && val.nOut == i)));
                    }
                    else
                    {
                        int nValidAtHeight;
                        if (pnccTrie->haveClaimInQueue(sName, hash, i, coin->nHeight, nValidAtHeight))
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
                else
                {
                    o.push_back(Pair("depth", 0));
                    o.push_back(Pair("in claim trie", false));
                    o.push_back(Pair("in queue", false));
                }
                ret.push_back(o);
            }
        }
    }
    return ret;
}

UniValue proofToJSON(const CNCCTrieProof& proof)
{
    UniValue result(UniValue::VOBJ);
    UniValue nodes(UniValue::VARR);
    for (std::vector<CNCCTrieProofNode>::const_iterator itNode = proof.nodes.begin(); itNode != proof.nodes.end(); ++itNode)
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
        result.push_back(Pair("txhash", proof.txhash.GetHex()));
        result.push_back(Pair("nOut", (int)proof.nOut));
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

    if (chainActive.Tip()->nHeight > (pblockIndex->nHeight + 20))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Block too deep to generate proof");

    CNCCTrieProof proof;
    if (!GetProofForName(pblockIndex, strName, proof))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate proof");

    return proofToJSON(proof);
}
