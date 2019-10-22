
#ifndef CLAIMTRIE_TXOUTPUT_H
#define CLAIMTRIE_TXOUTPUT_H

#include <uints.h>

#include <algorithm>
#include <type_traits>
#include <vector>
#include <utility>

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class CTxOutPoint
{
public:
    CUint256 hash;
    uint32_t n = uint32_t(-1);

    CTxOutPoint() = default;
    CTxOutPoint(CTxOutPoint&&) = default;
    CTxOutPoint(const CTxOutPoint&) = default;
    CTxOutPoint(CUint256 hashIn, uint32_t nIn);

    CTxOutPoint& operator=(CTxOutPoint&&) = default;
    CTxOutPoint& operator=(const CTxOutPoint&) = default;

    void SetNull();
    bool IsNull() const;

    friend bool operator<(const CTxOutPoint& a, const CTxOutPoint& b);
    friend bool operator==(const CTxOutPoint& a, const CTxOutPoint& b);
    friend bool operator!=(const CTxOutPoint& a, const CTxOutPoint& b);

    std::string ToString() const;
};

#endif // CLAIMTRIE_TXOUTPUT_H
