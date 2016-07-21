#ifndef BITCOIN_NAMECLAIM_H
#define BITCOIN_NAMECLAIM_H

#include "script/script.h"
#include "uint256.h"

#include <vector>

// This is the max claim script size in bytes, not including the script pubkey part of the script.
// Scripts exceeding this size are rejected in CheckTransaction in main.cpp
#define MAX_CLAIM_SCRIPT_SIZE 8192

// This is the max claim name size in bytes, for all claim trie transactions. 
// Scripts exceeding this size are rejected in CheckTransaction in main.cpp
#define MAX_CLAIM_NAME_SIZE 255

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams);
bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc);
CScript StripClaimScriptPrefix(const CScript& scriptIn);
CScript StripClaimScriptPrefix(const CScript& scriptIn, int& op);
uint160 ClaimIdHash(const uint256& txhash, uint32_t nOut);
std::vector<unsigned char> uint32_t_to_vch(uint32_t n);
uint32_t vch_to_uint32_t(std::vector<unsigned char>& vchN);
// get size of the claim script, minus the script pubkey part 
size_t ClaimScriptSize(const CScript& scriptIn);
// get size of the name in a claim script, returns 0 if scriptin is not a claimetrie transaction 
size_t ClaimNameSize(const CScript& scriptIn); 

#endif // BITCOIN_NAMECLAIM_H
