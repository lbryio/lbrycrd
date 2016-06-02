// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include "compressor.h" 
#include "claimtrie.h"
#include "primitives/transaction.h"
#include "serialize.h"

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class CTxInUndo
{
public:
    CTxOut txout;         // the txout data before being spent
    bool fLastUnspent;    // whether the outpoint was the last unspent
    bool fCoinBase;       // if the outpoint was the last unspent: whether it belonged to a coinbase
    unsigned int nHeight; // if the outpoint was the last unspent: its height
    int nVersion;         // if the outpoint was the last unspent: its version
    unsigned int nClaimValidHeight;   // If the outpoint was a claim or support, the height at which the claim or support should be inserted into the trie
    bool fIsClaim;        // if the outpoint was a claim or support

    CTxInUndo() : txout(), fLastUnspent(false), fCoinBase(false), nHeight(0), nVersion(0), nClaimValidHeight(0), fIsClaim(false) {}
    CTxInUndo(const CTxOut &txoutIn, bool fLastUnspent = false, bool fCoinBaseIn = false, unsigned int nHeightIn = 0, int nVersionIn = 0, unsigned int nClaimValidHeight = 0, bool fIsClaim = false) : txout(txoutIn), fLastUnspent(fLastUnspent), fCoinBase(fCoinBaseIn), nHeight(nHeightIn), nVersion(nVersionIn), nClaimValidHeight(nClaimValidHeight), fIsClaim(fIsClaim) { }

    unsigned int GetSerializeSize(int nType, int nVersion) const {
        return ::GetSerializeSize(VARINT(nHeight*4+(fCoinBase ? 2 : 0)+(fLastUnspent ? 1: 0)), nType, nVersion) +
               (fLastUnspent ? ::GetSerializeSize(VARINT(this->nVersion), nType, nVersion) : 0) +
               ::GetSerializeSize(CTxOutCompressor(REF(txout)), nType, nVersion) +
               ::GetSerializeSize(VARINT(nClaimValidHeight), nType, nVersion) +
               ::GetSerializeSize(fIsClaim, nType, nVersion);
    }

    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersion) const {
        ::Serialize(s, VARINT(nHeight*4+(fCoinBase ? 2 : 0)+(fLastUnspent ? 1: 0)), nType, nVersion);
        if (fLastUnspent)
            ::Serialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Serialize(s, CTxOutCompressor(REF(txout)), nType, nVersion);
        ::Serialize(s, VARINT(nClaimValidHeight), nType, nVersion);
        ::Serialize(s, fIsClaim, nType, nVersion);
    }

    template<typename Stream>
    void Unserialize(Stream &s, int nType, int nVersion) {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode), nType, nVersion);
        nHeight = nCode / 4;
        fCoinBase = nCode & 2;
        fLastUnspent = nCode & 1;
        if (fLastUnspent)
            ::Unserialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Unserialize(s, REF(CTxOutCompressor(REF(txout))), nType, nVersion);
        ::Unserialize(s, VARINT(nClaimValidHeight), nType, nVersion);
        ::Unserialize(s, fIsClaim, nType, nVersion);
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
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
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
    std::vector<std::pair<std::string, int> > takeoverHeightUndo; // for any name that was taken over, the previous time that name was taken over 

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vtxundo);
        READWRITE(insertUndo);
        READWRITE(expireUndo);
        READWRITE(insertSupportUndo);
        READWRITE(expireSupportUndo);
        READWRITE(takeoverHeightUndo);
    }
};

#endif // BITCOIN_UNDO_H
