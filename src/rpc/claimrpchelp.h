
#ifndef CLAIMRPCHELP_H
#define CLAIMRPCHELP_H

#include <rpc/claimrpcdefs.h>
#include <rpc/util.h>

enum {
    GETNAMESINTRIE = 0,
    GETVALUEFORNAME,
    GETCLAIMSFORNAME,
    GETCLAIMBYID,
    GETTOTALCLAIMEDNAMES,
    GETTOTALCLAIMS,
    GETTOTALVALUEOFCLAIMS,
    GETCLAIMSFORTX,
    GETNAMEPROOF,
    CHECKNORMALIZATION,
    GETCLAIMBYBID,
    GETCLAIMBYSEQ,
    GETCLAIMPROOFBYBID,
    GETCLAIMPROOFBYSEQ,
    GETCHANGESINBLOCK,
};

#define S3_(pre, name, def) pre "\"" name "\"" def "\n"
#define S3(pre, name, def) S3_(pre, name, def)
#define S1(str)  str "\n"

#define NAME_TEXT      RPCArg::Type::STR, RPCArg::Optional::NO, "The name to look up"
#define BLOCKHASH_TEXT RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Get claims in the trie\n" \
"                                                  at the block specified by this block hash.\n" \
"                                                  If none is given, the latest active\n" \
"                                                  block will be used."

#define CLAIM_OUTPUT    \
S3("    ", T_NORMALIZEDNAME, "         (string) the name of the claim (after normalization)") \
S3("    ", T_NAME, "                   (string) the original name of this claim (before normalization)") \
S3("    ", T_VALUE, "                  (string) the value of this claim") \
S3("    ", T_ADDRESS, "                (string) the destination address of this claim") \
S3("    ", T_CLAIMID, "                (string) the claimId of the claim") \
S3("    ", T_TXID, "                   (string) the txid of the claim or its most recent update") \
S3("    ", T_N, "                      (numeric) the index of the claim in the transaction's list of outputs") \
S3("    ", T_HEIGHT, "                 (numeric) the height of the block in which this transaction is located") \
S3("    ", T_VALIDATHEIGHT, "          (numeric) the height at which the support became/becomes valid") \
S3("    ", T_AMOUNT, "                 (numeric) the amount of the claim or its most recent update") \
S3("    ", T_EFFECTIVEAMOUNT, "        (numeric) the amount plus amount from all supports associated with the claim") \
S3("    ", T_ORIGINALHEIGHT, "         (numeric) the block height where the claim was first created") \
S3("    ", T_PENDINGAMOUNT, "          (numeric) expected amount when claim and its supports are all valid") \
S3("    ", T_SUPPORTS, ": [            (array of object) supports for this claim") \
S3("        ", T_VALUE, "              (string) the metadata of the support if any") \
S3("        ", T_ADDRESS, "            (string) the destination address of the support") \
S3("        ", T_TXID, "               (string) the txid of the support") \
S3("        ", T_N, "                  (numeric) the index of the support in the transaction's list of outputs") \
S3("        ", T_HEIGHT, "             (numeric) the height of the block in which this transaction is located") \
S3("        ", T_VALIDATHEIGHT, "      (numeric) the height at which the support became/becomes valid") \
S3("        ", T_AMOUNT, "             (numeric) the amount of the support") \
S1("    ]") \
S3("    ", T_LASTTAKEOVERHEIGHT, "     (numeric) the last height at which ownership of the name changed") \
S3("    ", T_BID, "                    (numeric) lower value means a higher bid rate, ordered by effective amount") \
S3("    ", T_SEQUENCE, "               (numeric) lower value means an older one in sequence, ordered by height of insertion")

#define PROOF_OUTPUT \
S3("    ", T_NODES, ": [               (array of object, pre-fork) full nodes\n" \
"                                                (i.e. those which lead to the requested name)") \
S3("        ", T_CHILDREN, ": [        (array of object) the children of the node") \
S3("            ", T_CHARACTER, "      (string) the character which leads from the parent to this child node") \
S3("            ", T_NODEHASH, "       (string, if exists) the hash of the node if this is a leaf node") \
S1("        ]") \
S3("       ", T_VALUEHASH, "           (string, if exists) the hash of this node's value, if" \
"                                                it has one. If this is the requested name this\n" \
"                                                will not exist whether the node has a value or not") \
S1("    ]") \
S3("    ", T_PAIRS, ": [               (array of pairs, post-fork) hash can be validated by" \
"                                                hashing claim from the bottom up") \
S3("        ", T_ODD, "                (boolean) this value goes on the right of hash") \
S3("        ", T_HASH, "               (string) the hash to be mixed in") \
S1("    ]") \
S3("    ", T_TXID, "                   (string, if exists) the txid of the claim which controls" \
"                                                this name, if there is one.") \
S3("    ", T_N, "                      (numeric) the index of the claim in the transaction's list of outputs") \
S3("    ", T_LASTTAKEOVERHEIGHT, "     (numeric) the last height at which ownership of the name changed")

static const RPCHelpMan rpc_help[] = {

// GETNAMESINTRIE
RPCHelpMan{"getnamesintrie",
    S1("\nReturn all claim names in the trie."),
    {
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        S3("    ", T_NAMES, "                  all names in the trie that have claims")
        "]"
    },
    RPCExamples{
        HelpExampleCli("getnamesintrie", "")
        + HelpExampleRpc("getnamesintrie", "")
    },
},

// GETVALUEFORNAME
RPCHelpMan{"getvalueforname",
    S1("\nReturn the winning or specified by claimId value associated with a name."),
    {
        { T_NAME, NAME_TEXT },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
        { T_CLAIMID, RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Full or partial claim id" },
    },
    RPCResult{
        S1("[")
        CLAIM_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getvalueforname", "\"test\"")
        + HelpExampleRpc("getvalueforname", "\"test\"")
    },
},

// GETCLAIMSFORNAME
RPCHelpMan{"getclaimsforname",
    S1("\nReturn all claims and supports for a name."),
    {
        { T_NAME, NAME_TEXT },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        S3("    ", T_NORMALIZEDNAME, "         (string) the name of the claim(s) (after normalization)")
        S3("    ", T_CLAIMS, ": [              (array of object) the claims for this name")
        S3("        ", T_NAME, "               (string) the original name of this claim (before normalization)")
        S3("        ", T_VALUE, "              (string) the value of this claim")
        S3("        ", T_ADDRESS, "            (string) the destination address of this claim")
        S3("        ", T_CLAIMID, "            (string) the claimId of the claim")
        S3("        ", T_TXID, "               (string) the txid of the claim or its most recent update")
        S3("        ", T_N, "                  (numeric) the index of the claim in the transaction's list of outputs")
        S3("        ", T_HEIGHT, "             (numeric) the height of the block in which this transaction is located")
        S3("        ", T_VALIDATHEIGHT, "      (numeric) the height at which the claim became/becomes valid")
        S3("        ", T_AMOUNT, "             (numeric) the amount of the claim or its most recent update")
        S3("        ", T_EFFECTIVEAMOUNT, "    (numeric) the amount plus amount from all supports associated with the claim")
        S3("        ", T_ORIGINALHEIGHT, "     (numeric) the block height where the claim was first created") \
        S3("        ", T_PENDINGAMOUNT, "      (numeric) expected amount when claim and its support got valid")
        S3("        ", T_SUPPORTS, ": [        (array of object) supports for this claim")
        S3("            ", T_VALUE, "          (string) the metadata of the support if any")
        S3("            ", T_ADDRESS, "        (string) the destination address of the support")
        S3("            ", T_TXID, "           (string) the txid of the support")
        S3("            ", T_N, "              (numeric) the index of the support in the transaction's list of outputs")
        S3("            ", T_HEIGHT, "         (numeric) the height of the block in which this transaction is located")
        S3("            ", T_VALIDATHEIGHT, "  (numeric) the height at which the support became/becomes valid")
        S3("            ", T_AMOUNT, "         (numeric) the amount of the support")
        S1("        ]")
        S3("        ", T_BID, "                (numeric) lower value means a higher bid rate, ordered by effective amount")
        S3("        ", T_SEQUENCE, "           (numeric) lower value means an older one in sequence, ordered by height of insertion")
        S1("    ]")
        S3("    ", T_LASTTAKEOVERHEIGHT, "     (numeric) the last height at which ownership of the name changed")
        S3("    ", T_SUPPORTSWITHOUTCLAIM, ": [")
        S3("        ", T_TXID, "               (string) the txid of the support")
        S3("        ", T_N, "                  (numeric) the index of the support in the transaction's list of outputs")
        S3("        ", T_HEIGHT, "             (numeric) the height of the block in which this transaction is located")
        S3("        ", T_VALIDATHEIGHT, "      (numeric) the height at which the support became/becomes valid")
        S3("        ", T_AMOUNT, "             (numeric) the amount of the support")
        S1("    ]")
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimsforname", "\"test\"")
        + HelpExampleRpc("getclaimsforname", "\"test\"")
    },
},

// GETCLAIMBYID
RPCHelpMan{"getclaimbyid",
    S1("\nGet a claim by claim id."),
    {
        { T_CLAIMID, RPCArg::Type::STR, RPCArg::Optional::NO, "The claimId of this claim or patial id (at least 6 chars)" },
    },
    RPCResult{
        S1("[")
        CLAIM_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimbyid", "\"012345\"")
        + HelpExampleRpc("getclaimbyid", "\"012345\"")
    },
},

// GETTOTALCLAIMEDNAMES
RPCHelpMan{"gettotalclaimednames",
    S1("\nReturn the total number of names that have been."),
    {},
    RPCResult{
        S3("    ", T_TOTALNAMES, "             (numeric) the total number of names in the trie")
    },
    RPCExamples{
        HelpExampleCli("gettotalclaimednames", "")
        + HelpExampleRpc("gettotalclaimednames", "")
    },
},

// GETTOTALCLAIMS
RPCHelpMan{"gettotalclaims",
    S1("\nReturn the total number of active claims in the trie."),
    {},
    RPCResult{
        S3("    ", T_TOTALCLAIMS, "            (numeric) the total number of active claims")
    },
    RPCExamples{
        HelpExampleCli("gettotalclaims", "")
        + HelpExampleRpc("gettotalclaims", "")
    },
},

// GETTOTALVALUEOFCLAIMS
RPCHelpMan{"gettotalvalueofclaims",
    S1("\nReturn the total value of the claims in the trie."),
    {
        { T_CONTROLLINGONLY, RPCArg::Type::BOOL, /* default */ "false", "Only include the value of controlling claims" },
    },
    RPCResult{
        S3("    ", T_TOTALVALUE, "             (numeric) the total value of the claims in the trie")
    },
    RPCExamples{
        HelpExampleCli("gettotalvalueofclaims", "")
        + HelpExampleRpc("gettotalvalueofclaims", "")
    },
},

// GETCLAIMSFORTX
RPCHelpMan{"getclaimsfortx",
    S1("\nReturn any claims or supports found in a transaction."),
    {
        { T_TXID, RPCArg::Type::STR, RPCArg::Optional::NO, "The txid of the transaction to check for unspent claims" },
    },
    RPCResult{
        S1("[")
        S3("    ", T_N, "                      (numeric) the index of the claim in the transaction's list of outputs")
        S3("    ", T_CLAIMTYPE, "              (string) claim or support")
        S3("    ", T_NAME, "                   (string) the name claimed or supported")
        S3("    ", T_CLAIMID, "                (string) if a claim, its ID")
        S3("    ", T_VALUE, "                  (string) if a claim, its value")
        S3("    ", T_DEPTH, "                  (numeric) the depth of the transaction in the main chain")
        S3("    ", T_INCLAIMTRIE, "            (boolean) if a name claim, whether the claim is active, i.e. has made it into the trie")
        S3("    ", T_ISCONTROLLING, "          (boolean) if a name claim, whether the claim is the current controlling claim for the name")
        S3("    ", T_INSUPPORTMAP, "           (boolean) if a support, whether the support is active, i.e. has made it into the support map")
        S3("    ", T_INQUEUE, "                (boolean) whether the claim is in a queue waiting to be inserted into the trie or support map")
        S3("    ", T_BLOCKSTOVALID, "          (numeric) if in a queue, the number of blocks until it's inserted into the trie or support map")
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimsfortx", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        + HelpExampleRpc("getclaimsfortx", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
    },
},

// GETNAMEPROOF
RPCHelpMan{"getnameproof",
    S1("\nReturn the cryptographic proof that a name maps to a value or doesn't."),
    {
        { T_NAME, NAME_TEXT },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
        { T_TXID, RPCArg::Type::STR, RPCArg::Optional::OMITTED, "(post-fork) for validating a specific claim (partial)" },
    },
    RPCResult{
        S1("[")
        PROOF_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getnameproof", "\"test\"")
        + HelpExampleRpc("getnameproof", "\"test\"")
    },
},

// CHECKNORMALIZATION
RPCHelpMan{"checknormalization",
    S1("\nGiven an unnormalized name of a claim, return normalized version of it."),
    {
        { T_NAME, RPCArg::Type::STR, RPCArg::Optional::NO, "The name to normalize" },
    },
    RPCResult{
        S3("    ", T_NORMALIZEDNAME,  "        (string) normalized name")
    },
    RPCExamples{
        HelpExampleCli("checknormalization", "\"test\"")
        + HelpExampleRpc("checknormalization", "\"test\"")
    },
},

// GETCLAIMBYBID
RPCHelpMan{"getclaimbybid",
    S1("\nGet a claim by bid."),
    {
        { T_NAME, NAME_TEXT },
        { T_BID, RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "The bid number" },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        CLAIM_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimbybid", "\"test\" 1")
        + HelpExampleRpc("getclaimbybid", "\"test\" 1")
    },
},

// GETCLAIMBYSEQ
RPCHelpMan{"getclaimbyseq",
    S1("\nGet a claim by sequence."),
    {
        { T_NAME, NAME_TEXT },
        { T_SEQUENCE, RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "The sequence number" },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        CLAIM_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimbyseq", "\"test\" 1")
        + HelpExampleRpc("getclaimbyseq", "\"test\" 1")
    },
},

// GETCLAIMPROOFBYBID
RPCHelpMan{"getclaimproofbybid",
    S1("\nReturn the cryptographic proof that a name maps to a value or doesn't by a bid."),
    {
        { T_NAME, NAME_TEXT },
        { T_BID, RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "The bid number" },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        PROOF_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimproofbybid", "\"test\" 1")
        + HelpExampleRpc("getclaimproofbybid", "\"test\" 1")
    },
},

// GETCLAIMPROOFBYSEQ
RPCHelpMan{"getclaimproofbyseq",
    S1("\nReturn the cryptographic proof that a name maps to a value or doesn't by a sequence."),
    {
        { T_NAME, NAME_TEXT },
        { T_SEQUENCE, RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "The sequence number" },
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        PROOF_OUTPUT
        "]",
    },
    RPCExamples{
        HelpExampleCli("getclaimproofbyseq", "\"test\" 1")
        + HelpExampleRpc("getclaimproofbyseq", "\"test\" 1")
    },
},

// GETCHANGESINBLOCK
RPCHelpMan{"getchangesinblock",
    S1("\nReturn the list of claims added, updated, and removed as pulled from the queued work for that block.")
    S1("Use this method to determine which claims or supports went live on a given block."),
    {
        { T_BLOCKHASH, BLOCKHASH_TEXT },
    },
    RPCResult{
        S1("[")
        S3("    ", T_CLAIMSADDEDORUPDATED, "   (array of string) claimIDs added or updated in the trie")
        S3("    ", T_CLAIMSREMOVED, "          (array of string) claimIDs that were removed from the trie")
        S3("    ", T_SUPPORTSADDEDORUPDATED, " (array of string) IDs of supports added or updated")
        S3("    ", T_SUPPORTSREMOVED, "        (array of string) IDs that were removed from the trie")
        "]",
    },
    RPCExamples{
        HelpExampleCli("getchangesinblock", "")
        + HelpExampleRpc("getchangesinblock", "")
    },
},

};

#endif // CLAIMRPCHELP_H
