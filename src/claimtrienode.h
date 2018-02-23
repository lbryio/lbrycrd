#ifndef BITCOIN_CLAIMTRIE_NODE_H
#define BITCOIN_CLAIMTRIE_NODE_H

#include "amount.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "dbwrapper.h"
#include "primitives/transaction.h"

class CClaimTrieNode_Impl;

class CClaimTrieNode
{
public:
    CClaimTrieNode();
    CClaimTrieNode(uint256 hash);
    
	CClaimTrieNode(const CClaimTrieNode& Node);
    
	CClaimTrieNode& operator = (const CClaimTrieNode& Node);
    
    CClaimTrieNode clone() const;
    
	~CClaimTrieNode();

    uint256 getHash() const;    
    const nodeMapType& getChildren() const;
    nodeMapType& getChildren();
    
    std::vector<CClaimValue>& getClaims();
    const std::vector<CClaimValue>& getClaims() const;
    int getHeightOfLastTakeover() const;
    void setHeightOfLastTakeover(int newLastTakeover);
    void setHash(uint256 newHash);
    
    bool insertClaim(CClaimValue claim);
    bool removeClaim(const COutPoint& outPoint, CClaimValue& claim);
    bool getBestClaim(CClaimValue& claim) const;
    bool empty() const;
    bool haveClaim(const COutPoint& outPoint) const;
    void reorderClaims(supportMapEntryType& supports);
        
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        m_Worker->SerializationOp(s, ser_action, nType, nVersion);
    }
    
    bool operator==(const CClaimTrieNode_Impl& other) const
    {
        return m_Worker->operator==(other);
    }

    bool operator!=(const CClaimTrieNode_Impl& other) const
    {
        return m_Worker->operator!=(other);
    }

    bool operator==(const CClaimTrieNode& other) const
    {
        return m_Worker == other.m_Worker;
    }

    bool operator!=(const CClaimTrieNode& other) const
    {
        return m_Worker != other.m_Worker;
    }
    
private:
	CClaimTrieNode_Impl* m_Worker;
    
    CClaimTrieNode(const CClaimTrieNode_Impl* Worker);
   
	void Release();
    
    static int s_NumberOfAllocations;
    static int s_NumberOfDeletions;
};

#endif // BITCOIN_CLAIMTRIE_NODE_H
