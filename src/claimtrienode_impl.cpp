#include <vector>
#include <map>
#include <utility>
#include <string>

#include "uint256.h"
#include "claimtrietypedefs.h"
#include "claimtrienode_impl.h"
#include "claimtrienode.h"
#include "claimtrie.h"
#include "coins.h"
#include "hash.h"

#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <algorithm>

CClaimTrieNode_Impl::CClaimTrieNode_Impl()
 : nHeightOfLastTakeover(0)
{
     
}
 
CClaimTrieNode_Impl::CClaimTrieNode_Impl(uint256 hash)
 : hash(hash), nHeightOfLastTakeover(0)
{
     
}

CClaimTrieNode_Impl::CClaimTrieNode_Impl(const CClaimTrieNode_Impl* other)
 : hash(other->hash), nHeightOfLastTakeover(other->nHeightOfLastTakeover), claims(other->claims), children(other->children)
{
}
 
uint256 CClaimTrieNode_Impl::getHash() const
{
    return hash;
}

void CClaimTrieNode_Impl::setHash(uint256 newHash)
{
    hash = newHash;
}

void CClaimTrieNode_Impl::setHeightOfLastTakeover(int newLastTakeover)
{
    nHeightOfLastTakeover = newLastTakeover;
}

const nodeMapType& CClaimTrieNode_Impl::getChildren() const
{
    return children;
}

nodeMapType& CClaimTrieNode_Impl::getChildren() 
{
    return children;
}

std::vector<CClaimValue>& CClaimTrieNode_Impl::getClaims()
{
    return claims;
}

const std::vector<CClaimValue>& CClaimTrieNode_Impl::getClaims() const
{
    return claims;
}

int CClaimTrieNode_Impl::getHeightOfLastTakeover() const
{
    return nHeightOfLastTakeover;
}

bool CClaimTrieNode_Impl::insertClaim(CClaimValue claim)
{
    LogPrintf("%s: Inserting %s:%d (amount: %d)  into the claim trie\n", __func__, claim.outPoint.hash.ToString(), claim.outPoint.n, claim.nAmount);
    claims.push_back(claim);
    return true;
}

bool CClaimTrieNode_Impl::removeClaim(const COutPoint& outPoint, CClaimValue& claim)
{
    LogPrintf("%s: Removing txid: %s, nOut: %d from the claim trie\n", __func__, outPoint.hash.ToString(), outPoint.n);

    std::vector<CClaimValue>::iterator itClaims;
    for (itClaims = claims.begin(); itClaims != claims.end(); ++itClaims)
    {
        if (itClaims->outPoint == outPoint)
        {
            std::swap(claim, *itClaims);
            break;
        }
    }
    if (itClaims != claims.end())
    {
        claims.erase(itClaims);
    }
    else
    {
        LogPrintf("CClaimTrieNode::%s() : asked to remove a claim that doesn't exist\n", __func__);
        LogPrintf("CClaimTrieNode::%s() : claims that do exist:\n", __func__);
        for (unsigned int i = 0; i < claims.size(); i++)
        {
            LogPrintf("\ttxhash: %s, nOut: %d:\n", claims[i].outPoint.hash.ToString(), claims[i].outPoint.n);
        }
        return false;
    }
    return true;
}

bool CClaimTrieNode_Impl::empty() const
{
    return children.empty() && claims.empty();
}

bool CClaimTrieNode_Impl::getBestClaim(CClaimValue& claim) const
{
    if (claims.empty())
    {
        return false;
    }
    else
    {
        claim = claims.front();
        return true;
    }
}

bool CClaimTrieNode_Impl::haveClaim(const COutPoint& outPoint) const
{
    for (std::vector<CClaimValue>::const_iterator itclaim = claims.begin(); itclaim != claims.end(); ++itclaim)
    {
        if (itclaim->outPoint == outPoint)
            return true;
    }
    return false;
}

void CClaimTrieNode_Impl::reorderClaims(supportMapEntryType& supports)
{
    std::vector<CClaimValue>::iterator itclaim;
    
    for (itclaim = claims.begin(); itclaim != claims.end(); ++itclaim)
    {
        itclaim->nEffectiveAmount = itclaim->nAmount;
    }

    for (supportMapEntryType::iterator itsupport = supports.begin(); itsupport != supports.end(); ++itsupport)
    {
        for (itclaim = claims.begin(); itclaim != claims.end(); ++itclaim)
        {
            if (itsupport->supportedClaimId == itclaim->claimId)
            {
                itclaim->nEffectiveAmount += itsupport->nAmount;
                break;
            }
        }
    }
    
    std::make_heap(claims.begin(), claims.end());
}

