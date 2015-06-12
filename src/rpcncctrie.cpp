#include "main.h"
#include "ncc.h"
#include "rpcserver.h"
#include "univalue/univalue.h"

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

UniValue gettxinfoforname(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "gettxinfoforname \"name\"\n"
            "Return information about the transaction that has successfully claimed a name, if one exists\n"
            "Arguments:\n"
            "1. \"name\"          (string) the name about which to return info\n"
            "Result: \n"
            "\"txid\"             (string) the hash of the transaction which successfully claimed the name\n"
            "\"n\"                (numeric) vout value\n"
            "\"amount\"           (numeric) txout amount\n"
            "\"height\"           (numeric) the height of the block in which this transaction is located\n"
        ); 
    LOCK(cs_main);

    std::string name = params[0].get_str();
    
    UniValue ret(UniValue::VOBJ);
    CNodeValue val;
    if (pnccTrie->getInfoForName(name, val))
    {
        ret.push_back(Pair("txid", val.txhash.GetHex()));
        ret.push_back(Pair("n", (int)val.nOut));
        ret.push_back(Pair("amount", val.nAmount));
        ret.push_back(Pair("height", val.nHeight));
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

