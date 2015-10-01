#include "nameclaim.h"
#include "util.h"

std::vector<unsigned char> uint32_t_to_vch(uint32_t n)
{
    std::vector<unsigned char> vchN;
    vchN.resize(4);
    vchN[0] = n >> 24;
    vchN[1] = n >> 16;
    vchN[2] = n >> 8;
    vchN[3] = n;
    return vchN;
}

uint32_t vch_to_uint32_t(std::vector<unsigned char>& vchN)
{
    uint32_t n;
    if (vchN.size() != 4)
    {
        LogPrintf("%s() : a vector<unsigned char> with size other than 4 has been given", __func__);
        return 0;
    }
    n = vchN[0] << 24 | vchN[1] << 16 | vchN[2] << 8 | vchN[3];
    return n;
}

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
        if (!scriptIn.GetOp(pc, opcode, vchParam3) || opcode < 0 || opcode > OP_PUSHDATA4)
        {
            return false;
        }
        if (vchParam3.size() != 4)
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
        if (vchParam2.size() != (256/8))
            return false;
    }
    
    return true;
}

CScript StripClaimScriptPrefix(const CScript& scriptIn)
{
    int op;
    return StripClaimScriptPrefix(scriptIn, op);
}

CScript StripClaimScriptPrefix(const CScript& scriptIn, int& op)
{
    std::vector<std::vector<unsigned char> > vvchParams;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeClaimScript(scriptIn, op, vvchParams, pc))
    {
        return scriptIn;
    }

    return CScript(pc, scriptIn.end());
}

