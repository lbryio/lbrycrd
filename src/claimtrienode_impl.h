#ifndef BITCOIN_CLAIMTRIE_NODE_IMPL_H
#define BITCOIN_CLAIMTRIE_NODE_IMPL_H

#include "amount.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "dbwrapper.h"
#include "primitives/transaction.h"

class CClaimTrieNode_Impl
{
    friend class CClaimTrieNode;
    
private:
    CClaimTrieNode_Impl();
    CClaimTrieNode_Impl(uint256 hash);
    CClaimTrieNode_Impl(const CClaimTrieNode_Impl* other);
    uint256 hash;
    nodeMapType children;
    int nHeightOfLastTakeover;
    std::vector<CClaimValue> claims;
    
public:

    uint256 getHash() const;
    void setHash(uint256 newHash);

    const nodeMapType& getChildren() const;
    nodeMapType& getChildren();

    std::vector<CClaimValue>& getClaims();
    const std::vector<CClaimValue>& getClaims() const;

    int getHeightOfLastTakeover() const;
    void setHeightOfLastTakeover(int newLastTakeover);

    bool insertClaim(CClaimValue claim);
    bool removeClaim(const COutPoint& outPoint, CClaimValue& claim);
    bool getBestClaim(CClaimValue& claim) const;
    bool empty() const;
    bool haveClaim(const COutPoint& outPoint) const;
    void reorderClaims(supportMapEntryType& supports);
    
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(claims);
        READWRITE(nHeightOfLastTakeover);
    }
    
    bool operator==(const CClaimTrieNode_Impl& other) const
    {
        return hash == other.hash && claims == other.claims;
    }

    bool operator!=(const CClaimTrieNode_Impl& other) const
    {
        return !(*this == other);
    }

private:
    int m_Count;
};


#endif // BITCOIN_CLAIMTRIE_IMPL_H
