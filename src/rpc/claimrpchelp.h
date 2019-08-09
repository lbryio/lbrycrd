
#include <string>

// always keep defines T_ + value in upper case
#define T_NORMALIZEDNAME                "normalizedName"
#define T_BLOCKHASH                     "blockhash"
#define T_CLAIMS                        "claims"
#define T_CLAIMID                       "claimId"
#define T_TXID                          "txId"
#define T_N                             "n"
#define T_AMOUNT                        "amount"
#define T_HEIGHT                        "height"
#define T_VALUE                         "value"
#define T_NAME                          "name"
#define T_VALIDATHEIGHT                 "validAtHeight"
#define T_NAMES                         "names"
#define T_EFFECTIVEAMOUNT               "effectiveAmount"
#define T_LASTTAKEOVERHEIGHT            "lastTakeoverHeight"
#define T_SUPPORTS                      "supports"
#define T_SUPPORTSWITHOUTCLAIM          "supportsWithoutClaim"
#define T_TOTALNAMES                    "totalNames"
#define T_TOTALCLAIMS                   "totalClaims"
#define T_TOTALVALUE                    "totalValue"
#define T_CONTROLLINGONLY               "controllingOnly"
#define T_CLAIMTYPE                     "claimType"
#define T_DEPTH                         "depth"
#define T_INCLAIMTRIE                   "inClaimTrie"
#define T_ISCONTROLLING                 "isControlling"
#define T_INSUPPORTMAP                  "inSupportMap"
#define T_INQUEUE                       "inQueue"
#define T_BLOCKSTOVALID                 "blocksToValid"
#define T_NODES                         "nodes"
#define T_CHILDREN                      "children"
#define T_CHARACTER                     "character"
#define T_NODEHASH                      "nodeHash"
#define T_VALUEHASH                     "valueHash"
#define T_PAIRS                         "pairs"
#define T_ODD                           "odd"
#define T_HASH                          "hash"

enum {
    GETCLAIMSINTRIE = 0,
    GETNAMESINTRIE,
    GETVALUEFORNAME,
    GETCLAIMSFORNAME,
    GETCLAIMBYID,
    GETTOTALCLAIMEDNAMES,
    GETTOTALCLAIMS,
    GETTOTALVALUEOFCLAIMS,
    GETCLAIMSFORTX,
    GETNAMEPROOF,
    CHECKNORMALIZATION,
};

static const char* const rpc_help[] = {

// GETCLAIMSINTRIE
R"(getclaimsintrie
Return all claims in the name trie. Deprecated
Arguments:
1. ")" T_BLOCKHASH R"("               (string, optional) get claims in the trie
                                                  at the block specified
                                                  by this block hash.
                                                  If none is given,
                                                  the latest active
                                                  block will be used.
Result: [
    ")" T_NORMALIZEDNAME R"("         (string) the name of the claim(s) (after normalization)
    ")" T_CLAIMS R"(": [              (array of object) the claims for this name
        ")" T_NAME R"("               (string) the original name of this claim (before normalization)
        ")" T_VALUE R"("              (string) the value of this claim
        ")" T_CLAIMID R"("            (string) the claimId of the claim
        ")" T_TXID R"("               (string) the txid of the claim
        ")" T_N R"("                  (numeric) the index of the claim in the transaction's list of outputs
        ")" T_HEIGHT R"("             (numeric) the height of the block in which this transaction is located
        ")" T_VALIDATHEIGHT R"("      (numeric) the height at which the claim became/becomes valid
        ")" T_AMOUNT R"("             (numeric) the amount of the claim
    ]
])",

// GETNAMESINTRIE
R"(getnamesintrie
Return all claim names in the trie.
Arguments:
1. ")" T_BLOCKHASH R"("               (string, optional) get claims in the trie
                                                  at the block specified
                                                  by this block hash.
                                                  If none is given,
                                                  the latest active
                                                  block will be used.
Result: [
    ")" T_NAMES R"("                  all names in the trie that have claims
])",

// GETVALUEFORNAME
R"(getvalueforname
Return the winning or specified by claimId value associated with a name
Arguments:
1. ")" T_NAME R"("                    (string) the name to look up
2. ")" T_BLOCKHASH R"("               (string, optional) get the value
                                                associated with the name
                                                at the block specified
                                                by this block hash.
                                                If none is given,
                                                the latest active
                                                block will be used.
3. ")" T_CLAIMID R"("                 (string, optional) can be partial one
Result: [
    ")" T_NORMALIZEDNAME R"("         (string) the name of the claim(s) (after normalization)
    ")" T_NAME R"("                   (string) the original name of this claim (before normalization)
    ")" T_VALUE R"("                  (string) the value of this claim
    ")" T_CLAIMID R"("                (string) the claimId of the claim
    ")" T_TXID R"("                   (string) the txid of the claim
    ")" T_N R"("                      (numeric) the index of the claim in the transaction's list of outputs
    ")" T_HEIGHT R"("                 (numeric) the height of the block in which this transaction is located
    ")" T_VALIDATHEIGHT R"("          (numeric) the height at which the support became/becomes valid
    ")" T_AMOUNT R"("                 (numeric) the amount of the claim
    ")" T_EFFECTIVEAMOUNT R"("        (numeric) the amount plus amount from all supports associated with the claim
    ")" T_SUPPORTS R"(": [            (array of object) supports for this claim
        ")" T_VALUE R"("              (string) the metadata of the support if any
        ")" T_TXID R"("               (string) the txid of the support
        ")" T_N R"("                  (numeric) the index of the support in the transaction's list of outputs
        ")" T_HEIGHT R"("             (numeric) the height of the block in which this transaction is located
        ")" T_VALIDATHEIGHT R"("      (numeric) the height at which the support became/becomes valid
        ")" T_AMOUNT R"("             (numeric) the amount of the support
    ]
    ")" T_LASTTAKEOVERHEIGHT R"("     (numeric) the last height at which ownership of the name changed
])",

// GETCLAIMSFORNAME
R"(getclaimsforname
Return all claims and supports for a name
Arguments:
1. ")" T_NAME R"("                        (string) the name to look up
2. ")" T_BLOCKHASH R"("                   (string, optional) get claims in the trie
                                                    at the block specified
                                                    by this block hash.
                                                    If none is given,
                                                    the latest active
                                                    block will be used.
Result: [
    ")" T_NORMALIZEDNAME R"("             (string) the name of the claim(s) (after normalization)
    ")" T_CLAIMS R"(": [                  (array of object) the claims for this name
        ")" T_NAME R"("                   (string) the original name of this claim (before normalization)
        ")" T_VALUE R"("                  (string) the value of this claim
        ")" T_CLAIMID R"("                (string) the claimId of the claim
        ")" T_TXID R"("                   (string) the txid of the claim
        ")" T_N R"("                      (numeric) the index of the claim in the transaction's list of outputs
        ")" T_HEIGHT R"("                 (numeric) the height of the block in which this transaction is located
        ")" T_VALIDATHEIGHT R"("          (numeric) the height at which the claim became/becomes valid
        ")" T_AMOUNT R"("                 (numeric) the amount of the claim
        ")" T_EFFECTIVEAMOUNT R"("        (numeric) the amount plus amount from all supports associated with the claim
        ")" T_SUPPORTS R"(": [            (array of object) supports for this claim
            ")" T_VALUE R"("              (string) the metadata of the support if any
            ")" T_TXID R"("               (string) the txid of the support
            ")" T_N R"("                  (numeric) the index of the support in the transaction's list of outputs
            ")" T_HEIGHT R"("             (numeric) the height of the block in which this transaction is located
            ")" T_VALIDATHEIGHT R"("      (numeric) the height at which the support became/becomes valid
            ")" T_AMOUNT R"("             (numeric) the amount of the support
        ]
    ]
    ")" T_LASTTAKEOVERHEIGHT R"("         (numeric) the last height at which ownership of the name changed
    ")" T_SUPPORTSWITHOUTCLAIM R"(": [
        ")" T_TXID R"("                   (string) the txid of the support
        ")" T_N R"("                      (numeric) the index of the support in the transaction's list of outputs
        ")" T_HEIGHT R"("                 (numeric) the height of the block in which this transaction is located
        ")" T_VALIDATHEIGHT R"("          (numeric) the height at which the support became/becomes valid
        ")" T_AMOUNT R"("                 (numeric) the amount of the support
    ]
])",

// GETCLAIMBYID
R"(getclaimbyid
Get a claim by claim id
Arguments:
1. ")" T_CLAIMID R"("                 (string) the claimId of this claim or patial id (at least 3 chars)
Result: [
    ")" T_NORMALIZEDNAME R"("         (string) the name of the claim(s) (after normalization)
    ")" T_NAME R"("                   (string) the original name of this claim (before normalization)
    ")" T_VALUE R"("                  (string) the value of this claim
    ")" T_CLAIMID R"("                (string) the claimId of the claim
    ")" T_TXID R"("                   (string) the txid of the claim
    ")" T_N R"("                      (numeric) the index of the claim in the transaction's list of outputs
    ")" T_HEIGHT R"("                 (numeric) the height of the block in which this transaction is located
    ")" T_VALIDATHEIGHT R"("          (numeric) the height at which the support became/becomes valid
    ")" T_AMOUNT R"("                 (numeric) the amount of the claim
    ")" T_EFFECTIVEAMOUNT R"("        (numeric) the amount plus amount from all supports associated with the claim
    ")" T_SUPPORTS R"(": [            (array of object) supports for this claim
        ")" T_VALUE R"("              (string) the metadata of the support if any
        ")" T_TXID R"("               (string) the txid of the support
        ")" T_N R"("                  (numeric) the index of the support in the transaction's list of outputs
        ")" T_HEIGHT R"("             (numeric) the height of the block in which this transaction is located
        ")" T_VALIDATHEIGHT R"("      (numeric) the height at which the support became/becomes valid
        ")" T_AMOUNT R"("             (numeric) the amount of the support
    ]
    ")" T_LASTTAKEOVERHEIGHT R"("     (numeric) the last height at which ownership of the name changed
])",

// GETTOTALCLAIMEDNAMES

R"(gettotalclaimednames
Return the total number of names that have been
Arguments:
Result:
    ")" T_TOTALNAMES R"("             (numeric) the total number of names in the trie
)",

// GETTOTALCLAIMS
R"(gettotalclaims
Return the total number of active claims in the trie
Arguments:
Result:
    ")" T_TOTALCLAIMS R"("            (numeric) the total number of active claims
)",

// GETTOTALVALUEOFCLAIMS
R"(gettotalvalueofclaims
Return the total value of the claims in the trie
Arguments:
1. ")" T_CONTROLLINGONLY R"("         (boolean) only include the value of controlling claims
Result:
    ")" T_TOTALVALUE R"("             (numeric) the total value of the claims in the trie
)",

// GETCLAIMSFORTX
R"(getclaimsfortx
Return any claims or supports found in a transaction
Arguments:
1.  ")" T_TXID R"("                   (string) the txid of the transaction to check for unspent claims
Result: [
    ")" T_N R"("                      (numeric) the index of the claim in the transaction's list of outputs
    ")" T_CLAIMTYPE R"("              (string) claim or support
    ")" T_NAME R"("                   (string) the name claimed or supported
    ")" T_CLAIMID R"("                (string) if a claim, its ID
    ")" T_VALUE R"("                  (string) if a claim, its value
    ")" T_DEPTH R"("                  (numeric) the depth of the transaction in the main chain
    ")" T_INCLAIMTRIE R"("            (boolean) if a name claim, whether the claim is active, i.e. has made it into the trie
    ")" T_ISCONTROLLING R"("          (boolean) if a name claim, whether the claim is the current controlling claim for the name
    ")" T_INSUPPORTMAP R"("           (boolean) if a support, whether the support is active, i.e. has made it into the support map
    ")" T_INQUEUE R"("                (boolean) whether the claim is in a queue waiting to be inserted into the trie or support map
    ")" T_BLOCKSTOVALID R"("          (numeric) if in a queue, the number of blocks until it's inserted into the trie or support map
])",

// GETNAMEPROOF
R"(getnameproof
Return the cryptographic proof that a name maps to a value or doesn't.
Arguments:
1. ")" T_NAME R"("                    (string) the name to look up
2. ")" T_BLOCKHASH R"("               (string, optional) get the value
                                                associated with the name
                                                at the block specified
                                                by this block hash.
                                                If none is given,
                                                the latest active
                                                block will be used.
3. ")" T_CLAIMID R"("                 (string, optional, post-fork) for validating a specific claim
                                                can be partial one
Result: [
    ")" T_NODES R"(": [               (array of object, pre-fork) full nodes
                                                (i.e. those which lead to the requested name)
        ")" T_CHILDREN R"(": [        (array of object) the children of the node
            ")" T_CHARACTER R"("      (string) the character which leads from the parent
                                                to this child node
            ")" T_NODEHASH R"("       (string, if exists) the hash of the node if this is a leaf node
        ]
        ")" T_VALUEHASH R"("          (string, if exists) the hash of this node's value, if
                                                it has one. If this is the requested name this
                                                will not exist whether the node has a value or not
    ]
    ")" T_PAIRS R"(": [               (array of pairs, post-fork) hash can be validated by
                                                hashing claim from the bottom up
        ")" T_ODD R"("                (boolean) this value goes on the right of hash
        ")" T_HASH R"("               (string) the hash to be mixed in
    ]
    ")" T_TXID R"("                   (string, if exists) the txid of the claim which controls
                                                this name, if there is one.
    ")" T_N R"("                      (numeric) the index of the claim in the transaction's list of outputs
    ")" T_LASTTAKEOVERHEIGHT R"("     (numeric) the last height at which ownership of the name changed
])",

// CHECKNORMALIZATION
R"(checknormalization
Given an unnormalized name of a claim, return normalized version of it
Arguments:
1. ")" T_NAME R"("                    (string) the name to normalize
Result:
")" T_NORMALIZEDNAME R"("             (string) normalized name
)",

};
