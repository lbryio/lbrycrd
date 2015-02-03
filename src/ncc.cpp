#include "ncc.h"
#include "util.h"

bool DecodeNCCScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams)
{
    CScript::const_iterator pc = scriptIn.begin();
    return DecodeNCCScript(scriptIn, op, vvchParams, pc);
}

bool DecodeNCCScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc)
{
    opcodetype opcode;
    if (!scriptIn.GetOp(pc, opcode))
    {
        return false;
    }
    if (opcode != OP_CLAIM_NAME)
    {
        return false;
    }

    op = opcode;

    std::vector<unsigned char> vchName;
    std::vector<unsigned char> vchValue;

    // The correct format is:
    // OP_CLAIM_NAME vchName vchValue OP_DROP2 OP_DROP pubkeyscript
    // All others are invalid.

    if (!scriptIn.GetOp(pc, opcode, vchName) || opcode < 0 || opcode > OP_PUSHDATA4)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode, vchValue) || opcode < 0 || opcode > OP_PUSHDATA4)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode) || opcode != OP_2DROP)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode) || opcode != OP_DROP)
    {
        return false;
    }

    vvchParams.push_back(vchName);
    vvchParams.push_back(vchValue);

    return true;
}

CScript StripNCCScriptPrefix(const CScript& scriptIn)
{
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeNCCScript(scriptIn, op, vvchParams, pc))
    {
        return scriptIn;
    }

    return CScript(pc, scriptIn.end());
}

