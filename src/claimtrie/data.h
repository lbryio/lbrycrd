
#ifndef CLAIMTRIE_DATA_H
#define CLAIMTRIE_DATA_H

#include <txoutpoint.h>
#include <uints.h>

#include <cstring>
#include <string>
#include <vector>

struct CClaimValue
{
    CTxOutPoint outPoint;
    CUint160 claimId;
    int64_t nAmount = 0;
    int64_t nEffectiveAmount = 0;
    int nHeight = 0;
    int nValidAtHeight = 0;

    CClaimValue() = default;
    CClaimValue(CTxOutPoint outPoint, CUint160 claimId, int64_t nAmount, int nHeight, int nValidAtHeight);

    CClaimValue(CClaimValue&&) = default;
    CClaimValue(const CClaimValue&) = default;
    CClaimValue& operator=(CClaimValue&&) = default;
    CClaimValue& operator=(const CClaimValue&) = default;

    bool operator<(const CClaimValue& other) const;
    bool operator==(const CClaimValue& other) const;
    bool operator!=(const CClaimValue& other) const;

    std::string ToString() const;
};

struct CSupportValue
{
    CTxOutPoint outPoint;
    CUint160 supportedClaimId;
    int64_t nAmount = 0;
    int nHeight = 0;
    int nValidAtHeight = 0;

    CSupportValue() = default;
    CSupportValue(CTxOutPoint outPoint, CUint160 supportedClaimId, int64_t nAmount, int nHeight, int nValidAtHeight);

    CSupportValue(CSupportValue&&) = default;
    CSupportValue(const CSupportValue&) = default;
    CSupportValue& operator=(CSupportValue&&) = default;
    CSupportValue& operator=(const CSupportValue&) = default;

    bool operator==(const CSupportValue& other) const;
    bool operator!=(const CSupportValue& other) const;

    std::string ToString() const;
};

typedef std::vector<CClaimValue> claimEntryType;
typedef std::vector<CSupportValue> supportEntryType;

struct CNameOutPointHeightType
{
    std::string name;
    CTxOutPoint outPoint;
    int nValidHeight = 0;

    CNameOutPointHeightType() = default;
    CNameOutPointHeightType(std::string name, CTxOutPoint outPoint, int nValidHeight);
};

struct CClaimNsupports
{
    CClaimNsupports() = default;
    CClaimNsupports(CClaimNsupports&&) = default;
    CClaimNsupports(const CClaimNsupports&) = default;

    bool operator<(const CClaimNsupports& other) const;
    CClaimNsupports& operator=(CClaimNsupports&&) = default;
    CClaimNsupports& operator=(const CClaimNsupports&) = default;

    CClaimNsupports(CClaimValue claim, int64_t effectiveAmount, std::vector<CSupportValue> supports = {});

    bool IsNull() const;

    CClaimValue claim;
    int64_t effectiveAmount = 0;
    std::vector<CSupportValue> supports;
};

struct CClaimSupportToName
{
    CClaimSupportToName(std::string name, int nLastTakeoverHeight, std::vector<CClaimNsupports> claimsNsupports, std::vector<CSupportValue> unmatchedSupports);

    const CClaimNsupports& find(const CUint160& claimId) const;
    const CClaimNsupports& find(const std::string& partialId) const;

    const std::string name;
    const int nLastTakeoverHeight;
    const std::vector<CClaimNsupports> claimsNsupports;
    const std::vector<CSupportValue> unmatchedSupports;
};

struct CClaimTrieProofNode
{
    CClaimTrieProofNode(std::vector<std::pair<unsigned char, CUint256>> children, bool hasValue, CUint256 valHash);

    CClaimTrieProofNode() = default;
    CClaimTrieProofNode(CClaimTrieProofNode&&) = default;
    CClaimTrieProofNode(const CClaimTrieProofNode&) = default;
    CClaimTrieProofNode& operator=(CClaimTrieProofNode&&) = default;
    CClaimTrieProofNode& operator=(const CClaimTrieProofNode&) = default;

    std::vector<std::pair<unsigned char, CUint256>> children;
    bool hasValue;
    CUint256 valHash;
};

struct CClaimTrieProof
{
    CClaimTrieProof() = default;
    CClaimTrieProof(CClaimTrieProof&&) = default;
    CClaimTrieProof(const CClaimTrieProof&) = default;
    CClaimTrieProof& operator=(CClaimTrieProof&&) = default;
    CClaimTrieProof& operator=(const CClaimTrieProof&) = default;

    std::vector<std::pair<bool, CUint256>> pairs;
    std::vector<CClaimTrieProofNode> nodes;
    int nHeightOfLastTakeover = 0;
    bool hasValue = false;
    CTxOutPoint outPoint;
};

#endif // CLAIMTRIE_DATA_H
