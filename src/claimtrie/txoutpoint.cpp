
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

std::string CTxOutPoint::ToString() const
{
    std::stringstream ss;
    ss << "CTxOutPoint(" << hash.ToString().substr(0, 10) << ", " << n << ')';
    return ss.str();
}

bool operator<(const CTxOutPoint& a, const CTxOutPoint& b)
{
    int cmp = a.hash.Compare(b.hash);
    return cmp < 0 || (cmp == 0 && a.n < b.n);
}

bool operator==(const CTxOutPoint& a, const CTxOutPoint& b)
{
    return (a.hash == b.hash && a.n == b.n);
}

bool operator!=(const CTxOutPoint& a, const CTxOutPoint& b)
{
    return !(a == b);
}
