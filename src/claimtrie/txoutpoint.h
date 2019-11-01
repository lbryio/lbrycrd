
#ifndef CLAIMTRIE_TXOUTPUT_H
#define CLAIMTRIE_TXOUTPUT_H

#include <uints.h>

#include <algorithm>
#include <string>
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

    bool operator<(const CTxOutPoint& b) const;
    bool operator==(const CTxOutPoint& b) const;
    bool operator!=(const CTxOutPoint& b) const;

    std::string ToString() const;
};

#endif // CLAIMTRIE_TXOUTPUT_H
