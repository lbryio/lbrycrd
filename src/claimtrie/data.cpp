
#include <data.h>

#include <algorithm>
#include <sstream>

CClaimValue::CClaimValue(COutPoint outPoint, uint160 claimId, int64_t nAmount, int nHeight, int nValidAtHeight)
    : outPoint(std::move(outPoint)), claimId(std::move(claimId)), nAmount(nAmount), nEffectiveAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight)
{
}

bool CClaimValue::operator<(const CClaimValue& other) const
{
    if (nEffectiveAmount < other.nEffectiveAmount)
        return true;
    if (nEffectiveAmount != other.nEffectiveAmount)
        return false;
    if (nHeight > other.nHeight)
        return true;
    if (nHeight != other.nHeight)
        return false;
    return outPoint != other.outPoint && !(outPoint < other.outPoint);
}

bool CClaimValue::operator==(const CClaimValue& other) const
{
    return outPoint == other.outPoint && claimId == other.claimId && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
}

bool CClaimValue::operator!=(const CClaimValue& other) const
{
    return !(*this == other);
}

std::string CClaimValue::ToString() const
{
    std::stringstream ss;
    ss  << "CClaimValue(" << outPoint.ToString()
        << ", " << claimId.ToString()
        << ", " << nAmount
        << ", " << nEffectiveAmount
        << ", " << nHeight
        << ", " << nValidAtHeight << ')';
    return ss.str();
}

CSupportValue::CSupportValue(COutPoint outPoint, uint160 supportedClaimId, int64_t nAmount, int nHeight, int nValidAtHeight)
    : outPoint(std::move(outPoint)), supportedClaimId(std::move(supportedClaimId)), nAmount(nAmount), nHeight(nHeight), nValidAtHeight(nValidAtHeight)
{
}

bool CSupportValue::operator==(const CSupportValue& other) const
{
    return outPoint == other.outPoint && supportedClaimId == other.supportedClaimId && nAmount == other.nAmount && nHeight == other.nHeight && nValidAtHeight == other.nValidAtHeight;
}

bool CSupportValue::operator!=(const CSupportValue& other) const
{
    return !(*this == other);
}

std::string CSupportValue::ToString() const
{
    std::stringstream ss;
    ss  << "CSupportValue(" << outPoint.ToString()
        << ", " << supportedClaimId.ToString()
        << ", " << nAmount
        << ", " << nHeight
        << ", " << nValidAtHeight << ')';
    return ss.str();
}

CNameOutPointHeightType::CNameOutPointHeightType(std::string name, COutPoint outPoint, int nValidHeight)
    : name(std::move(name)), outPoint(std::move(outPoint)), nValidHeight(nValidHeight)
{
}

CClaimNsupports::CClaimNsupports(CClaimValue claim, int64_t effectiveAmount, std::vector<CSupportValue> supports)
    : claim(std::move(claim)), effectiveAmount(effectiveAmount), supports(std::move(supports))
{
}

bool CClaimNsupports::IsNull() const
{
    return claim.claimId.IsNull();
}

CClaimSupportToName::CClaimSupportToName(std::string name, int nLastTakeoverHeight, std::vector<CClaimNsupports> claimsNsupports, std::vector<CSupportValue> unmatchedSupports)
    : name(std::move(name)), nLastTakeoverHeight(nLastTakeoverHeight), claimsNsupports(std::move(claimsNsupports)), unmatchedSupports(std::move(unmatchedSupports))
{
}

static const CClaimNsupports invalid;

const CClaimNsupports& CClaimSupportToName::find(const uint160& claimId) const
{
    auto it = std::find_if(claimsNsupports.begin(), claimsNsupports.end(), [&claimId](const CClaimNsupports& value) {
        return claimId == value.claim.claimId;
    });
    return it != claimsNsupports.end() ? *it : invalid;
}

const CClaimNsupports& CClaimSupportToName::find(const std::string& partialId) const
{
    std::string lowered(partialId);
    for (auto& c: lowered)
        c = std::tolower(c);

    auto it = std::find_if(claimsNsupports.begin(), claimsNsupports.end(), [&lowered](const CClaimNsupports& value) {
        return value.claim.claimId.GetHex().find(lowered) == 0;
    });
    return it != claimsNsupports.end() ? *it : invalid;
}

bool CClaimNsupports::operator<(const CClaimNsupports& other) const
{
    return claim < other.claim;
}

CClaimTrieProofNode::CClaimTrieProofNode(std::vector<std::pair<unsigned char, uint256>> children, bool hasValue, uint256 valHash)
    : children(std::move(children)), hasValue(hasValue), valHash(std::move(valHash))
{
}
