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
    return trieCache.addClaim(name, point, claimId, nValue, nHeight);
}

bool CClaimScriptAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return trieCache.addSupport(name, point, nValue, claimId, nHeight);
}

CClaimScriptUndoAddOp::CClaimScriptUndoAddOp(const COutPoint& point, int nHeight) : point(point), nHeight(nHeight)
{
}

bool CClaimScriptUndoAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrintf("--- [%lu]: OP_CLAIM_NAME \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("--- [%lu]: OP_UPDATE_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::undoAddClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("%s: (txid: %s, nOut: %d) Removing %s, claimId: %s, from the claim trie due to block disconnect\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    bool res = trieCache.undoAddClaim(name, point, nHeight);
    if (!res)
        LogPrintf("%s: Removing fails\n", __func__);
    return res;
}

bool CClaimScriptUndoAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("--- [%lu]: OP_SUPPORT_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    LogPrintf("%s: (txid: %s, nOut: %d) Removing support for %s, claimId: %s, from the claim trie due to block disconnect\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    bool res = trieCache.undoAddSupport(name, point, nHeight);
    if (!res)
        LogPrintf("%s: Removing support fails\n", __func__);
    return res;
}

CClaimScriptSpendOp::CClaimScriptSpendOp(const COutPoint& point, int nHeight, int& nValidHeight)
    : point(point), nHeight(nHeight), nValidHeight(nValidHeight)
{
}

bool CClaimScriptSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrintf("+++ [%lu]: OP_CLAIM_NAME \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return spendClaim(trieCache, name, claimId);
}

bool CClaimScriptSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("+++ [%lu]: OP_UPDATE_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return spendClaim(trieCache, name, claimId);
}

bool CClaimScriptSpendOp::spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("%s: (txid: %s, nOut: %d) Removing %s, claimId: %s, from the claim trie\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    bool res = trieCache.spendClaim(name, point, nHeight, nValidHeight);
    if (!res)
        LogPrintf("%s: Removing fails\n", __func__);
    return res;
}

bool CClaimScriptSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("+++ [%lu]: OP_SUPPORT_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    LogPrintf("%s: (txid: %s, nOut: %d) Restoring support for %s, claimId: %s, to the claim trie\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    bool res = trieCache.spendSupport(name, point, nHeight, nValidHeight);
    if (!res)
        LogPrintf("%s: Removing support fails\n", __func__);
    return res;
}

CClaimScriptUndoSpendOp::CClaimScriptUndoSpendOp(const COutPoint& point, CAmount nValue, int nHeight, int nValidHeight)
    : point(point), nValue(nValue), nHeight(nHeight), nValidHeight(nValidHeight)
{
}

bool CClaimScriptUndoSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    return undoSpendClaim(trieCache, name, ClaimIdHash(point.hash, point.n));
}

bool CClaimScriptUndoSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return undoSpendClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoSpendOp::undoSpendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("%s: (txid: %s, nOut: %d) Restoring %s, claimId: %s, to the claim trie due to block disconnect\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    return trieCache.undoSpendClaim(name, point, claimId, nValue, nHeight, nValidHeight);
}

bool CClaimScriptUndoSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrintf("%s: (txid: %s, nOut: %d) Restoring support for %s, claimId: %s, to the claim trie due to block disconnect\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    return trieCache.undoSpendSupport(name, point, claimId, nValue, nHeight, nValidHeight);
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

    switch (op) {
        case OP_CLAIM_NAME:
            return claimOp.claimName(trieCache, vchToString(vvchParams[0]));
        case OP_SUPPORT_CLAIM:
            return claimOp.supportClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]));
        case OP_UPDATE_CLAIM:
            return claimOp.updateClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]));
    }
    throw std::runtime_error("Unimplemented OP handler.");
}

bool SpendClaim(CClaimTrieCache& trieCache, const CScript& scriptPubKey, const COutPoint& point, int nHeight, int& nValidHeight, spentClaimsType& spentClaims)
{
    class CSpendClaimHistory : public CClaimScriptSpendOp
    {
    public:
        CSpendClaimHistory(spentClaimsType& spentClaims, const COutPoint& point, int nHeight, int& nValidHeight)
            : CClaimScriptSpendOp(point, nHeight, nValidHeight), spentClaims(spentClaims)
        {
        }

        bool spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId) override
        {
            if (CClaimScriptSpendOp::spendClaim(trieCache, name, claimId)) {
                spentClaims.emplace_back(name, claimId);
                return true;
            }
            return false;
        }

    private:
        spentClaimsType& spentClaims;
    };

    CSpendClaimHistory spendClaim(spentClaims, point, nHeight, nValidHeight);
    return ProcessClaim(spendClaim, trieCache, scriptPubKey);
}

bool AddSpendClaim(CClaimTrieCache& trieCache, const CScript& scriptPubKey, const COutPoint& point, CAmount nValue, int nHeight, spentClaimsType& spentClaims)
{
    class CAddSpendClaim : public CClaimScriptAddOp
    {
    public:
        CAddSpendClaim(spentClaimsType& spentClaims, const COutPoint& point, CAmount nValue, int nHeight)
            : CClaimScriptAddOp(point, nValue, nHeight), spentClaims(spentClaims)
        {
        }

        bool updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId) override
        {
            spentClaimsType::iterator itSpent = spentClaims.begin();
            for (; itSpent != spentClaims.end(); ++itSpent) {
                if (itSpent->second == claimId && trieCache.normalizeClaimName(name) == trieCache.normalizeClaimName(itSpent->first)) {
                    spentClaims.erase(itSpent);
                    return CClaimScriptAddOp::updateClaim(trieCache, name, claimId);
                }
            }
            return false;
        }

    private:
        spentClaimsType& spentClaims;
    };

    CAddSpendClaim addClaim(spentClaims, point, nValue, nHeight);
    return ProcessClaim(addClaim, trieCache, scriptPubKey);
}