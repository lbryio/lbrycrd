#include <nameclaim.h>
#include <hash.h>
#include <logging.h>

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
        LogPrintf("%s() : a vector<unsigned char> with size other than 4 has been given\n", __func__);
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

CScript SupportClaimScript(std::string name, uint160 claimId, std::string value, bool fakeSuffix)
{
    std::vector<unsigned char> vchName(name.begin(), name.end());
    std::vector<unsigned char> vchClaimId(claimId.begin(), claimId.end());
    CScript ret;
    if (value.empty())
        ret = CScript() << OP_SUPPORT_CLAIM << vchName << vchClaimId << OP_2DROP << OP_DROP;
    else {
        std::vector<unsigned char> vchValue(value.begin(), value.end());
        ret = CScript() << OP_SUPPORT_CLAIM << vchName << vchClaimId << vchValue << OP_2DROP << OP_2DROP;
    }
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

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, bool allowSupportMetadata)
{
    CScript::const_iterator pc = scriptIn.begin();
    return DecodeClaimScript(scriptIn, op, vvchParams, pc, allowSupportMetadata);
}

bool DecodeClaimScript(const CScript& scriptIn, int& op, std::vector<std::vector<unsigned char> >& vvchParams, CScript::const_iterator& pc, bool allowSupportMetadata)
{
    op = -1;
    opcodetype opcode;
    if (!scriptIn.GetOp(pc, opcode))
        return false;

    if (opcode != OP_CLAIM_NAME && opcode != OP_SUPPORT_CLAIM && opcode != OP_UPDATE_CLAIM)
        return false;

    op = opcode;

    std::vector<unsigned char> vchParam1;
    std::vector<unsigned char> vchParam2;
    std::vector<unsigned char> vchParam3;
    // Valid formats:
    // OP_CLAIM_NAME vchName vchValue OP_2DROP OP_DROP pubkeyscript
    // OP_UPDATE_CLAIM vchName vchClaimId vchValue OP_2DROP OP_2DROP pubkeyscript
    // OP_SUPPORT_CLAIM vchName vchClaimId OP_2DROP OP_DROP pubkeyscript
    // OP_SUPPORT_CLAIM vchName vchClaimId vchValue OP_2DROP OP_2DROP pubkeyscript
    // All others are invalid.

    if (!scriptIn.GetOp(pc, opcode, vchParam1) || opcode < 0 || opcode > OP_PUSHDATA4)
        return false;

    if (!scriptIn.GetOp(pc, opcode, vchParam2) || opcode < 0 || opcode > OP_PUSHDATA4)
        return false;

    if (op == OP_UPDATE_CLAIM || op == OP_SUPPORT_CLAIM)
        if (vchParam2.size() != sizeof(uint160))
            return false;

    if (!scriptIn.GetOp(pc, opcode, vchParam3))
        return false;

    auto last_drop = OP_DROP;
    if (opcode >= 0 && opcode <= OP_PUSHDATA4 && op != OP_CLAIM_NAME) {
        if (!scriptIn.GetOp(pc, opcode))
            return false;
        last_drop = OP_2DROP;
    } else if (op == OP_UPDATE_CLAIM)
        return false;

    if (opcode != OP_2DROP)
        return false;

    if (!scriptIn.GetOp(pc, opcode) || opcode != last_drop)
        return false;

    if (op == OP_SUPPORT_CLAIM && last_drop == OP_2DROP && !allowSupportMetadata)
        return false;

    vvchParams.push_back(std::move(vchParam1));
    vvchParams.push_back(std::move(vchParam2));
    if (last_drop == OP_2DROP)
        vvchParams.push_back(std::move(vchParam3));

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
        return scriptIn;

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
        return 0;
    return vvchParams[0].size();
}

CAmount CalcMinClaimTrieFee(const CMutableTransaction& tx, const CAmount &minFeePerNameClaimChar)
{
    if (minFeePerNameClaimChar == 0)
        return 0;

    CAmount min_fee = 0;
    for (const CTxOut& txout: tx.vout) {
        int op;
        std::vector<std::vector<unsigned char> > vvchParams;
        if (DecodeClaimScript(txout.scriptPubKey, op, vvchParams)) {
            if (op == OP_CLAIM_NAME) {
                int claim_name_size = vvchParams[0].size();
                min_fee += claim_name_size*minFeePerNameClaimChar;
            }
        }
    }
    return min_fee;
}
