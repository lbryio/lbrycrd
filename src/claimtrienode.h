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
        
     /*   
     * We don't use ADD_SERIALIZE_METHODS since we have to forward everything to the worker.
     */
    
    template<typename Stream>
    void Serialize(Stream &s, int nType, int nVersionDummy) const
    {
        m_Worker->Serialize(s, nType, nVersionDummy);
    }
    
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersionDummy)
    {
        m_Worker->Unserialize(s, nType, nVersionDummy);
    }    

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return m_Worker->GetSerializeSize(nType, nVersion);
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
