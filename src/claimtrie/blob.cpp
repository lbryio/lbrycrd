
#include <blob.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <sstream>

/** Template base class for fixed-sized opaque blobs. */
template<uint32_t BITS>
CBaseBlob<BITS>::CBaseBlob()
{
    SetNull();
}

template<uint32_t BITS>
CBaseBlob<BITS>::CBaseBlob(const std::vector<uint8_t>& vec)
{
    assert(vec.size() == size());
    std::copy(vec.begin(), vec.end(), begin());
}

template<uint32_t BITS>
CBaseBlob<BITS>::CBaseBlob(const CBaseBlob& o)
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
int CBaseBlob<BITS>::Compare(const CBaseBlob& b) const
{
    return std::memcmp(begin(), b.begin(), size());
}

template<uint32_t BITS>
bool CBaseBlob<BITS>::operator<(const CBaseBlob& b) const
{
    return Compare(b) < 0;
}

template<uint32_t BITS>
bool CBaseBlob<BITS>::operator==(const CBaseBlob& b) const
{
    return Compare(b) == 0;
}

template<uint32_t BITS>
bool CBaseBlob<BITS>::operator!=(const CBaseBlob& b) const
{
    return !(*this == b);
}

template<uint32_t BITS>
bool CBaseBlob<BITS>::IsNull() const
{
    return std::all_of(begin(), end(), [](uint8_t e) { return e == 0; });
}

template<uint32_t BITS>
void CBaseBlob<BITS>::SetNull()
{
    data.fill(0);
}

template<uint32_t BITS>
std::string CBaseBlob<BITS>::GetHex() const
{
    std::stringstream ss;
    ss << std::hex;
    for (auto it = data.rbegin(); it != data.rend(); ++it)
        ss << std::setw(2) << std::setfill('0') << uint32_t(*it);
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
uint64_t CBaseBlob<BITS>::GetUint64(int pos) const
{
    assert((pos + 1) * 8 <= size());
    const uint8_t* ptr = begin() + pos * 8;
    uint64_t res = 0;
    for (int i = 0; i < 8; ++i)
        res |= (uint64_t(ptr[i]) << (8 * i));
    return res;
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
uint8_t* CBaseBlob<BITS>::begin() noexcept
{
    return data.begin();
}

template<uint32_t BITS>
const uint8_t* CBaseBlob<BITS>::begin() const noexcept
{
    return data.begin();
}

template<uint32_t BITS>
uint8_t* CBaseBlob<BITS>::end() noexcept
{
    return data.end();
}

template<uint32_t BITS>
const uint8_t* CBaseBlob<BITS>::end() const noexcept
{
    return data.end();
}

template<uint32_t BITS>
std::size_t CBaseBlob<BITS>::size() const
{
    return data.size();
}

template class CBaseBlob<160>;
template class CBaseBlob<256>;
