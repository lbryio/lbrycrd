#ifndef BITCOIN_NAMECLAIM_H
#define BITCOIN_NAMECLAIM_H

#include "script/script.h"

#include <vector>

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams);
bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc);
CScript StripClaimScriptPrefix(const CScript& scriptIn);
CScript StripClaimScriptPrefix(const CScript& scriptIn, int& op);
std::vector<unsigned char> uint32_t_to_vch(uint32_t n);
uint32_t vch_to_uint32_t(std::vector<unsigned char>& vchN);

#endif // BITCOIN_NAMECLAIM_H
