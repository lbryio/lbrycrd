
#include <uints.h>
#include <cstring>

uint160::uint160(const std::vector<uint8_t>& vec) : CBaseBlob<160>(vec)
{
}

uint256::uint256(const std::vector<uint8_t>& vec) : CBaseBlob<256>(vec)
{
}

uint256::uint256(int64_t value) : CBaseBlob<256>() {
    std::memcpy(this->begin(), &value, sizeof(value)); // TODO: fix the endianness here
}

uint160 uint160S(const char* str)
{
    uint160 s;
    s.SetHex(str);
    return s;
}

uint160 uint160S(const std::string& s)
{
    return uint160S(s.c_str());
}

uint256 uint256S(const char* str)
{
    uint256 s;
    s.SetHex(str);
    return s;
}

uint256 uint256S(const std::string& s)
{
    return uint256S(s.c_str());
}
