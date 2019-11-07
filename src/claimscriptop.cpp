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
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "+++ Claim added: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
                            name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
    return addClaim(trieCache, name, claimId, -1, metadata);
}

bool CClaimScriptAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "+++ Claim updated: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
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
    LogPrint(BCLog::CLAIMS, "+++ Support added: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
    return trieCache.addSupport(name, point, nValue, claimId, nHeight, -1, metadata);
}

CClaimScriptUndoAddOp::CClaimScriptUndoAddOp(const COutPoint& point, int nHeight) : point(point), nHeight(nHeight)
{
}

bool CClaimScriptUndoAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name,
        const std::vector<unsigned char>& metadata)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "--- Undoing claim add: %s, c: %.6s, t: %.6s:%d, h: %.6d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "--- Undoing claim update: %s, c: %.6s, t: %.6s:%d, h: %.6d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::undoAddClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    int validHeight;
    bool res = trieCache.removeClaim(claimId, point, nodeName, validHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove claim failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
    return res;
}

bool CClaimScriptUndoAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "--- Undoing support add: %s, c: %.6s, t: %.6s:%d, h: %.6d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight);
    std::string nodeName;
    int validHeight;
    bool res = trieCache.removeSupport(point, nodeName, validHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove support failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
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
    auto ret = spendClaim(trieCache, name, claimId);
    LogPrint(BCLog::CLAIMS, "--- Spending original claim: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return ret;
}

bool CClaimScriptSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    auto ret = spendClaim(trieCache, name, claimId);
    LogPrint(BCLog::CLAIMS, "--- Spending updated claim: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return ret;
}

bool CClaimScriptSpendOp::spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    bool res = trieCache.removeClaim(claimId, point, nodeName, nValidHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove claim failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
    return res;
}

bool CClaimScriptSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    std::string nodeName;
    bool res = trieCache.removeSupport(point, nodeName, nValidHeight);
    LogPrint(BCLog::CLAIMS, "--- Spending support: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove support failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
    return res;
}

CClaimScriptUndoSpendOp::CClaimScriptUndoSpendOp(const COutPoint& point, CAmount nValue, int nHeight, int nValidHeight)
    : point(point), nValue(nValue), nHeight(nHeight), nValidHeight(nValidHeight)
{
}

bool CClaimScriptUndoSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name,
        const std::vector<unsigned char>& metadata)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "+++ Undoing original claim spend: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return undoSpendClaim(trieCache, name, claimId, metadata);
}

bool CClaimScriptUndoSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "+++ Undoing updated claim spend: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return undoSpendClaim(trieCache, name, claimId, metadata);
}

bool CClaimScriptUndoSpendOp::undoSpendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    return trieCache.addClaim(name, point, claimId, nValue, nHeight, nValidHeight, metadata);
}

bool CClaimScriptUndoSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
        const std::vector<unsigned char>& metadata)
{
    LogPrint(BCLog::CLAIMS, "+++ Undoing support spend: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
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
                return CClaimScriptAddOp::updateClaim(trieCache, name, claimId, metadata);
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
