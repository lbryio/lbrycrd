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

int CClaimTrieNode::s_NumberOfAllocations = 0;
int CClaimTrieNode::s_NumberOfDeletions = 0;

CClaimTrieNode::CClaimTrieNode()
	: m_Worker(new CClaimTrieNode_Impl)
{
	m_Worker->m_Count = 1;
    s_NumberOfAllocations ++;
}

CClaimTrieNode::CClaimTrieNode(uint256 hash)
: m_Worker(new CClaimTrieNode_Impl(hash))
{
	m_Worker->m_Count = 1;
    s_NumberOfAllocations ++;
}

CClaimTrieNode::CClaimTrieNode(const CClaimTrieNode& Node)
	: m_Worker(Node.m_Worker)
{
	m_Worker->m_Count++;
}

CClaimTrieNode::CClaimTrieNode(const CClaimTrieNode_Impl* Worker)
	: m_Worker(new CClaimTrieNode_Impl(Worker))
{
	m_Worker->m_Count = 1;
    s_NumberOfAllocations ++;
}

CClaimTrieNode CClaimTrieNode::clone() const
{
    return CClaimTrieNode(m_Worker);
}

CClaimTrieNode& CClaimTrieNode::operator = (const CClaimTrieNode& Node)
{
	CClaimTrieNode_Impl* temp = m_Worker;

	m_Worker = Node.m_Worker;
	m_Worker->m_Count++;

	temp->m_Count--;

    if (temp->m_Count <= 0)
    {
        delete temp;
        s_NumberOfDeletions ++;
    }
    
	return *this;
}

CClaimTrieNode::~CClaimTrieNode()
{
	m_Worker->m_Count--;
	Release();
}

void CClaimTrieNode::Release()
{
	if (m_Worker->m_Count <= 0)
	{
		delete m_Worker;
        s_NumberOfDeletions ++;
	}
}

bool CClaimTrieNode::empty() const
{
 return m_Worker->empty();
 }

 uint256 CClaimTrieNode::getHash() const
 {
     return m_Worker->getHash();
 }
 
 void CClaimTrieNode::setHash(uint256 newHash)
 {
    static int count = 0;
    m_Worker->setHash(newHash);
    std::cout << count++ << ":" << newHash.GetHex() << std::endl;
 }
 
void CClaimTrieNode::setHeightOfLastTakeover(int newLastTakeover)
{
    m_Worker->setHeightOfLastTakeover(newLastTakeover);
}

 
 const nodeMapType& CClaimTrieNode::getChildren() const
 {
     return m_Worker->getChildren();
 }
 
 nodeMapType& CClaimTrieNode::getChildren()
 {
     return m_Worker->getChildren();
 }
 
std::vector<CClaimValue>& CClaimTrieNode::getClaims()
{
    return m_Worker->getClaims();
}

const std::vector<CClaimValue>& CClaimTrieNode::getClaims() const
{
    return m_Worker->getClaims();
}

int CClaimTrieNode::getHeightOfLastTakeover() const
{
    return m_Worker->getHeightOfLastTakeover();
}
  
bool CClaimTrieNode::insertClaim(CClaimValue claim)
{
    return m_Worker->insertClaim(claim);
}

bool CClaimTrieNode::removeClaim(const COutPoint& outPoint, CClaimValue& claim)
{
    return m_Worker->removeClaim(outPoint, claim);
}

bool CClaimTrieNode::getBestClaim(CClaimValue& claim) const
{
    return m_Worker->getBestClaim(claim);
}

bool CClaimTrieNode::haveClaim(const COutPoint& outPoint) const
{
    return m_Worker->haveClaim(outPoint);
}

void CClaimTrieNode::reorderClaims(supportMapEntryType& supports)
{
    m_Worker->reorderClaims(supports);
}

