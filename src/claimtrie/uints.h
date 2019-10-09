
#ifndef CLAIMTRIE_UINTS_H
#define CLAIMTRIE_UINTS_H

#include <memory>
#include <vector>

/** Template base class for fixed-sized opaque blobs. */
template<uint32_t BITS>
class CBaseBlob
{
protected:
    static constexpr int WIDTH = BITS / 8;
    std::unique_ptr<uint8_t[]> data;
public:
    CBaseBlob();

    explicit CBaseBlob(const std::vector<uint8_t>& vec);

    CBaseBlob(CBaseBlob&&) = default;
    CBaseBlob& operator=(CBaseBlob&&) = default;

    CBaseBlob(const CBaseBlob& o);
    CBaseBlob& operator=(const CBaseBlob& o);

    bool IsNull() const;
    void SetNull();

    int Compare(const CBaseBlob& other) const;

    friend inline bool operator==(const CBaseBlob& a, const CBaseBlob& b) { return a.Compare(b) == 0; }
    friend inline bool operator!=(const CBaseBlob& a, const CBaseBlob& b) { return a.Compare(b) != 0; }
    friend inline bool operator<(const CBaseBlob& a, const CBaseBlob& b) { return a.Compare(b) < 0; }

    std::string GetHex() const;
    void SetHex(const char* psz);
    void SetHex(const std::string& str);

    std::string ToString() const;

    uint8_t* begin();
    uint8_t* end();

    const uint8_t* begin() const;
    const uint8_t* end() const;

    constexpr uint32_t size() const { return WIDTH; }
};

typedef CBaseBlob<160> CUint160;
typedef CBaseBlob<256> CUint256;

CUint160 CUint160S(const char* str);
CUint160 CUint160S(const std::string& s);

CUint256 CUint256S(const char* str);
CUint256 CUint256S(const std::string& s);

#endif // CLAIMTRIE_UINTS_H
