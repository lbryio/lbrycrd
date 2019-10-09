
#include <claimtrie/uints.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <sstream>

/** Template base class for fixed-sized opaque blobs. */
template<uint32_t BITS>
CBaseBlob<BITS>::CBaseBlob() : data(std::make_unique<uint8_t[]>(WIDTH))
{
    SetNull();
}

template<uint32_t BITS>
CBaseBlob<BITS>::CBaseBlob(const std::vector<uint8_t>& vec) : data(std::make_unique<uint8_t[]>(WIDTH))
{
    assert(vec.size() == size());
    std::copy(vec.begin(), vec.end(), begin());
}

template<uint32_t BITS>
CBaseBlob<BITS>::CBaseBlob(const CBaseBlob& o) : data(std::make_unique<uint8_t[]>(WIDTH))
{
    *this = o;
}

template<uint32_t BITS>
CBaseBlob<BITS>& CBaseBlob<BITS>::operator=(const CBaseBlob& o)
{
    if (this != &o)
        std::copy(o.begin(), o.end(), begin());
    return *this;
}

template<uint32_t BITS>
bool CBaseBlob<BITS>::IsNull() const
{
    return std::all_of(begin(), end(), [](uint8_t e) { return e == 0; });
}

template<uint32_t BITS>
void CBaseBlob<BITS>::SetNull()
{
    std::memset(begin(), 0, size());
}

template<uint32_t BITS>
int CBaseBlob<BITS>::Compare(const CBaseBlob& other) const
{
    return std::memcmp(begin(), other.begin(), size());
}

template<uint32_t BITS>
std::string CBaseBlob<BITS>::GetHex() const
{
    std::stringstream ss;
    ss << std::hex;
    for (int i = WIDTH - 1; i >= 0; --i)
        ss << std::setw(2) << std::setfill('0') << uint32_t(data[i]);
    return ss.str();
}

template<uint32_t BITS>
void CBaseBlob<BITS>::SetHex(const char* psz)
{
    SetNull();

    // skip leading spaces
    while (isspace(*psz))
        psz++;

    // skip 0x
    if (psz[0] == '0' && tolower(psz[1]) == 'x')
        psz += 2;

    auto b = psz;
    // advance to end
    while (isxdigit(*psz))
        psz++;

    --psz;
    char buf[3] = {};
    auto it = begin();
    while (psz >= b && it != end()) {
        buf[1] = *psz--;
        buf[0] = psz >= b ? *psz-- : '0';
        *it++ = std::strtoul(buf, nullptr, 16);
    }
}

template<uint32_t BITS>
void CBaseBlob<BITS>::SetHex(const std::string& str)
{
    SetHex(str.c_str());
}

template<uint32_t BITS>
std::string CBaseBlob<BITS>::ToString() const
{
    return GetHex();
}

template<uint32_t BITS>
uint8_t* CBaseBlob<BITS>::begin()
{
    return data.get();
}

template<uint32_t BITS>
uint8_t* CBaseBlob<BITS>::end()
{
    return begin() + WIDTH;
}

template<uint32_t BITS>
const uint8_t* CBaseBlob<BITS>::begin() const
{
    return data.get();
}

template<uint32_t BITS>
const uint8_t* CBaseBlob<BITS>::end() const
{
    return begin() + WIDTH;
}

CUint160 CUint160S(const char* str)
{
    CUint160 s;
    s.SetHex(str);
    return s;
}

CUint160 CUint160S(const std::string& s)
{
    return CUint160S(s.c_str());
}

CUint256 CUint256S(const char* str)
{
    CUint256 s;
    s.SetHex(str);
    return s;
}

CUint256 CUint256S(const std::string& s)
{
    return CUint256S(s.c_str());
}

template class CBaseBlob<160>;
template class CBaseBlob<256>;
