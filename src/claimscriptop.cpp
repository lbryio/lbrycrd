// Copyright (c) 2018 The LBRY developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <claimscriptop.h>
#include <consensus/validation.h>
#include <logging.h>
#include <nameclaim.h>
#include <validation.h>
#include <util/translation.h>

#include <boost/locale.hpp>

CClaimScriptAddOp::CClaimScriptAddOp(const COutPoint& point, CAmount nValue, int nHeight)
    : point(point), nValue(nValue), nHeight(nHeight)
{
}

bool CClaimScriptAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "+++ Claim added: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
                            name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
    return addClaim(trieCache, name, claimId, -1, -1);
}

bool CClaimScriptAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrint(BCLog::CLAIMS, "+++ Claim updated: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
    return addClaim(trieCache, name, claimId, -1, -1);
}

bool CClaimScriptAddOp::addClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId,
                                 int takeoverHeight, int originalHeight)
{
    return trieCache.addClaim(name, point, claimId, nValue, nHeight, takeoverHeight, originalHeight);
}

bool CClaimScriptAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrint(BCLog::CLAIMS, "+++ Support added: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
    return trieCache.addSupport(name, point, claimId, nValue, nHeight, -1);
}

CClaimScriptUndoAddOp::CClaimScriptUndoAddOp(const COutPoint& point, int nHeight) : point(point), nHeight(nHeight)
{
}

bool CClaimScriptUndoAddOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "--- Undoing claim add: %s, c: %.6s, t: %.6s:%d, h: %.6d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrint(BCLog::CLAIMS, "--- Undoing claim update: %s, c: %.6s, t: %.6s:%d, h: %.6d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight);
    return undoAddClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoAddOp::undoAddClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    int validHeight, originalHeight;
    bool res = trieCache.removeClaim(claimId, point, nodeName, validHeight, originalHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove claim failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
    return res;
}

bool CClaimScriptUndoAddOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
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

bool CClaimScriptSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    auto ret = spendClaim(trieCache, name, claimId);
    LogPrint(BCLog::CLAIMS, "--- Spent original claim: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return ret;
}

bool CClaimScriptSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    auto ret = spendClaim(trieCache, name, claimId);
    LogPrint(BCLog::CLAIMS, "--- Spent updated claim: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return ret;
}

bool CClaimScriptSpendOp::spendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    bool res = trieCache.removeClaim(claimId, point, nodeName, nValidHeight, nOriginalHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove claim failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
    return res;
}

bool CClaimScriptSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    std::string nodeName;
    bool res = trieCache.removeSupport(point, nodeName, nValidHeight);
    LogPrint(BCLog::CLAIMS, "--- Spent support: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    if (!res)
        LogPrint(BCLog::CLAIMS, "Remove support failed for %s with claimid %s\n", name, claimId.GetHex().substr(0, 6));
    return res;
}

CClaimScriptUndoSpendOp::CClaimScriptUndoSpendOp(const COutPoint& point, CAmount nValue, int nHeight, int nValidHeight, int nOriginalHeight)
    : point(point), nValue(nValue), nHeight(nHeight), nValidHeight(nValidHeight), nOriginalHeight(nOriginalHeight)
{
}

bool CClaimScriptUndoSpendOp::claimName(CClaimTrieCache& trieCache, const std::string& name)
{
    auto claimId = ClaimIdHash(point.hash, point.n);
    LogPrint(BCLog::CLAIMS, "+++ Undoing original claim spend: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return undoSpendClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoSpendOp::updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrint(BCLog::CLAIMS, "+++ Undoing updated claim spend: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return undoSpendClaim(trieCache, name, claimId);
}

bool CClaimScriptUndoSpendOp::undoSpendClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    return trieCache.addClaim(name, point, claimId, nValue, nHeight,
            nValidHeight, nOriginalHeight);
}

bool CClaimScriptUndoSpendOp::supportClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId)
{
    LogPrint(BCLog::CLAIMS, "+++ Undoing support spend: %s, c: %.6s, t: %.6s:%d, h: %.6d, vh: %d\n",
             name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValidHeight);
    return trieCache.addSupport(name, point, claimId, nValue, nHeight, nValidHeight);
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
            return claimOp.claimName(trieCache, vchToString(vvchParams[0]));
        case OP_SUPPORT_CLAIM:
            return claimOp.supportClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]));
        case OP_UPDATE_CLAIM:
            return claimOp.updateClaim(trieCache, vchToString(vvchParams[0]), uint160(vvchParams[1]));
        default:
            throw std::runtime_error(_("Unimplemented OP handler.").translated);
    }
}

static bool isUtf8(const std::string& name)
{
    using namespace boost::locale::conv;
    try {
        return to_utf<char>(name, "UTF-8", stop) == name;
    } catch (const conversion_error&) {
        return false;
    }
}

bool ValidateClaimName(const CScript& scriptPubKey, std::string& reason)
{
    int op;
    std::vector<std::vector<unsigned char> > vvchParams;
    if (!DecodeClaimScript(scriptPubKey, op, vvchParams))
        return true;

    switch (op) {
        case OP_CLAIM_NAME:
        case OP_UPDATE_CLAIM:
        case OP_SUPPORT_CLAIM:
            break;
        default:
            reason = _("Unsupported operation").translated;
            return false;
    }
    const auto name = vchToString(vvchParams[0]);
    if (!isUtf8(name)) {
        reason = _("Claim name is not valid UTF8 string").translated;
        return false;
    }
    static const std::string disallowedSymbols("=&#:$@%?;/\\\"<>*\n\t\r\b\0", 20);
    if (name.find_first_of(disallowedSymbols) != std::string::npos) {
        reason = _("Claim name contains invalid symbol").translated;
        return false;
    }
    return true;
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
                callback(name, claimId, nOriginalHeight);
                return true;
            }
            return false;
        }
        std::function<void(const std::string& name, const uint160& claimId, int originalHeight)> callback;
    };

    spentClaimsType spentClaims;

    for (uint32_t j = 0; j < tx.vin.size(); j++) {
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

        int nValidAtHeight, nOriginalHeight = 0;
        CSpendClaimHistory spendClaim(COutPoint(txin.prevout.hash, txin.prevout.n), scriptHeight, nValidAtHeight);
        spendClaim.callback = [&spentClaims, &nOriginalHeight](const std::string& name, const uint160& claimId, int originalHeight) {
            spentClaims.push_back({name, claimId, originalHeight});
            nOriginalHeight = originalHeight;
        };
        if (ProcessClaim(spendClaim, trieCache, scriptPubKey) && callbacks.claimUndoHeights) {
            // assert(nValidAtHeight > 0 && nOriginalHeight > 0); // fails on tests
            callbacks.claimUndoHeights(j, uint32_t(nValidAtHeight), uint32_t(nOriginalHeight));
        }
    }

    class CAddSpendClaim : public CClaimScriptAddOp
    {
    public:
        using CClaimScriptAddOp::CClaimScriptAddOp;

        bool updateClaim(CClaimTrieCache& trieCache, const std::string& name, const uint160& claimId) override
        {
            auto originalHeight = callback(name, claimId);
            if (originalHeight >= 0) {
                LogPrint(BCLog::CLAIMS, "+++ Claim updated: %s, c: %.6s, t: %.6s:%d, h: %.6d, a: %d\n",
                         name, claimId.GetHex(), point.hash.GetHex(), point.n, nHeight, nValue);
                return CClaimScriptAddOp::addClaim(trieCache, name, claimId, -1, originalHeight);
            }
            return false;
        }
        std::function<int(const std::string& name, const uint160& claimId)> callback;
    };

    for (std::size_t j = 0; j < tx.vout.size(); j++) {
        const CTxOut& txout = tx.vout[j];

        if (txout.scriptPubKey.empty())
            continue;

        CAddSpendClaim addClaim(COutPoint(tx.GetHash(), j), txout.nValue, nHeight);
        addClaim.callback = [&trieCache, &spentClaims](const std::string& name, const uint160& claimId) -> int {
            for (auto itSpent = spentClaims.begin(); itSpent != spentClaims.end(); ++itSpent) {
                if (itSpent->id == claimId && trieCache.normalizeClaimName(name) == trieCache.normalizeClaimName(itSpent->name)) {
                    spentClaims.erase(itSpent);
                    return itSpent->originalHeight;
                }
            }
            return -1;
        };
        ProcessClaim(addClaim, trieCache, txout.scriptPubKey);
    }
}
