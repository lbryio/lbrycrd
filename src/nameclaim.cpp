#include <boost/foreach.hpp>
#include "nameclaim.h"
#include "hash.h"
#include "util.h"
#include "claimtrie.h"


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
    static const size_t uint32Size = sizeof(uint32_t);
    if (vchN.size() != uint32Size) {
        LogPrintf("%s() : a vector<unsigned char> with size other than 4 has been given", __func__);
        return 0;
    }
    n = vchN[0] << 24 | vchN[1] << 16 | vchN[2] << 8 | vchN[3];
    return n;
}

CScript ClaimNameScript(std::string name, std::string value, bool fakeSuffix)
{
    std::vector<unsigned char> vchName(name.begin(), name.end());
    std::vector<unsigned char> vchValue(value.begin(), value.end());
    auto ret = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP;
    if (fakeSuffix) ret.push_back(OP_TRUE);
    return ret;
}

CScript SupportClaimScript(std::string name, uint160 claimId, bool fakeSuffix)
{
    std::vector<unsigned char> vchName(name.begin(), name.end());
    std::vector<unsigned char> vchClaimId(claimId.begin(), claimId.end());
    auto ret = CScript() << OP_SUPPORT_CLAIM << vchName << vchClaimId << OP_2DROP << OP_DROP;
    if (fakeSuffix) ret.push_back(OP_TRUE);
    return ret;
}

CScript UpdateClaimScript(std::string name, uint160 claimId, std::string value)
{
    std::vector<unsigned char> vchName(name.begin(), name.end());
    std::vector<unsigned char> vchClaimId(claimId.begin(), claimId.end());
    std::vector<unsigned char> vchValue(value.begin(), value.end());
    return CScript() << OP_UPDATE_CLAIM << vchName << vchClaimId << vchValue << OP_2DROP << OP_2DROP << OP_TRUE;
}

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams)
{
    CScript::const_iterator pc = scriptIn.begin();
    return DecodeClaimScript(scriptIn, op, vvchParams, pc);
}

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc)
{
    op = -1;
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
        static const size_t claimIdHashSize = sizeof(uint160);
        if (vchParam2.size() != claimIdHashSize) {
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

size_t ClaimNameSize(const CScript& scriptIn)
{
    std::vector<std::vector<unsigned char> > vvchParams;
    CScript::const_iterator pc = scriptIn.begin();
    int op;
    if (!DecodeClaimScript(scriptIn, op, vvchParams, pc))
    {
        return 0;
    }
    else
    {
        return vvchParams[0].size();
    }
}

CAmount CalcMinClaimTrieFee(const CTransaction& tx, const CAmount &minFeePerNameClaimChar)
{
    if (minFeePerNameClaimChar == 0)
    {
        return 0;
    }

    CAmount min_fee = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        int op;
        std::vector<std::vector<unsigned char> > vvchParams;
        if (DecodeClaimScript(txout.scriptPubKey, op, vvchParams))
        {
            if (op == OP_CLAIM_NAME)
            {
                int claim_name_size = vvchParams[0].size();
                min_fee += claim_name_size*minFeePerNameClaimChar;
            }
        }
    }
    return min_fee;
}
