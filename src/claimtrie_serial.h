
#ifndef CLAIMTRIE_SERIAL_H
#define CLAIMTRIE_SERIAL_H

#include <claimtrie/data.h>
#include <claimtrie/txoutpoint.h>
#include <claimtrie/uints.h>

template<typename Stream, uint32_t BITS>
void Serialize(Stream& s, const CBaseBlob<BITS>& u)
{
    s.write((const char*)u.begin(), u.size());
}

template<typename Stream, uint32_t BITS>
void Unserialize(Stream& s, CBaseBlob<BITS>& u)
{
    s.read((char*)u.begin(), u.size());
}

template<typename Stream>
void Serialize(Stream& s, const CTxOutPoint& u)
{
    Serialize(s, u.hash);
    Serialize(s, u.n);
}

template<typename Stream>
void Unserialize(Stream& s, CTxOutPoint& u)
{
    Unserialize(s, u.hash);
    Unserialize(s, u.n);
}

template<typename Stream>
void Serialize(Stream& s, const CClaimValue& u)
{
    Serialize(s, u.outPoint);
    Serialize(s, u.claimId);
    Serialize(s, u.nAmount);
    Serialize(s, u.nHeight);
    Serialize(s, u.nValidAtHeight);
}

template<typename Stream>
void Unserialize(Stream& s, CClaimValue& u)
{
    Unserialize(s, u.outPoint);
    Unserialize(s, u.claimId);
    Unserialize(s, u.nAmount);
    Unserialize(s, u.nHeight);
    Unserialize(s, u.nValidAtHeight);
}

template<typename Stream>
void Serialize(Stream& s, const CSupportValue& u)
{
    Serialize(s, u.outPoint);
    Serialize(s, u.supportedClaimId);
    Serialize(s, u.nAmount);
    Serialize(s, u.nHeight);
    Serialize(s, u.nValidAtHeight);
}

template<typename Stream>
void Unserialize(Stream& s, CSupportValue& u)
{
    Unserialize(s, u.outPoint);
    Unserialize(s, u.supportedClaimId);
    Unserialize(s, u.nAmount);
    Unserialize(s, u.nHeight);
    Unserialize(s, u.nValidAtHeight);
}

template<typename Stream>
void Serialize(Stream& s, const CNameOutPointHeightType& u)
{
    Serialize(s, u.name);
    Serialize(s, u.outPoint);
    Serialize(s, u.nValidHeight);
}

template<typename Stream>
void Unserialize(Stream& s, CNameOutPointHeightType& u)
{
    Unserialize(s, u.name);
    Unserialize(s, u.outPoint);
    Unserialize(s, u.nValidHeight);
}

#endif // CLAIMTRIE_SERIAL_H
