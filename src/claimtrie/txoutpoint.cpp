
#include <txoutpoint.h>

#include <limits>
#include <sstream>

COutPoint::COutPoint() noexcept
{
    SetNull();
}

COutPoint::COutPoint(uint256 hashIn, uint32_t nIn) : hash(std::move(hashIn)), n(nIn)
{
}

void COutPoint::SetNull()
{
    hash.SetNull();
    n = std::numeric_limits<uint32_t>::max();
}

bool COutPoint::IsNull() const
{
    return hash.IsNull() && n == std::numeric_limits<uint32_t>::max();
}

bool COutPoint::operator<(const COutPoint& b) const
{
    int cmp = hash.Compare(b.hash);
    return cmp < 0 || (cmp == 0 && n < b.n);
}

bool COutPoint::operator==(const COutPoint& b) const
{
    return hash == b.hash && n == b.n;
}

bool COutPoint::operator!=(const COutPoint& b) const
{
    return !(*this == b);
}

std::string COutPoint::ToString() const
{
    std::stringstream ss;
    ss << "COutPoint(" << hash.ToString().substr(0, 10) << ", " << n << ')';
    return ss.str();
}
