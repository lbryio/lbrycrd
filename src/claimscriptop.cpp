// Copyright (c) 2018 The LBRY developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "claimscriptop.h"
#include "nameclaim.h"

CClaimScriptAddOp::CClaimScriptAddOp(const COutPoint& point, CAmount nValue, int nHeight)
    : point(point), nValue(nValue), nHeight(nHeight)
{
}

bool CClaimScriptAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    return addClaim(trieCache, name, ClaimIdHash(point.hash, point.n));
}

bool CClaimScriptAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return addClaim(trieCache, name, claimId);
}

bool CClaimScriptAddOp::addClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.addClaim(name, point, claimId, nValue, nHeight);

    LogPrintf("--- %s[%lu]: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, nHeight, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

bool CClaimScriptAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.addSupport(name, point, nValue, claimId, nHeight);

    LogPrintf("--- %s[%lu]: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, nHeight, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

CClaimScriptUndoAddOp::CClaimScriptUndoAddOp(const COutPoint& point) : point(point)
{
}

bool CClaimScriptUndoAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    return undoAddClaim(trieCache, name, ClaimIdHash(point.hash, point.n));
}

bool CClaimScriptUndoAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::undoAddClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.undoAddClaim(name, point);

    LogPrintf("--- %s: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

bool CClaimScriptUndoAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.undoAddSupport(name, point);

    LogPrintf("--- %s: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

CClaimScriptSpendOp::CClaimScriptSpendOp(const COutPoint& point, int& nValidHeight)
    : point(point), nValidHeight(nValidHeight)
{
}

bool CClaimScriptSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    return spendClaim(trieCache, name, ClaimIdHash(point.hash, point.n));
}

bool CClaimScriptSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return spendClaim(trieCache, name, claimId);
}

bool CClaimScriptSpendOp::spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.spendClaim(name, point, nValidHeight);

    LogPrintf("--- %s: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

bool CClaimScriptSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.spendSupport(name, point, nValidHeight);

    LogPrintf("--- %s: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

CClaimScriptUndoSpentOp::CClaimScriptUndoSpentOp(const COutPoint& point, CAmount nValue, int nHeight, int nValidHeight)
    : point(point), nValue(nValue), nHeight(nHeight), nValidHeight(nValidHeight)
{
}

bool CClaimScriptUndoSpentOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    return undoSpendClaim(trieCache, name, ClaimIdHash(point.hash, point.n));
}

bool CClaimScriptUndoSpentOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return undoSpendClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoSpentOp::undoSpendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.undoSpendClaim(name, point, claimId, nValue, nHeight, nValidHeight);

    LogPrintf("--- %s[%lu]: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, nHeight, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

bool CClaimScriptUndoSpentOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    bool succeed = trieCache.undoSpendSupport(name, point, claimId, nValue, nHeight, nValidHeight);

    LogPrintf("--- %s[%lu]: %s for \"%s\" with claimId %s and tx prevout %s at index %d\n",
        __func__, nHeight, (succeed ? "SUCCEED" : "FAILED"), name, claimId.GetHex(), point.hash.ToString(), point.n);

    return succeed;
}

static std::string vchToString(const std::vector<unsigned char>& name)
{
    return std::string(name.begin(), name.end());
}

bool ProcessClaim(CClaimScriptOp& claimOp, CClaimTrieCache& trieCache, const CScript& scriptPubKey)
{
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;

    if (!DecodeClaimScript(scriptPubKey, op, vvchParams))
        return false;

    LogPrintf("%s - %s\n", __func__, GetOpName(opcodetype(op)));

    switch (op) {
    case OP_CLAIM_NAME:
        assert(vvchParams.size() == 2);
        return claimOp.claimName(trieCache, vchToString(vvchParams[0]));
    case OP_SUPPORT_CLAIM:
        assert(vvchParams.size() == 2);
        return claimOp.supportClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]));
    case OP_UPDATE_CLAIM:
        assert(vvchParams.size() == 3);
        return claimOp.updateClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]));
    default:
        return false;
    }
}

bool SpendClaim(CClaimTrieCache& trieCache, const CScript& scriptPubKey, const COutPoint& point, int& nValidHeight, spentClaimsType& spentClaims)
{
    class CSpendClaimHistory : public CClaimScriptSpendOp
    {
    public:
        CSpendClaimHistory(spentClaimsType& spentClaims, const COutPoint& point, int& nValidHeight)
            : CClaimScriptSpendOp(point, nValidHeight), spentClaims(spentClaims)
        {
        }

        bool spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
        {
            if (CClaimScriptSpendOp::spendClaim(trieCache, name, claimId)) {
                spentClaims.push_back(spentClaimType(name, claimId));
                return true;
            }
            return false;
        }

    private:
        spentClaimsType& spentClaims;
    };

    CSpendClaimHistory spendClaim(spentClaims, point, nValidHeight);
    return ProcessClaim(spendClaim, trieCache, scriptPubKey);
}

bool AddSpentClaim(CClaimTrieCache& trieCache, const CScript& scriptPubKey, const COutPoint& point, CAmount nValue, int nHeight, spentClaimsType& spentClaims)
{
    class CAddSpentClaim : public CClaimScriptAddOp
    {
    public:
        CAddSpentClaim(spentClaimsType& spentClaims, const COutPoint& point, CAmount nValue, int nHeight)
            : CClaimScriptAddOp(point, nValue, nHeight), spentClaims(spentClaims)
        {
        }

        bool updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
        {
            spentClaimsType::iterator itSpent = spentClaims.begin();
            for (; itSpent != spentClaims.end(); ++itSpent) {
                if (itSpent->first == name && itSpent->second == claimId) {
                    spentClaims.erase(itSpent);
                    return CClaimScriptAddOp::updateClaim(trieCache, name, claimId);
                }
            }
            return false;
        }

    private:
        spentClaimsType& spentClaims;
    };

    CAddSpentClaim addClaim(spentClaims, point, nValue, nHeight);
    return ProcessClaim(addClaim, trieCache, scriptPubKey);
}
