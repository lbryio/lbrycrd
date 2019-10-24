
#include <txoutpoint.h>

#include <sstream>

CTxOutPoint::CTxOutPoint(CUint256 hashIn, uint32_t nIn) : hash(std::move(hashIn)), n(nIn)
{
}

void CTxOutPoint::SetNull()
{
    hash.SetNull();
    n = uint32_t(-1);
}

bool CTxOutPoint::IsNull() const
{
    return hash.IsNull() && n == uint32_t(-1);
}

bool CTxOutPoint::operator<(const CTxOutPoint& b) const
{
    int cmp = hash.Compare(b.hash);
    return cmp < 0 || (cmp == 0 && n < b.n);
}

bool CTxOutPoint::operator==(const CTxOutPoint& b) const
{
    return hash == b.hash && n == b.n;
}

bool CTxOutPoint::operator!=(const CTxOutPoint& b) const
{
    return !(*this == b);
}

std::string CTxOutPoint::ToString() const
{
    std::stringstream ss;
    ss << "CTxOutPoint(" << hash.ToString().substr(0, 10) << ", " << n << ')';
    return ss.str();
}
