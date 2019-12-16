
#ifndef CLAIMTRIE_BLOB_H
#define CLAIMTRIE_BLOB_H

#include <array>
#include <string>
#include <vector>

/** Template base class for fixed-sized opaque blobs. */
template<uint32_t BITS>
class CBaseBlob
{
    std::array<uint8_t, BITS / 8> data;
public:
    CBaseBlob();

    explicit CBaseBlob(const std::vector<uint8_t>& vec);

    CBaseBlob(CBaseBlob&&) = default;
    CBaseBlob& operator=(CBaseBlob&&) = default;

    CBaseBlob(const CBaseBlob& o);
    CBaseBlob& operator=(const CBaseBlob& o);

    int Compare(const CBaseBlob& b) const;
    bool operator<(const CBaseBlob& b) const;
    bool operator==(const CBaseBlob& b) const;
    bool operator!=(const CBaseBlob& b) const;

    uint8_t* begin() noexcept;
    const uint8_t* begin() const noexcept;

    uint8_t* end() noexcept;
    const uint8_t* end() const noexcept;

    std::size_t size() const;

    bool IsNull() const;
    void SetNull();

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);

    std::string ToString() const;

    uint64_t GetUint64(int pos) const;
};

#endif // CLAIMTRIE_BLOB_H
