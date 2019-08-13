
#ifndef CLAIMRPCHELP_H
#define CLAIMRPCHELP_H

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
#define T_BID                           "bid"
#define T_SEQUENCE                      "sequence"
#define T_CLAIMSADDEDORUPDATED          "claimsAddedOrUpdated"
#define T_SUPPORTSADDEDORUPDATED        "supportsAddedOrUpdated"
#define T_CLAIMSREMOVED                 "claimsRemoved"
#define T_SUPPORTSREMOVED               "supportsRemoved"
#define T_ADDRESS                       "address"

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
    GETCLAIMBYBID,
    GETCLAIMBYSEQ,
    GETCLAIMPROOFBYID,
    GETCLAIMPROOFBYSEQ,
    GETCHANGESINBLOCK,
};

#define S3_(pre, name, def) pre "\"" name "\"" def "\n"
#define S3(pre, name, def) S3_(pre, name, def)
#define S1(str)  str "\n"

#define NAME_TEXT      "                    (string) the name to look up"
#define BLOCKHASH_TEXT "               (string, optional) get claims in the trie\n" \
"                                                  at the block specified\n" \
"                                                  by this block hash.\n" \
"                                                  If none is given,\n" \
"                                                  the latest active\n" \
"                                                  block will be used."

#define CLAIM_OUTPUT    \
S3("    ", T_NORMALIZEDNAME, "         (string) the name of the claim (after normalization)") \
S3("    ", T_NAME, "                   (string) the original name of this claim (before normalization)") \
S3("    ", T_VALUE, "                  (string) the value of this claim") \
S3("    ", T_ADDRESS, "                (string) the destination address of this claim") \
S3("    ", T_CLAIMID, "                (string) the claimId of the claim") \
S3("    ", T_TXID, "                   (string) the txid of the claim") \
S3("    ", T_N, "                      (numeric) the index of the claim in the transaction's list of outputs") \
S3("    ", T_HEIGHT, "                 (numeric) the height of the block in which this transaction is located") \
S3("    ", T_VALIDATHEIGHT, "          (numeric) the height at which the support became/becomes valid") \
S3("    ", T_AMOUNT, "                 (numeric) the amount of the claim") \
S3("    ", T_EFFECTIVEAMOUNT, "        (numeric) the amount plus amount from all supports associated with the claim") \
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
S3("    ", T_SEQUENCE, "               (numeric) lower value means latest in sequence, ordered by height of insertion")

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

static const char* const rpc_help[] = {

// GETCLAIMSINTRIE
S1(R"(getclaimsintrie
Return all claims in the name trie. Deprecated
Arguments:)")
S3("1. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
S3("    ", T_NORMALIZEDNAME, "         (string) the name of the claim(s) (after normalization)")
S3("    ", T_CLAIMS, ": [              (array of object) the claims for this name")
S3("        ", T_NAME, "               (string) the original name of this claim (before normalization)")
S3("        ", T_VALUE, "              (string) the value of this claim")
S3("        ", T_ADDRESS, "            (string) the destination address of this claim")
S3("        ", T_CLAIMID, "            (string) the claimId of the claim")
S3("        ", T_TXID, "               (string) the txid of the claim")
S3("        ", T_N, "                  (numeric) the index of the claim in the transaction's list of outputs")
S3("        ", T_HEIGHT, "             (numeric) the height of the block in which this transaction is located")
S3("        ", T_VALIDATHEIGHT, "      (numeric) the height at which the claim became/becomes valid")
S3("        ", T_AMOUNT, "             (numeric) the amount of the claim")
S1("    ]")
"]",

// GETNAMESINTRIE
S1(R"(getnamesintrie
Return all claim names in the trie.
Arguments:)")
S3("1. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
S3("    ", T_NAMES, "                  all names in the trie that have claims")
"]",

// GETVALUEFORNAME
S1(R"(getvalueforname
Return the winning or specified by claimId value associated with a name
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S3("3. ", T_CLAIMID, "                 (string, optional) can be partial one")
S1("Result: [")
CLAIM_OUTPUT
"]",

// GETCLAIMSFORNAME
S1(R"(getclaimsforname
Return all claims and supports for a name
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
S3("    ", T_NORMALIZEDNAME, "         (string) the name of the claim(s) (after normalization)")
S3("    ", T_CLAIMS, ": [              (array of object) the claims for this name")
S3("        ", T_NAME, "               (string) the original name of this claim (before normalization)")
S3("        ", T_VALUE, "              (string) the value of this claim")
S3("        ", T_ADDRESS, "            (string) the destination address of this claim")
S3("        ", T_CLAIMID, "            (string) the claimId of the claim")
S3("        ", T_TXID, "               (string) the txid of the claim")
S3("        ", T_N, "                  (numeric) the index of the claim in the transaction's list of outputs")
S3("        ", T_HEIGHT, "             (numeric) the height of the block in which this transaction is located")
S3("        ", T_VALIDATHEIGHT, "      (numeric) the height at which the claim became/becomes valid")
S3("        ", T_AMOUNT, "             (numeric) the amount of the claim")
S3("        ", T_EFFECTIVEAMOUNT, "    (numeric) the amount plus amount from all supports associated with the claim")
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
S3("        ", T_SEQUENCE, "           (numeric) lower value means latest in sequence, ordered by height of insertion")
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

// GETCLAIMBYID
S1(R"(getclaimbyid
Get a claim by claim id
Arguments:)")
S3("1. ", T_CLAIMID, "                 (string) the claimId of this claim or patial id (at least 3 chars)")
S1("Result: [")
CLAIM_OUTPUT
"]",

// GETTOTALCLAIMEDNAMES
S1(R"(gettotalclaimednames
Return the total number of names that have been
Arguments:)")
S1("Result:")
S3("    ", T_TOTALNAMES, "             (numeric) the total number of names in the trie")
,

// GETTOTALCLAIMS
S1(R"(gettotalclaims
Return the total number of active claims in the trie
Arguments:)")
S1("Result:")
S3("    ", T_TOTALCLAIMS, "            (numeric) the total number of active claims")
,

// GETTOTALVALUEOFCLAIMS
S1(R"(gettotalvalueofclaims
Return the total value of the claims in the trie
Arguments:)")
S3("1. ", T_CONTROLLINGONLY, "         (boolean) only include the value of controlling claims")
S1("Result:")
S3("    ", T_TOTALVALUE, "             (numeric) the total value of the claims in the trie")
,

// GETCLAIMSFORTX
S1(R"(getclaimsfortx
Return any claims or supports found in a transaction
Arguments:)")
S3("1.  ", T_TXID, "                   (string) the txid of the transaction to check for unspent claims")
S1("Result: [")
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

// GETNAMEPROOF
S1(R"(getnameproof
Return the cryptographic proof that a name maps to a value or doesn't.
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S3("3. ", T_CLAIMID, R"(                 (string, optional, post-fork) for validating a specific claim
                                                can be partial one)")
S1("Result: [")
PROOF_OUTPUT
"]",

// CHECKNORMALIZATION
S1(R"(checknormalization
Given an unnormalized name of a claim, return normalized version of it
Arguments:)")
S3("1. ", T_NAME, "                    (string) the name to normalize")
S1("Result:")
S3("    ", T_NORMALIZEDNAME,  "        (string) normalized name")
,

// GETCLAIMBYBID
S1(R"(getclaimbybid
Get a claim by bid
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_BID, "                     (numeric) bid number")
S3("3. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
CLAIM_OUTPUT
"]",

// GETCLAIMBYSEQ
S1(R"(getclaimbyseq
Get a claim by sequence
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_SEQUENCE, "                (numeric) sequence number")
S3("3. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
CLAIM_OUTPUT
"]",

// GETCLAIMPROOFBYID
S1(R"(getclaimproofbyid
Return the cryptographic proof that a name maps to a value or doesn't by a bid.
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_BID, "                     (numeric) bid number")
S3("3. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
PROOF_OUTPUT
"]",

// GETCLAIMPROOFBYSEQ
S1(R"(getclaimproofbyseq
Return the cryptographic proof that a name maps to a value or doesn't by a sequence.
Arguments:)")
S3("1. ", T_NAME, NAME_TEXT)
S3("2. ", T_SEQUENCE, "                (numeric) sequence number")
S3("3. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
PROOF_OUTPUT
"]",

// GETCHANGESINBLOCK
S1(R"(getchangesinblock
Return the list of claims added, updated, and removed in a block or doesn't."
Arguments:)")
S3("1. ", T_BLOCKHASH, BLOCKHASH_TEXT)
S1("Result: [")
S3("    ", T_CLAIMSADDEDORUPDATED, "   (array of string) claimIDs added or updated in the trie")
S3("    ", T_CLAIMSREMOVED, "          (array of string) claimIDs that were removed from the trie")
S3("    ", T_SUPPORTSADDEDORUPDATED, " (array of string) IDs of supports added or updated")
S3("    ", T_SUPPORTSREMOVED, "        (array of string) IDs that were removed from the trie")
"]",

};

#endif // CLAIMRPCHELP_H
