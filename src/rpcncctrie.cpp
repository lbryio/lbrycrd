#include "main.h"

#include "json/json_spirit_value.h"

using namespace json_spirit;
//using namespace std;


Value getncctrie(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            "getncctrie\n"
            "Return the entire NCC trie.\n"
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

    Array ret = pnccTrie->dumpToJSON();

    return ret;
}

Value gettxinfoforname(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            "gettxinfoforname\n"
            "Return information about the transaction that has successfully claimed a name, if one exists\n"
            "Arguments:\n"
            "1. \"name\"          (string) the name about which to return info\n"
            "Result: \n"
            "\"txid\"             (string) the hash of the transaction which successfully claimed the name\n"
            "\"n\"                (numeric) vout value\n"
            "\"value\"            (numeric) txout value\n"
            "\"height\"           (numeric) the height of the block in which this transaction is located\n"
        ); 
    LOCK(cs_main);

    std::string name = params[0].get_str();
    
    Object ret = pnccTrie->getInfoForName(name);
    return ret;
}
