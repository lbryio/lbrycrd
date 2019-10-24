
#ifndef CLAIMTRIE_HASHES_H
#define CLAIMTRIE_HASHES_H

#include <openssl/sha.h>

#include <uints.h>

CUint256 CalcHash(SHA256_CTX* sha);

template<typename TIterator, typename... Args>
CUint256 CalcHash(SHA256_CTX* sha, TIterator begin, TIterator end, Args... args)
{
    static uint8_t blank;
    SHA256_Update(sha, begin == end ? &blank : (uint8_t*)&begin[0], std::distance(begin, end) * sizeof(begin[0]));
    return CalcHash(sha, args...);
}

template<typename TIterator, typename... Args>
CUint256 Hash(TIterator begin, TIterator end, Args... args)
{
    static_assert((sizeof...(args) & 1) != 1, "Parameters should be even number");
    SHA256_CTX sha;
    SHA256_Init(&sha);
    return CalcHash(&sha, begin, end, args...);
}

#endif // CLAIMTRIE_HASHES_H
