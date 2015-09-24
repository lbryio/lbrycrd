// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
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
    unsigned int nClaimValidHeight;   // If the outpoint was a name claim, the height at which the claim should be inserted into the trie

    CTxInUndo() : txout(), fLastUnspent(false), fCoinBase(false), nHeight(0), nVersion(0), nClaimValidHeight(0) {}
    CTxInUndo(const CTxOut &txoutIn, bool fLastUnspent = false, bool fCoinBaseIn = false, unsigned int nHeightIn = 0, int nVersionIn = 0, unsigned int nClaimValidHeight = 0) : txout(txoutIn), fLastUnspent(fLastUnspent), fCoinBase(fCoinBaseIn), nHeight(nHeightIn), nVersion(nVersionIn), nClaimValidHeight(nClaimValidHeight) { }

    unsigned int GetSerializeSize(int nType, int nVersion) const {
        return ::GetSerializeSize(VARINT(nHeight*4+(fCoinBase ? 2 : 0)+(fLastUnspent ? 1: 0)), nType, nVersion) +
               (fLastUnspent ? ::GetSerializeSize(VARINT(this->nVersion), nType, nVersion) : 0) +
               ::GetSerializeSize(CTxOutCompressor(REF(txout)), nType, nVersion) +
               ::GetSerializeSize(VARINT(nClaimValidHeight), nType, nVersion);
    }

    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersion) const {
        ::Serialize(s, VARINT(nHeight*4+(fCoinBase ? 2 : 0)+(fLastUnspent ? 1: 0)), nType, nVersion);
        if (fLastUnspent)
            ::Serialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Serialize(s, CTxOutCompressor(REF(txout)), nType, nVersion);
        ::Serialize(s, VARINT(nClaimValidHeight), nType, nVersion);
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
    CClaimTrieQueueUndo insertUndo; // any claims that went from the queue to the trie
    CClaimTrieQueueUndo expireUndo; // any claims that expired

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(vtxundo);
        READWRITE(insertUndo);
        READWRITE(expireUndo);
    }
};

#endif // BITCOIN_UNDO_H
