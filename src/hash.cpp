// Copyright (c) 2013-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <hash.h>
#include <crypto/common.h>
#include <crypto/hmac_sha512.h>


inline uint32_t ROTL32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

uint256 PoWHash(const std::vector<unsigned char>& input)
{
    CHash256 h256;
    CSHA512 h512;
    CRIPEMD160 h160;

    std::vector<unsigned char> out;
    out.resize(h512.OUTPUT_SIZE);

    std::vector<unsigned char> out_small;
    out_small.resize(h160.OUTPUT_SIZE);

    h256.Write(input.data(), input.size());
    h256.Finalize(&out[0]);
    h256.Reset();

    h512.Write(out.data(), h256.OUTPUT_SIZE);
    h512.Finalize(&out[0]);

    h160.Write(out.data(), h512.OUTPUT_SIZE / 2);
    h160.Finalize(&out_small[0]);
    h160.Reset();

    h256.Write(out_small.data(), h160.OUTPUT_SIZE);

    h160.Write(out.data() + h512.OUTPUT_SIZE / 2, h512.OUTPUT_SIZE / 2);
    h160.Finalize(&out_small[0]);

    out.resize(h256.OUTPUT_SIZE);
    h256.Write(out_small.data(), h160.OUTPUT_SIZE);
    h256.Finalize(&out[0]);

    uint256 result(out);
    return result;
}


unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash)
{
    // The following is MurmurHash3 (x86_32), see http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
    uint32_t h1 = nHashSeed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const int nblocks = vDataToHash.size() / 4;

    //----------
    // body
    const uint8_t* blocks = vDataToHash.data();

    for (int i = 0; i < nblocks; ++i) {
        uint32_t k1 = ReadLE32(blocks + i*4);

        k1 *= c1;
        k1 = ROTL32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = ROTL32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    //----------
    // tail
    const uint8_t* tail = vDataToHash.data() + nblocks * 4;

    uint32_t k1 = 0;

    switch (vDataToHash.size() & 3) {
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = ROTL32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    }

    //----------
    // finalization
    h1 ^= vDataToHash.size();
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

void BIP32Hash(const ChainCode &chainCode, unsigned int nChild, unsigned char header, const unsigned char data[32], unsigned char output[64])
{
    unsigned char num[4];
    num[0] = (nChild >> 24) & 0xFF;
    num[1] = (nChild >> 16) & 0xFF;
    num[2] = (nChild >>  8) & 0xFF;
    num[3] = (nChild >>  0) & 0xFF;
    CHMAC_SHA512(chainCode.begin(), chainCode.size()).Write(&header, 1).Write(data, 32).Write(num, 4).Finalize(output);
}
