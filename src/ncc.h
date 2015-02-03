#ifndef BITCOIN_NCC_H
#define BITCOIN_NCC_H

#include "script/script.h"

#include <vector>

bool DecodeNCCScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams);
bool DecodeNCCScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc);
CScript StripNCCScriptPrefix(const CScript& scriptIn);

#endif // BITCOIN_NCC_H
