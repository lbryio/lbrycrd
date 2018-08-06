// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include <coins.h>
#include <claimtrie.h>
#include <compressor.h>
#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <version.h>

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and its metadata as well
 *  (coinbase or not, height). The serialization contains a dummy value of
 *  zero. This is compatible with older versions which expect to see
 *  the transaction version there.
 */
class TxInUndoSerializer
{
    const Coin* txout;
    // whether the outpoint was the last unspent
    bool fLastUnspent;
    // if the outpoint was the last unspent: its version
    unsigned int nVersion;
    // If the outpoint was a claim or support, the height at which the claim or support should be inserted into the trie
    unsigned int nClaimValidHeight;
    // if the outpoint was a claim or support
    bool fIsClaim;

public:
    template<typename Stream>
    void Serialize(Stream &s) const {
        ::Serialize(s, VARINT(txout->nHeight * 4 + (txout->fCoinBase ? 2u : 0u) + (fLastUnspent ? 1u : 0u)));
        if (fLastUnspent)
            ::Serialize(s, VARINT(this->nVersion));
        ::Serialize(s, CTxOutCompressor(REF(txout->out)));
        ::Serialize(s, VARINT(nClaimValidHeight));
        ::Serialize(s, fIsClaim);
    }

    explicit TxInUndoSerializer(const Coin* coin) : txout(coin) {}
};

class TxInUndoDeserializer
{
    Coin* txout;
    // whether the outpoint was the last unspent
    bool fLastUnspent;
    // if the outpoint was the last unspent: its version
    unsigned int nVersion;
    // If the outpoint was a claim or support, the height at which the claim or support should be inserted into the trie
    unsigned int nClaimValidHeight;
    // if the outpoint was a claim or support
    bool fIsClaim;

public:
    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode));
        txout->nHeight = nCode / 4; // >> 2?
        txout->fCoinBase = (nCode & 2) ? 1: 0;
        fLastUnspent = (nCode & 1) > 0;
        if (fLastUnspent)
            ::Unserialize(s, VARINT(this->nVersion));
        ::Unserialize(s, CTxOutCompressor(REF(txout->out)));
        ::Unserialize(s, VARINT(nClaimValidHeight));
        ::Unserialize(s, fIsClaim);
    }

    explicit TxInUndoDeserializer(Coin* coin) : txout(coin) {}
};

static const size_t MIN_TRANSACTION_INPUT_WEIGHT = WITNESS_SCALE_FACTOR * ::GetSerializeSize(CTxIn(), PROTOCOL_VERSION);
static const size_t MAX_INPUTS_PER_BLOCK = MAX_BLOCK_WEIGHT / MIN_TRANSACTION_INPUT_WEIGHT;

/** Undo information for a CTransaction */
class CTxUndo
{
    struct claimState
    {
        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(nClaimValidHeight);
            READWRITE(fIsClaim);
        };

        // If the outpoint was a claim or support, the height at which the
        // claim or support should be inserted into the trie; indexed by Coin index
        unsigned int nClaimValidHeight;
        // if the outpoint was a claim or support; indexed by Coin index
        bool fIsClaim;
    };

    using claimStateMap = std::map<unsigned int, claimState>;

public:
    // undo information for all txins
    std::vector<Coin> vprevout;
    claimStateMap claimInfo;

    CTxUndo() {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vprevout);
        READWRITE(claimInfo);
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
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vtxundo);
        READWRITE(insertUndo);
        READWRITE(expireUndo);
        READWRITE(insertSupportUndo);
        READWRITE(expireSupportUndo);
        READWRITE(takeoverHeightUndo);
    }
};

#endif // BITCOIN_UNDO_H
