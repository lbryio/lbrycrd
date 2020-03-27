
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

template <typename T>
struct IsHashable {
    static const bool value = false;
};

template <typename T>
struct IsHashable<std::vector<T>> {
    static const bool value = true;
};

template <>
struct IsHashable<std::string> {
    static const bool value = true;
};

template <>
struct IsHashable<uint256> {
    static const bool value = true;
};

template <typename T>
uint256 Hash(const T& c)
{
    static_assert(IsHashable<T>::value, "T is not hashable");
    return Hash(c.begin(), c.end());
}

template <typename T1, typename T2>
uint256 Hash2(const T1& c1, const T2& c2)
{
    static_assert(IsHashable<T1>::value, "T1 is not hashable");
    static_assert(IsHashable<T2>::value, "T2 is not hashable");
    return Hash(c1.begin(), c1.end(), c2.begin(), c2.end());
}

extern std::function<void(std::vector<uint256>&)> sha256n_way;

#endif // CLAIMTRIE_HASHES_H
