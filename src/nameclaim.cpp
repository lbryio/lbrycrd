#include "nameclaim.h"
#include "util.h"

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams)
{
    CScript::const_iterator pc = scriptIn.begin();
    return DecodeClaimScript(scriptIn, op, vvchParams, pc);
}

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc)
{
    opcodetype opcode;
    if (!scriptIn.GetOp(pc, opcode))
    {
        return false;
    }
    
    if (opcode != OP_CLAIM_NAME && opcode != OP_SUPPORT_CLAIM)
    {
        return false;
    }

    op = opcode;

    std::vector<unsigned char> vchParam1;
    std::vector<unsigned char> vchParam2;
    std::vector<unsigned char> vchParam3;
    // Valid formats:
    // OP_CLAIM_NAME vchName vchValue OP_2DROP OP_DROP pubkeyscript
    // OP_SUPPORT_CLAIM vchName vchClaimHash vchClaimIndex OP_2DROP OP_2DROP pubkeyscript
    // All others are invalid.

    
    if (!scriptIn.GetOp(pc, opcode, vchParam1) || opcode < 0 || opcode > OP_PUSHDATA4)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode, vchParam2) || opcode < 0 || opcode > OP_PUSHDATA4)
    {
        return false;
    }
    if (op == OP_SUPPORT_CLAIM)
    {
        if (!scriptIn.GetOp(pc, opcode) || opcode < 0 || opcode > OP_PUSHDATA4)
        {
            return false;
        }
    }
    if (!scriptIn.GetOp(pc, opcode) || opcode != OP_2DROP)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode) || (op == OP_CLAIM_NAME && opcode != OP_DROP) || (op == OP_SUPPORT_CLAIM && opcode != OP_2DROP))
    {
        return false;
    }

    vvchParams.push_back(vchParam1);
    vvchParams.push_back(vchParam2);
    if (op == OP_SUPPORT_CLAIM)
    {
        vvchParams.push_back(vchParam3);
    }
    
    return true;
}

CScript StripClaimScriptPrefix(const CScript& scriptIn)
{
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeClaimScript(scriptIn, op, vvchParams, pc))
    {
        return scriptIn;
    }

    return CScript(pc, scriptIn.end());
}

