// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_PUREHEADER_H
#define BITCOIN_PRIMITIVES_PUREHEADER_H

#include <serialize.h>
#include <uint256.h>

/**
 * A block header without auxpow information.  This "intermediate step"
 * in constructing the full header is useful, because it breaks the cyclic
 * dependency between auxpow (referencing a parent block header) and
 * the block header (referencing an auxpow).  The parent block header
 * does not have auxpow itself, so it is a pure header.
 */
class CPureBlockHeader { // by "pure" we mean "bitcoin"
public:
    // header
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    int32_t nVersion;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CPureBlockHeader() : nVersion(0), nTime(0), nBits(0), nNonce(0) {
    }

    ADD_SERIALIZE_METHODS;

    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    }

    void SetNull() {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    bool IsNull() const {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    uint256 GetPoWHash() const;

    int64_t GetBlockTime() const {
        return (int64_t) nTime;
    }
};
#endif // BITCOIN_PRIMITIVES_PUREHEADER_H
