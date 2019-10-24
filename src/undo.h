// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include <coins.h>
#include <compressor.h>
#include <claimtrie.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <version.h>

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class CTxInUndo
{
    uint32_t nDeprecated1 = 0;    // if the outpoint was the last unspent: its version
    bool fDeprecated2 = false;    // whether the outpoint was the last unspent
public:
    CTxOut txout;         // the txout data before being spent
    bool fCoinBase;       // if the outpoint was the last unspent: whether it belonged to a coinbase
    uint32_t nHeight; // if the outpoint was the last unspent: its height
    uint32_t nClaimValidHeight;   // If the outpoint was a claim or support, the height at which the claim or support should be inserted into the trie
    bool fIsClaim;        // if the outpoint was a claim or support

    CTxInUndo() : txout(), fCoinBase(false), nHeight(0), nClaimValidHeight(0), fIsClaim(false) {}
    CTxInUndo(const CTxOut &txoutIn, bool fCoinBaseIn = false, uint32_t nHeightIn = 0) :
        txout(txoutIn), fCoinBase(fCoinBaseIn), nHeight(nHeightIn), nClaimValidHeight(0), fIsClaim(false) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead()) {
            uint32_t nCode = 0;
            READWRITE(VARINT(nCode)); // VARINT in this method is for legacy compatibility
            nHeight = nCode >> 2U;
            fCoinBase = nCode & 2U;
            fDeprecated2 = nCode & 1U;
        } else {
            uint32_t nCode = (nHeight << 2U) | (fCoinBase ? 2U : 0U) | (fDeprecated2 ? 1U: 0U);
            READWRITE(VARINT(nCode));
        }
        if (fDeprecated2)
            READWRITE(VARINT(nDeprecated1));
        READWRITE(REF(CTxOutCompressor(REF(txout))));
        READWRITE(VARINT(nClaimValidHeight));
        READWRITE(fIsClaim);
    }
};

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    // undo information for all txins
    std::vector<CTxInUndo> vprevout;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vprevout);
    }
};

/** Undo information for a CBlock */
class CBlockUndo
{
public:
    std::vector<CTxUndo> vtxundo; // for all but the coinbase
    insertUndoType insertUndo; // any claims that went from the queue to the trie
    claimQueueRowType expireUndo; // any claims that expired
    insertUndoType insertSupportUndo; // any supports that went from the support queue to the support map
    supportQueueRowType expireSupportUndo; // any supports that expired

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vtxundo);
        READWRITE(insertUndo);
        READWRITE(expireUndo);
        READWRITE(insertSupportUndo);
        READWRITE(expireSupportUndo);
    }
};

#endif // BITCOIN_UNDO_H
