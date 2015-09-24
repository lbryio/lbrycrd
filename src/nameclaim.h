#ifndef BITCOIN_NAMECLAIM_H
#define BITCOIN_NAMECLAIM_H

#include "script/script.h"

#include <vector>

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams);
bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc);
CScript StripClaimScriptPrefix(const CScript& scriptIn);

#endif // BITCOIN_NAMECLAIM_H
