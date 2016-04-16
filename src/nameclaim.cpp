#include "nameclaim.h"
#include "hash.h"
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
    
    if (opcode != OP_CLAIM_NAME && opcode != OP_SUPPORT_CLAIM && opcode != OP_UPDATE_CLAIM)
    {
        return false;
    }

    op = opcode;

    std::vector<unsigned char> vchParam1;
    std::vector<unsigned char> vchParam2;
    std::vector<unsigned char> vchParam3;
    // Valid formats:
    // OP_CLAIM_NAME vchName vchValue OP_2DROP OP_DROP pubkeyscript
    // OP_UPDATE_CLAIM vchName vchClaimId vchValue OP_2DROP OP_2DROP pubkeyscript
    // OP_SUPPORT_CLAIM vchName vchClaimId OP_2DROP OP_DROP pubkeyscript
    // All others are invalid.

    if (!scriptIn.GetOp(pc, opcode, vchParam1) || opcode < 0 || opcode > OP_PUSHDATA4)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode, vchParam2) || opcode < 0 || opcode > OP_PUSHDATA4)
    {
        return false;
    }
    if (op == OP_UPDATE_CLAIM || op == OP_SUPPORT_CLAIM)
    {
        if (vchParam2.size() != 160/8)
        {
            return false;
        }
    }
    if (op == OP_UPDATE_CLAIM)
    {
        if (!scriptIn.GetOp(pc, opcode, vchParam3) || opcode < 0 || opcode > OP_PUSHDATA4)
        {
            return false;
        }
    }
    if (!scriptIn.GetOp(pc, opcode) || opcode != OP_2DROP)
    {
        return false;
    }
    if (!scriptIn.GetOp(pc, opcode))
    {
        return false;
    }
    if ((op == OP_CLAIM_NAME || op == OP_SUPPORT_CLAIM) && opcode != OP_DROP)
    {
        return false;
    }
    else if ((op == OP_UPDATE_CLAIM) && opcode != OP_2DROP)
    {
        return false;
    }

    vvchParams.push_back(vchParam1);
    vvchParams.push_back(vchParam2);
    if (op == OP_UPDATE_CLAIM)
    {
        vvchParams.push_back(vchParam3);
    }
    return true;
}

uint160 ClaimIdHash(const uint256& txhash, uint32_t nOut)
{
    std::vector<unsigned char> claimToHash(txhash.begin(), txhash.end());
    std::vector<unsigned char> vchnOut = uint32_t_to_vch(nOut);
    claimToHash.insert(claimToHash.end(), vchnOut.begin(), vchnOut.end());
    return Hash160(claimToHash);
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

size_t ClaimScriptSize(const CScript& scriptIn)
{
    CScript strippedScript = StripClaimScriptPrefix(scriptIn);
    return scriptIn.size() - strippedScript.size();
}

