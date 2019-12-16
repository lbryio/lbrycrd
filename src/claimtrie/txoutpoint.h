
#ifndef CLAIMTRIE_TXOUTPUT_H
#define CLAIMTRIE_TXOUTPUT_H

#include <uints.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>
#include <utility>

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n = uint32_t(-1);

    COutPoint() = default;
    COutPoint(COutPoint&&) = default;
    COutPoint(const COutPoint&) = default;
    COutPoint(uint256 hashIn, uint32_t nIn);

    COutPoint& operator=(COutPoint&&) = default;
    COutPoint& operator=(const COutPoint&) = default;

    void SetNull();
    bool IsNull() const;

    bool operator<(const COutPoint& b) const;
    bool operator==(const COutPoint& b) const;
    bool operator!=(const COutPoint& b) const;

    std::string ToString() const;
};

#endif // CLAIMTRIE_TXOUTPUT_H
