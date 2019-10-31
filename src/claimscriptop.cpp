// Copyright (c) 2018 The LBRY developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <claimscriptop.h>
#include <nameclaim.h>

CClaimScriptAddOp::CClaimScriptAddOp(const COutPoint& point, CAmount nValue, int nHeight)
    : point(point), nValue(nValue), nHeight(nHeight)
{
}

bool CClaimScriptAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name,
        const std::vector<unsigned char>& metadata)
{
    return addClaim(trieCache, name, ClaimIdHash(point.hash, point.n), -1, metadata);
}

bool CClaimScriptAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    return addClaim(trieCache, name, claimId, -1, metadata);
}

bool CClaimScriptAddOp::addClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
                                 int takeoverHeight, const std::vector<unsigned char>& metadata)
{
    return trieCache.addClaim(name, point, claimId, nValue, nHeight, takeoverHeight, metadata);
}

bool CClaimScriptAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
                                     const std::vector<unsigned char>& metadata)
{
    return trieCache.addSupport(name, point, nValue, claimId, nHeight, -1, metadata);
}

CClaimScriptUndoAddOp::CClaimScriptUndoAddOp(const COutPoint& point, int nHeight) : point(point), nHeight(nHeight)
{
}

bool CClaimScriptUndoAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name,
        const std::vector<unsigned char>& metadata)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "--- [%lu]: Undoing OP_CLAIM_NAME \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "--- [%lu]: Undoing OP_UPDATE_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::undoAddClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    int validHeight;
    bool res = trieCache.removeClaim(claimId, point, nodeName, validHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "%s: Removing claim fails\n", __func__);
    return res;
}

bool CClaimScriptUndoAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    if (LogAcceptCategory(BCLog::CLAIMS)) {
        LogPrintf("--- [%lu]: Undoing OP_SUPPORT_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name,
                  claimId.GetHex(), point.hash.ToString(), point.n);
    }
    std::string nodeName;
    int validHeight;
    bool res = trieCache.removeSupport(point, nodeName, validHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "%s: Removing support fails\n", __func__);
    return res;
}

CClaimScriptSpendOp::CClaimScriptSpendOp(const COutPoint& point, int nHeight, int& nValidHeight)
    : point(point), nHeight(nHeight), nValidHeight(nValidHeight)
{
}

bool CClaimScriptSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name,
        const std::vector<unsigned char>& metadata)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "+++ [%lu]: Spending OP_CLAIM_NAME \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return spendClaim(trieCache, name, claimId);
}

bool CClaimScriptSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "+++ [%lu]: Spending OP_UPDATE_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name, claimId.GetHex(), point.hash.ToString(), point.n);
    return spendClaim(trieCache, name, claimId);
}

bool CClaimScriptSpendOp::spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    bool res = trieCache.removeClaim(claimId, point, nodeName, nValidHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "%s: Removing fails\n", __func__);
    return res;
}

bool CClaimScriptSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    if (LogAcceptCategory(BCLog::CLAIMS)) {
        LogPrintf("+++ [%lu]: Spending OP_SUPPORT_CLAIM \"%s\" with claimId %s and tx prevout %s at index %d\n", nHeight, name,
                  claimId.GetHex(), point.hash.ToString(), point.n);
    }
    std::string nodeName;
    bool res = trieCache.removeSupport(point, nodeName, nValidHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "%s: Removing support fails\n", __func__);
    return res;
}

CClaimScriptUndoSpendOp::CClaimScriptUndoSpendOp(const COutPoint& point, CAmount nValue, int nHeight, int nValidHeight)
    : point(point), nValue(nValue), nHeight(nHeight), nValidHeight(nValidHeight)
{
}

bool CClaimScriptUndoSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name,
        const std::vector<unsigned char>& metadata)
{
    return undoSpendClaim(trieCache, name, ClaimIdHash(point.hash, point.n), metadata);
}

bool CClaimScriptUndoSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    return undoSpendClaim(trieCache, name, claimId, metadata);
}

bool CClaimScriptUndoSpendOp::undoSpendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "%s: (txid: %s, nOut: %d) Undoing spend of %s, claimId: %s, to the claim trie due to block disconnect\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    return trieCache.addClaim(name, point, claimId, nValue, nHeight, nValidHeight, metadata);
}

bool CClaimScriptUndoSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "%s: (txid: %s, nOut: %d) Restoring support for %s, claimId: %s, to the claim trie due to block disconnect\n", __func__, point.hash.ToString(), point.n, name, claimId.ToString());
    return trieCache.addSupport(name, point, nValue, claimId, nHeight, nValidHeight, metadata);
}

static std::string vchToString(const std::vector<unsigned char>& name)
{
    return std::string(name.begin(), name.end());
}

bool ProcessClaim(CClaimScriptOp& claimOp, CClaimTrieCache& trieCache, const CScript& scriptPubKey)
{
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    if (!DecodeClaimScript(scriptPubKey, op, vvchParams, trieCache.allowSupportMetadata()))
        return false;

    switch (op) {
        case OP_CLAIM_NAME:
            return claimOp.claimName(trieCache, vchToString(vvchParams[0]), vvchParams[1]);
        case OP_SUPPORT_CLAIM:
            return claimOp.supportClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]),
                    vvchParams.size() > 2 ? vvchParams[2] : std::vector<unsigned char>());
        case OP_UPDATE_CLAIM:
            return claimOp.updateClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]),
                    vvchParams[2]);
        default:
            throw std::runtime_error("Unimplemented OP handler.");
    }
}

void UpdateCache(const CTransaction& tx, CClaimTrieCache& trieCache, const CCoinsViewCache& view, int nHeight, const CUpdateCacheCallbacks& callbacks)
{
    class CSpendClaimHistory : public CClaimScriptSpendOp
    {
    public:
        using CClaimScriptSpendOp::CClaimScriptSpendOp;

        bool spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId) override
        {
            if (CClaimScriptSpendOp::spendClaim(trieCache, name, claimId)) {
                callback(name, claimId);
                return true;
            }
            return false;
        }
        std::function<void(const std::string& name, const uint160& claimId)> callback;
    };

    spentClaimsType spentClaims;

    for (std::size_t j = 0; j < tx.vin.size(); j++) {
        const CTxIn& txin = tx.vin[j];
        const Coin& coin = view.AccessCoin(txin.prevout);

        CScript scriptPubKey;
        int scriptHeight = nHeight;
        if (coin.out.IsNull() && callbacks.findScriptKey) {
            scriptPubKey = callbacks.findScriptKey(txin.prevout);
        } else {
            scriptHeight = coin.nHeight;
            scriptPubKey = coin.out.scriptPubKey;
        }

        if (scriptPubKey.empty())
            continue;

        int nValidAtHeight;
        CSpendClaimHistory spendClaim(COutPoint(txin.prevout.hash, txin.prevout.n), scriptHeight, nValidAtHeight);
        spendClaim.callback = [&spentClaims](const std::string& name, const uint160& claimId) {
            spentClaims.emplace_back(name, claimId);
        };
        if (ProcessClaim(spendClaim, trieCache, scriptPubKey) && callbacks.claimUndoHeights)
            callbacks.claimUndoHeights(j, nValidAtHeight);
    }

    class CAddSpendClaim : public CClaimScriptAddOp
    {
    public:
        using CClaimScriptAddOp::CClaimScriptAddOp;

        bool updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
                         const std::vector<unsigned char>& metadata) override
        {
            if (callback(name, claimId))
                return CClaimScriptAddOp::addClaim(trieCache, name, claimId, -1, metadata);
            return false;
        }
        std::function<bool(const std::string& name, const uint160& claimId)> callback;
    };

    for (std::size_t j = 0; j < tx.vout.size(); j++) {
        const CTxOut& txout = tx.vout[j];

        if (txout.scriptPubKey.empty())
            continue;

        CAddSpendClaim addClaim(COutPoint(tx.GetHash(), j), txout.nValue, nHeight);
        addClaim.callback = [&trieCache, &spentClaims](const std::string& name, const uint160& claimId) -> bool {
            for (auto itSpent = spentClaims.begin(); itSpent != spentClaims.end(); ++itSpent) {
                if (itSpent->second == claimId && trieCache.normalizeClaimName(name) == trieCache.normalizeClaimName(itSpent->first)) {
                    spentClaims.erase(itSpent);
                    return true;
                }
            }
            return false;
        };
        ProcessClaim(addClaim, trieCache, txout.scriptPubKey);
    }
}
