
#ifndef CLAIMTRIE_HASHES_H
#define CLAIMTRIE_HASHES_H

#include <openssl/sha.h>

#include <functional>
#include <vector>

#include <uints.h>

uint256 CalcHash(SHA256_CTX* sha);

template<typename TIterator, typename... Args>
uint256 CalcHash(SHA256_CTX* sha, TIterator begin, TIterator end, Args... args)
{
    static uint8_t blank;
    SHA256_Update(sha, begin == end ? &blank : (uint8_t*)&begin[0], std::distance(begin, end) * sizeof(begin[0]));
    return CalcHash(sha, args...);
}

template<typename TIterator, typename... Args>
uint256 Hash(TIterator begin, TIterator end, Args... args)
{
    static_assert((sizeof...(args) & 1) != 1, "Parameters should be even number");
    SHA256_CTX sha;
    SHA256_Init(&sha);
    return CalcHash(&sha, begin, end, args...);
}

extern std::function<void(std::vector<uint256>&)> sha256n_way;

#endif // CLAIMTRIE_HASHES_H
