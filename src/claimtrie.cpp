#include "claimtrie.h"
#include "coins.h"
#include "hash.h"

#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <algorithm>

std::vector<unsigned char> heightToVch(int n)
{
    std::vector<unsigned char> vchHeight;
    vchHeight.resize(8);
    vchHeight[0] = 0;
    vchHeight[1] = 0;
    vchHeight[2] = 0;
    vchHeight[3] = 0;
    vchHeight[4] = n >> 24;
    vchHeight[5] = n >> 16;
    vchHeight[6] = n >> 8;
    vchHeight[7] = n;
    return vchHeight;
}

uint256 getValueHash(COutPoint outPoint, int nHeightOfLastTakeover)
{
    CHash256 txHasher;
    txHasher.Write(outPoint.hash.begin(), outPoint.hash.size());
    std::vector<unsigned char> vchtxHash(txHasher.OUTPUT_SIZE);
    txHasher.Finalize(&(vchtxHash[0]));
        
    CHash256 nOutHasher;
    std::stringstream ss;
    ss << outPoint.n;
    std::string snOut = ss.str();
    nOutHasher.Write((unsigned char*) snOut.data(), snOut.size());
    std::vector<unsigned char> vchnOutHash(nOutHasher.OUTPUT_SIZE);
    nOutHasher.Finalize(&(vchnOutHash[0]));

    CHash256 takeoverHasher;
    std::vector<unsigned char> vchTakeoverHeightToHash = heightToVch(nHeightOfLastTakeover);
    takeoverHasher.Write(vchTakeoverHeightToHash.data(), vchTakeoverHeightToHash.size());
    std::vector<unsigned char> vchTakeoverHash(takeoverHasher.OUTPUT_SIZE);
    takeoverHasher.Finalize(&(vchTakeoverHash[0]));

    CHash256 hasher;
    hasher.Write(vchtxHash.data(), vchtxHash.size());
    hasher.Write(vchnOutHash.data(), vchnOutHash.size());
    hasher.Write(vchTakeoverHash.data(), vchTakeoverHash.size());
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Finalize(&(vchHash[0]));
    
    uint256 valueHash(vchHash);
    return valueHash;
}

bool CClaimTrieNode::insertClaim(CClaimValue claim)
{
    LogPrintf("%s: Inserting %s:%d (amount: %d)  into the claim trie\n", __func__, claim.outPoint.hash.ToString(), claim.outPoint.n, claim.nAmount);
    claims.push_back(claim);
    return true;
}

bool CClaimTrieNode::removeClaim(const COutPoint& outPoint, CClaimValue& claim)
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

bool CClaimTrieNode::getBestClaim(CClaimValue& claim) const
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

bool CClaimTrieNode::haveClaim(const COutPoint& outPoint) const
{
    for (std::vector<CClaimValue>::const_iterator itclaim = claims.begin(); itclaim != claims.end(); ++itclaim)
    {
        if (itclaim->outPoint == outPoint)
            return true;
    }
    return false;
}

void CClaimTrieNode::reorderClaims(supportMapEntryType& supports)
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

uint256 CClaimTrie::getMerkleHash()
{
    return root.hash;
}

bool CClaimTrie::empty() const
{
    return root.empty();
}

template<typename K> bool CClaimTrie::keyTypeEmpty(char keyType, K& dummy) const
{
    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CDBWrapper*>(&db)->NewIterator());
    pcursor->SeekToFirst();
    
    while (pcursor->Valid())
    {
        std::pair<char, K> key;
        if (pcursor->GetKey(key))
        {
            if (key.first == keyType)
            {
                return false;
            }
        }
        else
        {
            break;
        }
        pcursor->Next();
    }
    return true;
}

bool CClaimTrie::queueEmpty() const
{
    for (claimQueueType::const_iterator itRow = dirtyQueueRows.begin(); itRow != dirtyQueueRows.end(); ++itRow)
    {
        if (!itRow->second.empty())
            return false;
    }
    int dummy;
    return keyTypeEmpty(CLAIM_QUEUE_ROW, dummy);
}

bool CClaimTrie::expirationQueueEmpty() const
{
    for (expirationQueueType::const_iterator itRow = dirtyExpirationQueueRows.begin(); itRow != dirtyExpirationQueueRows.end(); ++itRow)
    {
        if (!itRow->second.empty())
            return false;
    }
    int dummy;
    return keyTypeEmpty(EXP_QUEUE_ROW, dummy);
}

bool CClaimTrie::supportEmpty() const
{
    for (supportMapType::const_iterator itNode = dirtySupportNodes.begin(); itNode != dirtySupportNodes.end(); ++itNode)
    {
        if (!itNode->second.empty())
            return false;
    }
    std::string dummy;
    return keyTypeEmpty(SUPPORT, dummy);
}

bool CClaimTrie::supportQueueEmpty() const
{
    for (supportQueueType::const_iterator itRow = dirtySupportQueueRows.begin(); itRow != dirtySupportQueueRows.end(); ++itRow)
    {
        if (!itRow->second.empty())
            return false;
    }
    int dummy;
    return keyTypeEmpty(SUPPORT_QUEUE_ROW, dummy);
}

void CClaimTrie::setExpirationTime(int t)
{
    nExpirationTime = t;
}

void CClaimTrie::clear()
{
    clear(&root);
}

void CClaimTrie::clear(CClaimTrieNode* current)
{
    for (nodeMapType::const_iterator itchildren = current->children.begin(); itchildren != current->children.end(); ++itchildren)
    {
        clear(itchildren->second);
        delete itchildren->second;
    }
}

bool CClaimTrie::haveClaim(const std::string& name, const COutPoint& outPoint) const
{
    const CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end())
            return false;
        current = itchildren->second;
    }
    return current->haveClaim(outPoint);
}

bool CClaimTrie::haveSupport(const std::string& name, const COutPoint& outPoint) const
{
    supportMapEntryType node;
    if (!getSupportNode(name, node))
    {
        return false;
    }
    for (supportMapEntryType::const_iterator itnode = node.begin(); itnode != node.end(); ++itnode)
    {
        if (itnode->outPoint == outPoint)
            return true;
    }
    return false;
}

bool CClaimTrie::haveClaimInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    queueNameRowType nameRow;
    if (!getQueueNameRow(name, nameRow))
    {
        return false;
    }
    queueNameRowType::const_iterator itNameRow;
    for (itNameRow = nameRow.begin(); itNameRow != nameRow.end(); ++itNameRow)
    {
        if (itNameRow->outPoint == outPoint)
        {
            nValidAtHeight = itNameRow->nHeight;
            break;
        }
    }
    if (itNameRow == nameRow.end())
    {
        return false;
    }
    claimQueueRowType row;
    if (getQueueRow(nValidAtHeight, row))
    {
        for (claimQueueRowType::const_iterator itRow = row.begin(); itRow != row.end(); ++itRow)
        {
            if (itRow->first == name && itRow->second.outPoint == outPoint)
            {
                if (itRow->second.nValidAtHeight != nValidAtHeight)
                {
                    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, itRow->second.nValidAtHeight, nCurrentHeight);
                }
                return true;
            }
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nCurrentHeight);
    return false;
}

bool CClaimTrie::haveSupportInQueue(const std::string& name, const COutPoint& outPoint, int& nValidAtHeight) const
{
    queueNameRowType nameRow;
    if (!getSupportQueueNameRow(name, nameRow))
    {
        return false;
    }
    queueNameRowType::const_iterator itNameRow;
    for (itNameRow = nameRow.begin(); itNameRow != nameRow.end(); ++itNameRow)
    {
        if (itNameRow->outPoint == outPoint)
        {
            nValidAtHeight = itNameRow->nHeight;
            break;
        }
    }
    if (itNameRow == nameRow.end())
    {
        return false;
    }
    supportQueueRowType row;
    if (getSupportQueueRow(nValidAtHeight, row))
    {
        for (supportQueueRowType::const_iterator itRow = row.begin(); itRow != row.end(); ++itRow)
        {
            if (itRow->first == name && itRow->second.outPoint == outPoint)
            {
                if (itRow->second.nValidAtHeight != nValidAtHeight)
                {
                    LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, itRow->second.nValidAtHeight, nCurrentHeight);
                }
                return true;
            }
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nValidAtHeight, nCurrentHeight);
    return false;
}

unsigned int CClaimTrie::getTotalNamesInTrie() const
{
    if (empty())
        return 0;
    const CClaimTrieNode* current = &root;
    return getTotalNamesRecursive(current);
}

unsigned int CClaimTrie::getTotalNamesRecursive(const CClaimTrieNode* current) const
{
    unsigned int names_in_subtrie = 0;
    if (!(current->claims.empty()))
        names_in_subtrie += 1;
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it)
    {
        names_in_subtrie += getTotalNamesRecursive(it->second);
    }
    return names_in_subtrie;
}

unsigned int CClaimTrie::getTotalClaimsInTrie() const
{
    if (empty())
        return 0;
    const CClaimTrieNode* current = &root;
    return getTotalClaimsRecursive(current);
}

unsigned int CClaimTrie::getTotalClaimsRecursive(const CClaimTrieNode* current) const
{
    unsigned int claims_in_subtrie = current->claims.size();
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it)
    {
        claims_in_subtrie += getTotalClaimsRecursive(it->second);
    }
    return claims_in_subtrie;
}

CAmount CClaimTrie::getTotalValueOfClaimsInTrie(bool fControllingOnly) const
{
    if (empty())
        return 0;
    const CClaimTrieNode* current = &root;
    return getTotalValueOfClaimsRecursive(current, fControllingOnly);
}

CAmount CClaimTrie::getTotalValueOfClaimsRecursive(const CClaimTrieNode* current, bool fControllingOnly) const
{
    CAmount value_in_subtrie = 0;
    for (std::vector<CClaimValue>::const_iterator itclaim = current->claims.begin(); itclaim != current->claims.end(); ++itclaim)
    {
        value_in_subtrie += itclaim->nAmount;
        if (fControllingOnly)
            break;
    }
    for (nodeMapType::const_iterator itchild = current->children.begin(); itchild != current->children.end(); ++itchild)
     {
         value_in_subtrie += getTotalValueOfClaimsRecursive(itchild->second, fControllingOnly);
     }
     return value_in_subtrie;
}

bool CClaimTrie::recursiveFlattenTrie(const std::string& name, const CClaimTrieNode* current, std::vector<namedNodeType>& nodes) const
{
    namedNodeType node(name, *current);
    nodes.push_back(node);
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it)
    {
        std::stringstream ss;
        ss << name << it->first;
        if (!recursiveFlattenTrie(ss.str(), it->second, nodes))
            return false;
    }
    return true;
}

std::vector<namedNodeType> CClaimTrie::flattenTrie() const
{
    std::vector<namedNodeType> nodes;
    if (!recursiveFlattenTrie("", &root, nodes))
        LogPrintf("%s: Something went wrong flattening the trie", __func__);
    return nodes;
}

const CClaimTrieNode* CClaimTrie::getNodeForName(const std::string& name) const
{
    const CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end())
            return NULL;
        current = itchildren->second;
    }
    return current;
}

bool CClaimTrie::getInfoForName(const std::string& name, CClaimValue& claim) const
{
    const CClaimTrieNode* current = getNodeForName(name);
    if (current)
    {
        return current->getBestClaim(claim);
    }
    return false;
}

bool CClaimTrie::getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const
{
    const CClaimTrieNode* current = getNodeForName(name);
    if (current && !current->claims.empty())
    {
        lastTakeoverHeight = current->nHeightOfLastTakeover;
        return true;
    }
    return false;
}

claimsForNameType CClaimTrie::getClaimsForName(const std::string& name) const
{
    std::vector<CClaimValue> claims;
    std::vector<CSupportValue> supports;
    int nLastTakeoverHeight = 0;
    const CClaimTrieNode* current = getNodeForName(name);
    if (current)
    {
        if (!current->claims.empty())
        {
            nLastTakeoverHeight = current->nHeightOfLastTakeover;
        }
        for (std::vector<CClaimValue>::const_iterator itClaims = current->claims.begin(); itClaims != current->claims.end(); ++itClaims)
        {
            claims.push_back(*itClaims);
        }
    }
    supportMapEntryType supportNode;
    if (getSupportNode(name, supportNode))
    {
        for (std::vector<CSupportValue>::const_iterator itSupports = supportNode.begin(); itSupports != supportNode.end(); ++itSupports)
        {
            supports.push_back(*itSupports);
        }
    }
    queueNameRowType namedClaimRow;
    if (getQueueNameRow(name, namedClaimRow))
    {
        for (queueNameRowType::const_iterator itClaimsForName = namedClaimRow.begin(); itClaimsForName != namedClaimRow.end(); ++itClaimsForName)
        {
            claimQueueRowType claimRow;
            if (getQueueRow(itClaimsForName->nHeight, claimRow))
            {
                for (claimQueueRowType::const_iterator itClaimRow = claimRow.begin(); itClaimRow != claimRow.end(); ++itClaimRow)
                 {
                     if (itClaimRow->first == name && itClaimRow->second.outPoint == itClaimsForName->outPoint)
                     {
                         claims.push_back(itClaimRow->second);
                         break;
                     }
                 }
            }
        }
    }
    queueNameRowType namedSupportRow;
    if (getSupportQueueNameRow(name, namedSupportRow))
    {
        for (queueNameRowType::const_iterator itSupportsForName = namedSupportRow.begin(); itSupportsForName != namedSupportRow.end(); ++itSupportsForName)
        {
            supportQueueRowType supportRow;
            if (getSupportQueueRow(itSupportsForName->nHeight, supportRow))
            {
                for (supportQueueRowType::const_iterator itSupportRow = supportRow.begin(); itSupportRow != supportRow.end(); ++itSupportRow)
                {
                    if (itSupportRow->first == name && itSupportRow->second.outPoint == itSupportsForName->outPoint)
                    {
                        supports.push_back(itSupportRow->second);
                        break;
                    }
                }
            }
        }
    }
    claimsForNameType allClaims(claims, supports, nLastTakeoverHeight);
    return allClaims;
}

bool CClaimTrie::checkConsistency() const
{
    if (empty())
        return true;
    return recursiveCheckConsistency(&root);
}

bool CClaimTrie::recursiveCheckConsistency(const CClaimTrieNode* node) const
{
    std::vector<unsigned char> vchToHash;

    for (nodeMapType::const_iterator it = node->children.begin(); it != node->children.end(); ++it)
    {
        if (recursiveCheckConsistency(it->second))
        {
            vchToHash.push_back(it->first);
            vchToHash.insert(vchToHash.end(), it->second->hash.begin(), it->second->hash.end());
        }
        else
            return false;
    }

    CClaimValue claim;
    bool hasClaim = node->getBestClaim(claim);

    if (hasClaim)
    {
        uint256 valueHash = getValueHash(claim.outPoint, node->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    CHash256 hasher;
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Write(vchToHash.data(), vchToHash.size());
    hasher.Finalize(&(vchHash[0]));
    uint256 calculatedHash(vchHash);
    return calculatedHash == node->hash;
}

bool CClaimTrie::getQueueRow(int nHeight, claimQueueRowType& row) const
{
    claimQueueType::const_iterator itQueueRow = dirtyQueueRows.find(nHeight);
    if (itQueueRow != dirtyQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(CLAIM_QUEUE_ROW, nHeight), row);
}

bool CClaimTrie::getQueueNameRow(const std::string& name, queueNameRowType& row) const
{
    queueNameType::const_iterator itQueueNameRow = dirtyQueueNameRows.find(name);
    if (itQueueNameRow != dirtyQueueNameRows.end())
    {
        row = itQueueNameRow->second;
        return true;
    }
    return db.Read(std::make_pair(CLAIM_QUEUE_NAME_ROW, name), row);
}

bool CClaimTrie::getExpirationQueueRow(int nHeight, expirationQueueRowType& row) const
{
    expirationQueueType::const_iterator itQueueRow = dirtyExpirationQueueRows.find(nHeight);
    if (itQueueRow != dirtyExpirationQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(EXP_QUEUE_ROW, nHeight), row);
}

void CClaimTrie::updateQueueRow(int nHeight, claimQueueRowType& row)
{
    claimQueueType::iterator itQueueRow = dirtyQueueRows.find(nHeight);
    if (itQueueRow == dirtyQueueRows.end())
    {
        claimQueueRowType newRow;
        std::pair<claimQueueType::iterator, bool> ret;
        ret = dirtyQueueRows.insert(std::pair<int, claimQueueRowType >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateQueueNameRow(const std::string& name, queueNameRowType& row)
{
    queueNameType::iterator itQueueRow = dirtyQueueNameRows.find(name);
    if (itQueueRow == dirtyQueueNameRows.end())
    {
        queueNameRowType newRow;
        std::pair<queueNameType::iterator, bool> ret;
        ret = dirtyQueueNameRows.insert(std::pair<std::string, queueNameRowType>(name, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateExpirationRow(int nHeight, expirationQueueRowType& row)
{
    expirationQueueType::iterator itQueueRow = dirtyExpirationQueueRows.find(nHeight);
    if (itQueueRow == dirtyExpirationQueueRows.end())
    {
        expirationQueueRowType newRow;
        std::pair<expirationQueueType::iterator, bool> ret;
        ret = dirtyExpirationQueueRows.insert(std::pair<int, expirationQueueRowType >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateSupportMap(const std::string& name, supportMapEntryType& node)
{
    supportMapType::iterator itNode = dirtySupportNodes.find(name);
    if (itNode == dirtySupportNodes.end())
    {
        supportMapEntryType newNode;
        std::pair<supportMapType::iterator, bool> ret;
        ret = dirtySupportNodes.insert(std::pair<std::string, supportMapEntryType>(name, newNode));
        assert(ret.second);
        itNode = ret.first;
    }
    itNode->second.swap(node);
}

void CClaimTrie::updateSupportQueue(int nHeight, supportQueueRowType& row)
{
    supportQueueType::iterator itQueueRow = dirtySupportQueueRows.find(nHeight);
    if (itQueueRow == dirtySupportQueueRows.end())
    {
        supportQueueRowType newRow;
        std::pair<supportQueueType::iterator, bool> ret;
        ret = dirtySupportQueueRows.insert(std::pair<int, supportQueueRowType >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateSupportNameQueue(const std::string& name, queueNameRowType& row)
{
    queueNameType::iterator itQueueRow = dirtySupportQueueNameRows.find(name);
    if (itQueueRow == dirtySupportQueueNameRows.end())
    {
        queueNameRowType newRow;
        std::pair<queueNameType::iterator, bool> ret;
        ret = dirtySupportQueueNameRows.insert(std::pair<std::string, queueNameRowType>(name, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateSupportExpirationQueue(int nHeight, expirationQueueRowType& row)
{
    expirationQueueType::iterator itQueueRow = dirtySupportExpirationQueueRows.find(nHeight);
    if (itQueueRow == dirtySupportExpirationQueueRows.end())
    {
        expirationQueueRowType newRow;
        std::pair<expirationQueueType::iterator, bool> ret;
        ret = dirtySupportExpirationQueueRows.insert(std::pair<int, expirationQueueRowType >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

bool CClaimTrie::getSupportNode(std::string name, supportMapEntryType& node) const
{
    supportMapType::const_iterator itNode = dirtySupportNodes.find(name);
    if (itNode != dirtySupportNodes.end())
    {
        node = itNode->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT, name), node);
}

bool CClaimTrie::getSupportQueueRow(int nHeight, supportQueueRowType& row) const
{
    supportQueueType::const_iterator itQueueRow = dirtySupportQueueRows.find(nHeight);
    if (itQueueRow != dirtySupportQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT_QUEUE_ROW, nHeight), row);
}

bool CClaimTrie::getSupportQueueNameRow(const std::string& name, queueNameRowType& row) const
{
    queueNameType::const_iterator itQueueNameRow = dirtySupportQueueNameRows.find(name);
    if (itQueueNameRow != dirtySupportQueueNameRows.end())
    {
        row = itQueueNameRow->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT_QUEUE_NAME_ROW, name), row);
}

bool CClaimTrie::getSupportExpirationQueueRow(int nHeight, expirationQueueRowType& row) const
{
    expirationQueueType::const_iterator itQueueRow = dirtySupportExpirationQueueRows.find(nHeight);
    if (itQueueRow != dirtySupportExpirationQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT_EXP_QUEUE_ROW, nHeight), row);
}

bool CClaimTrie::update(nodeCacheType& cache, hashMapType& hashes, std::map<std::string, int>& takeoverHeights, const uint256& hashBlockIn, claimQueueType& queueCache, queueNameType& queueNameCache, expirationQueueType& expirationQueueCache, int nNewHeight, supportMapType& supportCache, supportQueueType& supportQueueCache, queueNameType& supportQueueNameCache, expirationQueueType& supportExpirationQueueCache)
{
    for (nodeCacheType::iterator itcache = cache.begin(); itcache != cache.end(); ++itcache)
    {
        if (!updateName(itcache->first, itcache->second))
            return false;
    }
    for (hashMapType::iterator ithash = hashes.begin(); ithash != hashes.end(); ++ithash)
    {
        if (!updateHash(ithash->first, ithash->second))
            return false;
    }
    for (std::map<std::string, int>::iterator itheight = takeoverHeights.begin(); itheight != takeoverHeights.end(); ++itheight)
    {
        if (!updateTakeoverHeight(itheight->first, itheight->second))
            return false;
    }
    for (claimQueueType::iterator itQueueCacheRow = queueCache.begin(); itQueueCacheRow != queueCache.end(); ++itQueueCacheRow)
    {
        updateQueueRow(itQueueCacheRow->first, itQueueCacheRow->second);
    }
    for (queueNameType::iterator itQueueNameCacheRow = queueNameCache.begin(); itQueueNameCacheRow != queueNameCache.end(); ++itQueueNameCacheRow)
    {
        updateQueueNameRow(itQueueNameCacheRow->first, itQueueNameCacheRow->second);
    }
    for (expirationQueueType::iterator itExpirationRow = expirationQueueCache.begin(); itExpirationRow != expirationQueueCache.end(); ++itExpirationRow)
    {
        updateExpirationRow(itExpirationRow->first, itExpirationRow->second);
    }
    for (supportMapType::iterator itSupportCache = supportCache.begin(); itSupportCache != supportCache.end(); ++itSupportCache)
    {
        updateSupportMap(itSupportCache->first, itSupportCache->second);
    }
    for (supportQueueType::iterator itSupportQueue = supportQueueCache.begin(); itSupportQueue != supportQueueCache.end(); ++itSupportQueue)
    {
        updateSupportQueue(itSupportQueue->first, itSupportQueue->second);
    }
    for (queueNameType::iterator itSupportNameQueue = supportQueueNameCache.begin(); itSupportNameQueue != supportQueueNameCache.end(); ++itSupportNameQueue)
    {
        updateSupportNameQueue(itSupportNameQueue->first, itSupportNameQueue->second);
    }
    for (expirationQueueType::iterator itSupportExpirationQueue = supportExpirationQueueCache.begin(); itSupportExpirationQueue != supportExpirationQueueCache.end(); ++itSupportExpirationQueue)
    {
        updateSupportExpirationQueue(itSupportExpirationQueue->first, itSupportExpirationQueue->second);
    }
    hashBlock = hashBlockIn;
    nCurrentHeight = nNewHeight;
    return true;
}

void CClaimTrie::markNodeDirty(const std::string &name, CClaimTrieNode* node)
{
    std::pair<nodeCacheType::iterator, bool> ret;
    ret = dirtyNodes.insert(std::pair<std::string, CClaimTrieNode*>(name, node));
    if (ret.second == false)
        ret.first->second = node;
}

bool CClaimTrie::updateName(const std::string &name, CClaimTrieNode* updatedNode)
{
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end())
        {
            if (itname + 1 == name.end())
            {
                CClaimTrieNode* newNode = new CClaimTrieNode();
                current->children[*itname] = newNode;
                current = newNode;
            }
            else
                return false;
        }
        else
        {
            current = itchild->second;
        }
    }
    assert(current != NULL);
    current->claims.swap(updatedNode->claims);
    markNodeDirty(name, current);
    for (nodeMapType::iterator itchild = current->children.begin(); itchild != current->children.end();)
    {
        nodeMapType::iterator itupdatechild = updatedNode->children.find(itchild->first);
        if (itupdatechild == updatedNode->children.end())
        {
            // This character has apparently been deleted, so delete
            // all descendents from this child.
            std::stringstream ss;
            ss << name << itchild->first;
            std::string newName = ss.str();
            if (!recursiveNullify(itchild->second, newName))
                return false;
            current->children.erase(itchild++);
        }
        else
            ++itchild;
    }
    return true;
}

bool CClaimTrie::recursiveNullify(CClaimTrieNode* node, std::string& name)
{
    assert(node != NULL);
    for (nodeMapType::iterator itchild = node->children.begin(); itchild != node->children.end(); ++itchild)
    {
        std::stringstream ss;
        ss << name << itchild->first;
        std::string newName = ss.str();
        if (!recursiveNullify(itchild->second, newName))
            return false;
    }
    node->children.clear();
    markNodeDirty(name, NULL);
    delete node;
    return true;
}

bool CClaimTrie::updateHash(const std::string& name, uint256& hash)
{
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end())
            return false;
        current = itchild->second;
    }
    assert(current != NULL);
    current->hash = hash;
    markNodeDirty(name, current);
    return true;
}

bool CClaimTrie::updateTakeoverHeight(const std::string& name, int nTakeoverHeight)
{
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end())
            return false;
        current = itchild->second;
    }
    assert(current != NULL);
    current->nHeightOfLastTakeover = nTakeoverHeight;
    markNodeDirty(name, current);
    return true;
}

void CClaimTrie::BatchWriteNode(CDBBatch& batch, const std::string& name, const CClaimTrieNode* pNode) const
{
    uint32_t num_claims = 0;
    if (pNode)
        num_claims = pNode->claims.size();
    LogPrintf("%s: Writing %s to disk with %d claims\n", __func__, name, num_claims);
    if (pNode)
        batch.Write(std::make_pair(TRIE_NODE, name), *pNode);
    else
        batch.Erase(std::make_pair(TRIE_NODE, name));
}

void CClaimTrie::BatchWriteQueueRows(CDBBatch& batch)
{
    for (claimQueueType::iterator itQueue = dirtyQueueRows.begin(); itQueue != dirtyQueueRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(CLAIM_QUEUE_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(CLAIM_QUEUE_ROW, itQueue->first), itQueue->second);
        }
    }
}

void CClaimTrie::BatchWriteQueueNameRows(CDBBatch& batch)
{
    for (queueNameType::iterator itQueue = dirtyQueueNameRows.begin(); itQueue != dirtyQueueNameRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(CLAIM_QUEUE_NAME_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(CLAIM_QUEUE_NAME_ROW, itQueue->first), itQueue->second);
        }
    }
}

void CClaimTrie::BatchWriteExpirationQueueRows(CDBBatch& batch)
{
    for (expirationQueueType::iterator itQueue = dirtyExpirationQueueRows.begin(); itQueue != dirtyExpirationQueueRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(EXP_QUEUE_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(EXP_QUEUE_ROW, itQueue->first), itQueue->second);
        }
    }
}

void CClaimTrie::BatchWriteSupportNodes(CDBBatch& batch)
{
    for (supportMapType::iterator itSupport = dirtySupportNodes.begin(); itSupport != dirtySupportNodes.end(); ++itSupport)
    {
        if (itSupport->second.empty())
        {
            batch.Erase(std::make_pair(SUPPORT, itSupport->first));
        }
        else
        {
            batch.Write(std::make_pair(SUPPORT, itSupport->first), itSupport->second);
        }
    }
}

void CClaimTrie::BatchWriteSupportQueueRows(CDBBatch& batch)
{
    for (supportQueueType::iterator itQueue = dirtySupportQueueRows.begin(); itQueue != dirtySupportQueueRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(SUPPORT_QUEUE_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(SUPPORT_QUEUE_ROW, itQueue->first), itQueue->second);
        }
    }
}

void CClaimTrie::BatchWriteSupportQueueNameRows(CDBBatch& batch)
{
    for (queueNameType::iterator itQueue = dirtySupportQueueNameRows.begin(); itQueue != dirtySupportQueueNameRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(SUPPORT_QUEUE_NAME_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(SUPPORT_QUEUE_NAME_ROW, itQueue->first), itQueue->second);
        }
    }
}

void CClaimTrie::BatchWriteSupportExpirationQueueRows(CDBBatch& batch)
{
    for (expirationQueueType::iterator itQueue = dirtySupportExpirationQueueRows.begin(); itQueue != dirtySupportExpirationQueueRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(SUPPORT_EXP_QUEUE_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(SUPPORT_EXP_QUEUE_ROW, itQueue->first), itQueue->second);
        }
    }
}

bool CClaimTrie::WriteToDisk()
{
    CDBBatch batch(&db.GetObfuscateKey());
    for (nodeCacheType::iterator itcache = dirtyNodes.begin(); itcache != dirtyNodes.end(); ++itcache)
        BatchWriteNode(batch, itcache->first, itcache->second);
    dirtyNodes.clear();
    BatchWriteQueueRows(batch);
    dirtyQueueRows.clear();
    BatchWriteQueueNameRows(batch);
    dirtyQueueNameRows.clear();
    BatchWriteExpirationQueueRows(batch);
    dirtyExpirationQueueRows.clear();
    BatchWriteSupportNodes(batch);
    dirtySupportNodes.clear();
    BatchWriteSupportQueueRows(batch);
    dirtySupportQueueRows.clear();
    BatchWriteSupportQueueNameRows(batch);
    dirtySupportQueueNameRows.clear();
    BatchWriteSupportExpirationQueueRows(batch);
    dirtySupportExpirationQueueRows.clear();
    batch.Write(HASH_BLOCK, hashBlock);
    batch.Write(CURRENT_HEIGHT, nCurrentHeight);
    return db.WriteBatch(batch);
}

bool CClaimTrie::InsertFromDisk(const std::string& name, CClaimTrieNode* node)
{
    if (name.size() == 0)
    {
        root = *node;
        return true;
    }
    CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname + 1 != name.end(); ++itname)
    {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end())
            return false;
        current = itchild->second;
    }
    current->children[name[name.size()-1]] = node;
    return true;
}

bool CClaimTrie::ReadFromDisk(bool check)
{
    if (!db.Read(HASH_BLOCK, hashBlock))
        LogPrintf("%s: Couldn't read the best block's hash\n", __func__);
    if (!db.Read(CURRENT_HEIGHT, nCurrentHeight))
        LogPrintf("%s: Couldn't read the current height\n", __func__);
    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CDBWrapper*>(&db)->NewIterator());
    pcursor->SeekToFirst();
    
    while (pcursor->Valid())
    {
        std::pair<char, std::string> key;
        if (pcursor->GetKey(key))
        {
            if (key.first == TRIE_NODE)
            {
                CClaimTrieNode* node = new CClaimTrieNode();
                if (pcursor->GetValue(*node))
                {
                    if (!InsertFromDisk(key.second, node))
                    {
                        return error("%s(): error restoring claim trie from disk", __func__);
                    }
                }
                else
                {
                    return error("%s(): error reading claim trie from disk", __func__);
                }
            }
        }
        pcursor->Next();
    }
    if (check)
    {
        LogPrintf("Checking Claim trie consistency...");
        if (checkConsistency())
        {
            LogPrintf("consistent\n");
            return true;
        }
        LogPrintf("inconsistent!\n");
        return false;
    }
    return true;
}

bool CClaimTrieCache::recursiveComputeMerkleHash(CClaimTrieNode* tnCurrent, std::string sPos) const
{
    if (sPos == "" && tnCurrent->empty())
    {
        cacheHashes[""] = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
        return true;
    }
    std::vector<unsigned char> vchToHash;
    nodeCacheType::iterator cachedNode;


    for (nodeMapType::iterator it = tnCurrent->children.begin(); it != tnCurrent->children.end(); ++it)
    {
        std::stringstream ss;
        ss << it->first;
        std::string sNextPos = sPos + ss.str();
        if (dirtyHashes.count(sNextPos) != 0)
        {
            // the child might be in the cache, so look for it there
            cachedNode = cache.find(sNextPos);
            if (cachedNode != cache.end())
                recursiveComputeMerkleHash(cachedNode->second, sNextPos);
            else
                recursiveComputeMerkleHash(it->second, sNextPos);
        }
        vchToHash.push_back(it->first);
        hashMapType::iterator ithash = cacheHashes.find(sNextPos);
        if (ithash != cacheHashes.end())
        {
            vchToHash.insert(vchToHash.end(), ithash->second.begin(), ithash->second.end());
        }
        else
        {
            vchToHash.insert(vchToHash.end(), it->second->hash.begin(), it->second->hash.end());
        }
    }
    
    CClaimValue claim;
    bool hasClaim = tnCurrent->getBestClaim(claim);

    if (hasClaim)
    {
        int nHeightOfLastTakeover;
        assert(getLastTakeoverForName(sPos, nHeightOfLastTakeover));
        uint256 valueHash = getValueHash(claim.outPoint, nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), valueHash.begin(), valueHash.end());
    }

    CHash256 hasher;
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Write(vchToHash.data(), vchToHash.size());
    hasher.Finalize(&(vchHash[0]));
    cacheHashes[sPos] = uint256(vchHash);
    std::set<std::string>::iterator itDirty = dirtyHashes.find(sPos);
    if (itDirty != dirtyHashes.end())
        dirtyHashes.erase(itDirty);
    return true;
}

uint256 CClaimTrieCache::getMerkleHash() const
{
    if (empty())
    {
        uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
        return one;
    }
    if (dirty())
    {
        nodeCacheType::iterator cachedNode = cache.find("");
        if (cachedNode != cache.end())
            recursiveComputeMerkleHash(cachedNode->second, "");
        else
            recursiveComputeMerkleHash(&(base->root), "");
    }
    hashMapType::iterator ithash = cacheHashes.find("");
    if (ithash != cacheHashes.end())
        return ithash->second;
    else
        return base->root.hash;
}

bool CClaimTrieCache::empty() const
{
    return base->empty() && cache.empty();
}

CClaimTrieNode* CClaimTrieCache::addNodeToCache(const std::string& position, CClaimTrieNode* original) const
{
    if (!original)
        original = new CClaimTrieNode();
    CClaimTrieNode* cacheCopy = new CClaimTrieNode(*original);
    cache[position] = cacheCopy;
    nodeCacheType::const_iterator itOriginals = block_originals.find(position);
    if (block_originals.end() == itOriginals)
    {
        CClaimTrieNode* originalCopy = new CClaimTrieNode(*original);
        block_originals[position] = originalCopy;
    }
    return cacheCopy;
}

bool CClaimTrieCache::getOriginalInfoForName(const std::string& name, CClaimValue& claim) const
{
    nodeCacheType::const_iterator itOriginalCache = block_originals.find(name);
    if (itOriginalCache == block_originals.end())
    {
        return base->getInfoForName(name, claim);
    }
    return itOriginalCache->second->getBestClaim(claim);
}

bool CClaimTrieCache::insertClaimIntoTrie(const std::string& name, CClaimValue claim, bool fCheckTakeover) const
{
    assert(base);
    CClaimTrieNode* currentNode = &(base->root);
    nodeCacheType::iterator cachedNode;
    cachedNode = cache.find("");
    if (cachedNode != cache.end())
        currentNode = cachedNode->second;
    for (std::string::const_iterator itCur = name.begin(); itCur != name.end(); ++itCur)
    {
        std::string sCurrentSubstring(name.begin(), itCur);
        std::string sNextSubstring(name.begin(), itCur + 1);

        cachedNode = cache.find(sNextSubstring);
        if (cachedNode != cache.end())
        {
            currentNode = cachedNode->second;
            continue;
        }
        nodeMapType::iterator childNode = currentNode->children.find(*itCur);
        if (childNode != currentNode->children.end())
        {
            currentNode = childNode->second;
            continue;
        }
        
        // This next substring doesn't exist in the cache and the next
        // character doesn't exist in current node's children, so check
        // if the current node is in the cache, and if it's not, copy
        // it and stick it in the cache, and then create a new node as
        // its child and stick that in the cache. We have to have both
        // this node and its child in the cache so that the current
        // node's child map will contain the next letter, which will be
        // used to find the child in the cache. This is necessary in
        // order to calculate the merkle hash.
        cachedNode = cache.find(sCurrentSubstring);
        if (cachedNode != cache.end())
        {
            assert(cachedNode->second == currentNode);
        }
        else
        {
            currentNode = addNodeToCache(sCurrentSubstring, currentNode);
        }
        CClaimTrieNode* newNode = addNodeToCache(sNextSubstring, NULL);
        currentNode->children[*itCur] = newNode;
        currentNode = newNode;
    }

    cachedNode = cache.find(name);
    if (cachedNode != cache.end())
    {
        assert(cachedNode->second == currentNode);
    }
    else
    {
        currentNode = addNodeToCache(name, currentNode);
    }
    bool fChanged = false;
    if (currentNode->claims.empty())
    {
        fChanged = true;
        currentNode->insertClaim(claim);
    }
    else
    {
        CClaimValue currentTop = currentNode->claims.front();
        currentNode->insertClaim(claim);
        supportMapEntryType node;
        getSupportsForName(name, node);
        currentNode->reorderClaims(node);
        if (currentTop != currentNode->claims.front())
            fChanged = true;
    }
    if (fChanged)
    {
        for (std::string::const_iterator itCur = name.begin(); itCur != name.end(); ++itCur)
        {
            std::string sub(name.begin(), itCur);
            dirtyHashes.insert(sub);
        }
        dirtyHashes.insert(name);
        if (fCheckTakeover)
            namesToCheckForTakeover.insert(name);
    }
    return true;
}

bool CClaimTrieCache::removeClaimFromTrie(const std::string& name, const COutPoint& outPoint, CClaimValue& claim, bool fCheckTakeover) const
{
    assert(base);
    CClaimTrieNode* currentNode = &(base->root);
    nodeCacheType::iterator cachedNode;
    cachedNode = cache.find("");
    if (cachedNode != cache.end())
        currentNode = cachedNode->second;
    assert(currentNode != NULL); // If there is no root in either the trie or the cache, how can there be any names to remove?
    for (std::string::const_iterator itCur = name.begin(); itCur != name.end(); ++itCur)
    {
        std::string sCurrentSubstring(name.begin(), itCur);
        std::string sNextSubstring(name.begin(), itCur + 1);

        cachedNode = cache.find(sNextSubstring);
        if (cachedNode != cache.end())
        {
            currentNode = cachedNode->second;
            continue;
        }
        nodeMapType::iterator childNode = currentNode->children.find(*itCur);
        if (childNode != currentNode->children.end())
        {
            currentNode = childNode->second;
            continue;
        }
        LogPrintf("%s: The name %s does not exist in the trie\n", __func__, name.c_str());
        return false;
    }

    cachedNode = cache.find(name);
    if (cachedNode != cache.end())
        assert(cachedNode->second == currentNode);
    else
    {
        currentNode = addNodeToCache(name, currentNode);
    }
    bool fChanged = false;
    assert(currentNode != NULL);
    bool success = false;
    
    if (currentNode->claims.empty())
    {
        LogPrintf("%s: Asked to remove claim from node without claims\n", __func__);
        return false;
    }
    CClaimValue currentTop = currentNode->claims.front();

    success = currentNode->removeClaim(outPoint, claim);
    
    if (!currentNode->claims.empty())
    {
        supportMapEntryType node;
        getSupportsForName(name, node);
        currentNode->reorderClaims(node);
        if (currentTop != currentNode->claims.front())
            fChanged = true;
    }
    else
        fChanged = true;
    
    if (!success)
    {
        LogPrintf("%s: Removing a claim was unsuccessful. name = %s, txhash = %s, nOut = %d", __func__, name.c_str(), outPoint.hash.GetHex(), outPoint.n);
        return false;
    }

    if (fChanged)
    {
        for (std::string::const_iterator itCur = name.begin(); itCur != name.end(); ++itCur)
        {
            std::string sub(name.begin(), itCur);
            dirtyHashes.insert(sub);
        }
        dirtyHashes.insert(name);
        if (fCheckTakeover)
            namesToCheckForTakeover.insert(name);
    }
    CClaimTrieNode* rootNode = &(base->root);
    cachedNode = cache.find("");
    if (cachedNode != cache.end())
        rootNode = cachedNode->second;
    return recursivePruneName(rootNode, 0, name);
}

bool CClaimTrieCache::recursivePruneName(CClaimTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified) const
{
    bool fNullified = false;
    std::string sCurrentSubstring = sName.substr(0, nPos);
    if (nPos < sName.size())
    {
        std::string sNextSubstring = sName.substr(0, nPos + 1);
        unsigned char cNext = sName.at(nPos);
        CClaimTrieNode* tnNext = NULL;
        nodeCacheType::iterator cachedNode = cache.find(sNextSubstring);
        if (cachedNode != cache.end())
            tnNext = cachedNode->second;
        else
        {
            nodeMapType::iterator childNode = tnCurrent->children.find(cNext);
            if (childNode != tnCurrent->children.end())
                tnNext = childNode->second;
        }
        if (tnNext == NULL)
            return false;
        bool fChildNullified = false;
        if (!recursivePruneName(tnNext, nPos + 1, sName, &fChildNullified))
            return false;
        if (fChildNullified)
        {
            // If the child nullified itself, the child should already be
            // out of the cache, and the character must now be removed
            // from the current node's map of child nodes to ensure that
            // it isn't found when calculating the merkle hash. But
            // tnCurrent isn't necessarily in the cache. If it's not, it
            // has to be added to the cache, so nothing is changed in the
            // trie. If the current node is added to the cache, however,
            // that does not imply that the parent node must be altered to 
            // reflect that its child is now in the cache, since it
            // already has a character in its child map which will be used
            // when calculating the merkle root.

            // First, find out if this node is in the cache.
            cachedNode = cache.find(sCurrentSubstring);
            if (cachedNode == cache.end())
            {
                // it isn't, so make a copy, stick it in the cache,
                // and make it the new current node
                tnCurrent = addNodeToCache(sCurrentSubstring, tnCurrent);
            }
            // erase the character from the current node, which is
            // now guaranteed to be in the cache
            nodeMapType::iterator childNode = tnCurrent->children.find(cNext);
            if (childNode != tnCurrent->children.end())
                tnCurrent->children.erase(childNode);
            else
                return false;
        }
    }
    if (sCurrentSubstring.size() != 0 && tnCurrent->empty())
    {
        // If the current node is in the cache, remove it from there
        nodeCacheType::iterator cachedNode = cache.find(sCurrentSubstring);
        if (cachedNode != cache.end())
        {
            assert(tnCurrent == cachedNode->second);
            delete tnCurrent;
            cache.erase(cachedNode);
        }
        fNullified = true;
    }
    if (pfNullified)
        *pfNullified = fNullified;
    return true;
}

claimQueueType::iterator CClaimTrieCache::getQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    claimQueueType::iterator itQueueRow = claimQueueCache.find(nHeight);
    if (itQueueRow == claimQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        claimQueueRowType queueRow;
        // If the row exists in the base, copy its claims into the new row.
        bool exists = base->getQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<claimQueueType::iterator, bool> ret;
        ret = claimQueueCache.insert(std::pair<int, claimQueueRowType >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

queueNameType::iterator CClaimTrieCache::getQueueCacheNameRow(const std::string& name, bool createIfNotExists) const
{
    queueNameType::iterator itQueueNameRow = claimQueueNameCache.find(name);
    if (itQueueNameRow == claimQueueNameCache.end())
    {
        // Have to make a new name row and put it in the cache, if createIfNotExists is true
        queueNameRowType queueNameRow;
        // If the row exists in the base, copy its claims into the new row.
        bool exists = base->getQueueNameRow(name, queueNameRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueNameRow;
        // Stick the new row in the cache
        std::pair<queueNameType::iterator, bool> ret;
        ret = claimQueueNameCache.insert(std::pair<std::string, queueNameRowType>(name, queueNameRow));
        assert(ret.second);
        itQueueNameRow = ret.first;
    }
    return itQueueNameRow;
}

bool CClaimTrieCache::addClaim(const std::string& name, const COutPoint& outPoint, uint160 claimId, CAmount nAmount, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, claimId: %s, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, claimId.GetHex(), nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CClaimValue currentClaim;
    int delayForClaim;
    if (getOriginalInfoForName(name, currentClaim) && currentClaim.claimId == claimId)
    {
        LogPrintf("%s: This is an update to a best claim.\n", __func__);
        delayForClaim = 0;
    }
    else
    {
        delayForClaim = getDelayForName(name);
    }
    CClaimValue newClaim(outPoint, claimId, nAmount, nHeight, nHeight + delayForClaim);
    return addClaimToQueues(name, newClaim);
}

bool CClaimTrieCache::undoSpendClaim(const std::string& name, const COutPoint& outPoint, uint160 claimId, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, claimId: %s, nAmount: %d, nHeight: %d, nValidAtHeight: %d, nCurrentHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, claimId.GetHex(), nAmount, nHeight, nValidAtHeight, nCurrentHeight);
    CClaimValue claim(outPoint, claimId, nAmount, nHeight, nValidAtHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        nameOutPointType entry(name, claim.outPoint);
        addToExpirationQueue(claim.nHeight + base->nExpirationTime, entry);
        return insertClaimIntoTrie(name, claim, false);
    }
    else
    {
        return addClaimToQueues(name, claim);
    }
}

bool CClaimTrieCache::addClaimToQueues(const std::string& name, CClaimValue& claim) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, claim.nValidAtHeight);
    claimQueueEntryType entry(name, claim);
    claimQueueType::iterator itQueueRow = getQueueCacheRow(claim.nValidAtHeight, true);
    queueNameType::iterator itQueueNameRow = getQueueCacheNameRow(name, true);
    itQueueRow->second.push_back(entry);
    itQueueNameRow->second.push_back(outPointHeightType(claim.outPoint, claim.nValidAtHeight));
    nameOutPointType expireEntry(name, claim.outPoint);
    addToExpirationQueue(claim.nHeight + base->nExpirationTime, expireEntry);
    return true;
}

bool CClaimTrieCache::removeClaimFromQueue(const std::string& name, const COutPoint& outPoint, CClaimValue& claim) const
{
    queueNameType::iterator itQueueNameRow = getQueueCacheNameRow(name, false);
    if (itQueueNameRow == claimQueueNameCache.end())
    {
        return false;
    }
    queueNameRowType::iterator itQueueName;
    for (itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
    {
        if (itQueueName->outPoint == outPoint)
        {
            break;
        }
    }
    if (itQueueName == itQueueNameRow->second.end())
    {
        return false;
    }
    claimQueueType::iterator itQueueRow = getQueueCacheRow(itQueueName->nHeight, false);
    if (itQueueRow != claimQueueCache.end())
    {
        claimQueueRowType::iterator itQueue;
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            if (name == itQueue->first && itQueue->second.outPoint == outPoint)
            {
                break;
            }
        }
        if (itQueue != itQueueRow->second.end())
        {
            std::swap(claim, itQueue->second);
            itQueueNameRow->second.erase(itQueueName);
            itQueueRow->second.erase(itQueue);
            return true;
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, itQueueName->nHeight, nCurrentHeight);
    return false;
}

bool CClaimTrieCache::undoAddClaim(const std::string& name, const COutPoint& outPoint, int nHeight) const
{
    int throwaway;
    return removeClaim(name, outPoint, nHeight, throwaway, false);
}

bool CClaimTrieCache::spendClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight) const
{
    return removeClaim(name, outPoint, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCache::removeClaim(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %s, nHeight: %s, nCurrentHeight: %s\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nCurrentHeight);
    bool removed = false;
    CClaimValue claim;
    if (removeClaimFromQueue(name, outPoint, claim))
    {
        removed = true;
    }
    if (removed == false && removeClaimFromTrie(name, outPoint, claim, fCheckTakeover))
    {
        removed = true;
    }
    if (removed == true)
    {
        nValidAtHeight = claim.nValidAtHeight;
        removeFromExpirationQueue(name, outPoint, nHeight);
    }
    return removed;
}

void CClaimTrieCache::addToExpirationQueue(int nExpirationHeight, nameOutPointType& entry) const
{
    expirationQueueType::iterator itQueueRow = getExpirationQueueCacheRow(nExpirationHeight, true);
    itQueueRow->second.push_back(entry);
}

void CClaimTrieCache::removeFromExpirationQueue(const std::string& name, const COutPoint& outPoint, int nHeight) const
{
    int expirationHeight = nHeight + base->nExpirationTime;
    expirationQueueType::iterator itQueueRow = getExpirationQueueCacheRow(expirationHeight, false);
    expirationQueueRowType::iterator itQueue;
    if (itQueueRow != expirationQueueCache.end())
    {
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            if (name == itQueue->name && outPoint == itQueue->outPoint)
                break;
        }
    }
    if (itQueue != itQueueRow->second.end())
    {
        itQueueRow->second.erase(itQueue);
    }
}

expirationQueueType::iterator CClaimTrieCache::getExpirationQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    expirationQueueType::iterator itQueueRow = expirationQueueCache.find(nHeight);
    if (itQueueRow == expirationQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        expirationQueueRowType queueRow;
        // If the row exists in the base, copy its claims into the new row.
        bool exists = base->getExpirationQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<expirationQueueType::iterator, bool> ret;
        ret = expirationQueueCache.insert(std::pair<int, expirationQueueRowType >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CClaimTrieCache::reorderTrieNode(const std::string& name, bool fCheckTakeover) const
{
    assert(base);
    nodeCacheType::iterator cachedNode;
    cachedNode = cache.find(name);
    if (cachedNode == cache.end())
    {
        CClaimTrieNode* currentNode = &(base->root);
        for (std::string::const_iterator itCur = name.begin(); itCur != name.end(); ++itCur)
        {
            std::string sCurrentSubstring(name.begin(), itCur);
            std::string sNextSubstring(name.begin(), itCur + 1);

            cachedNode = cache.find(sNextSubstring);
            if (cachedNode != cache.end())
            {
                currentNode = cachedNode->second;
                continue;
            }
            nodeMapType::iterator childNode = currentNode->children.find(*itCur);
            if (childNode != currentNode->children.end())
            {
                currentNode = childNode->second;
                continue;
            }
            // The node doesn't exist, so it can't be reordered.
            return true;
        }
        currentNode = new CClaimTrieNode(*currentNode);
        std::pair<nodeCacheType::iterator, bool> ret;
        ret = cache.insert(std::pair<std::string, CClaimTrieNode*>(name, currentNode));
        assert(ret.second);
        cachedNode = ret.first;
    }
    bool fChanged = false;
    if (cachedNode->second->claims.empty())
    {
        // Nothing in there to reorder
        return true;
    }
    else
    {
        CClaimValue currentTop = cachedNode->second->claims.front();
        supportMapEntryType node;
        getSupportsForName(name, node);
        cachedNode->second->reorderClaims(node);
        if (cachedNode->second->claims.front() != currentTop)
            fChanged = true;
    }
    if (fChanged)
    {
        for (std::string::const_iterator itCur = name.begin(); itCur != name.end(); ++itCur)
        {
            std::string sub(name.begin(), itCur);
            dirtyHashes.insert(sub);
        }
        dirtyHashes.insert(name);
        if (fCheckTakeover)
            namesToCheckForTakeover.insert(name);
    }
    return true;
}

bool CClaimTrieCache::getSupportsForName(const std::string& name, supportMapEntryType& node) const
{
    supportMapType::iterator cachedNode;
    cachedNode = supportCache.find(name);
    if (cachedNode != supportCache.end())
    {
        node = cachedNode->second;
        return true;
    }
    else
    {
        return base->getSupportNode(name, node);
    }
}

bool CClaimTrieCache::insertSupportIntoMap(const std::string& name, CSupportValue support, bool fCheckTakeover) const
{
    supportMapType::iterator cachedNode;
    // If this node is already in the cache, use that
    cachedNode = supportCache.find(name);
    // If not, copy the one from base if it exists, and use that
    if (cachedNode == supportCache.end())
    {
        supportMapEntryType node;
        base->getSupportNode(name, node);
        std::pair<supportMapType::iterator, bool> ret;
        ret = supportCache.insert(std::pair<std::string, supportMapEntryType>(name, node));
        assert(ret.second);
        cachedNode = ret.first;
    }
    cachedNode->second.push_back(support);
    // See if this changed the biggest bid
    return reorderTrieNode(name,  fCheckTakeover);
}

bool CClaimTrieCache::removeSupportFromMap(const std::string& name, const COutPoint& outPoint, CSupportValue& support, bool fCheckTakeover) const
{
    supportMapType::iterator cachedNode;
    cachedNode = supportCache.find(name);
    if (cachedNode == supportCache.end())
    {
        supportMapEntryType node;
        if (!base->getSupportNode(name, node))
        {
            // clearly, this support does not exist
            return false;
        }
        std::pair<supportMapType::iterator, bool> ret;
        ret = supportCache.insert(std::pair<std::string, supportMapEntryType>(name, node));
        assert(ret.second);
        cachedNode = ret.first;
    }
    supportMapEntryType::iterator itSupport;
    for (itSupport = cachedNode->second.begin(); itSupport != cachedNode->second.end(); ++itSupport)
    {
        if (itSupport->outPoint == outPoint)
        {
            break;
        }
    }
    if (itSupport != cachedNode->second.end())
    {
        std::swap(support, *itSupport);
        cachedNode->second.erase(itSupport);
        return reorderTrieNode(name, fCheckTakeover);
    }
    else
    {
        LogPrintf("CClaimTrieCache::%s() : asked to remove a support that doesn't exist\n", __func__);
        return false;
    }
}

supportQueueType::iterator CClaimTrieCache::getSupportQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    supportQueueType::iterator itQueueRow = supportQueueCache.find(nHeight);
    if (itQueueRow == supportQueueCache.end())
    {
        supportQueueRowType queueRow;
        bool exists = base->getSupportQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<supportQueueType::iterator, bool> ret;
        ret = supportQueueCache.insert(std::pair<int, supportQueueRowType >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

queueNameType::iterator CClaimTrieCache::getSupportQueueCacheNameRow(const std::string& name, bool createIfNotExists) const
{
    queueNameType::iterator itQueueNameRow = supportQueueNameCache.find(name);
    if (itQueueNameRow == supportQueueNameCache.end())
    {
        queueNameRowType queueNameRow;
        bool exists = base->getSupportQueueNameRow(name, queueNameRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueNameRow;
        // Stick the new row in the name cache
        std::pair<queueNameType::iterator, bool> ret;
        ret = supportQueueNameCache.insert(std::pair<std::string, queueNameRowType>(name, queueNameRow));
        assert(ret.second);
        itQueueNameRow = ret.first;
    }
    return itQueueNameRow;
}

bool CClaimTrieCache::addSupportToQueues(const std::string& name, CSupportValue& support) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, support.nValidAtHeight);
    supportQueueEntryType entry(name, support);
    supportQueueType::iterator itQueueRow = getSupportQueueCacheRow(support.nValidAtHeight, true);
    queueNameType::iterator itQueueNameRow = getSupportQueueCacheNameRow(name, true);
    itQueueRow->second.push_back(entry);
    itQueueNameRow->second.push_back(outPointHeightType(support.outPoint, support.nValidAtHeight));
    nameOutPointType expireEntry(name, support.outPoint);
    addSupportToExpirationQueue(support.nHeight + base->nExpirationTime, expireEntry);
    return true;
}

bool CClaimTrieCache::removeSupportFromQueue(const std::string& name, const COutPoint& outPoint, CSupportValue& support) const
{
    queueNameType::iterator itQueueNameRow = getSupportQueueCacheNameRow(name, false);
    if (itQueueNameRow == supportQueueNameCache.end())
    {
        return false;
    }
    queueNameRowType::iterator itQueueName;
    for (itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
    {
        if (itQueueName->outPoint == outPoint)
        {
            break;
        }
    }
    if (itQueueName == itQueueNameRow->second.end())
    {
        return false;
    }
    supportQueueType::iterator itQueueRow = getSupportQueueCacheRow(itQueueName->nHeight, false);
    if (itQueueRow != supportQueueCache.end())
    {
        supportQueueRowType::iterator itQueue;
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            CSupportValue& support = itQueue->second;
            if (name == itQueue->first && support.outPoint == outPoint)
            {
                break;
            }
        }
        if (itQueue != itQueueRow->second.end())
        {
            std::swap(support, itQueue->second);
            itQueueNameRow->second.erase(itQueueName);
            itQueueRow->second.erase(itQueue);
            return true;
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named support queue but not in height support queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, itQueueName->nHeight, nCurrentHeight);
    return false;
}

bool CClaimTrieCache::addSupport(const std::string& name, const COutPoint& outPoint, CAmount nAmount, uint160 supportedClaimId, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedClaimId: %s, nHeight: %d, nCurrentHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nAmount, supportedClaimId.GetHex(), nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CClaimValue claim;
    int delayForSupport;
    if (getOriginalInfoForName(name, claim) && claim.claimId == supportedClaimId)
    {
        LogPrintf("%s: This is a support to a best claim.\n", __func__);
        delayForSupport = 0;
    }
    else
    {
        delayForSupport = getDelayForName(name);
    }
    CSupportValue support(outPoint, supportedClaimId, nAmount, nHeight, nHeight + delayForSupport);
    return addSupportToQueues(name, support);
}

bool CClaimTrieCache::undoSpendSupport(const std::string& name, const COutPoint& outPoint, uint160 supportedClaimId, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedClaimId: %s, nHeight: %d, nCurrentHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nAmount, supportedClaimId.GetHex(), nHeight, nCurrentHeight);
    CSupportValue support(outPoint, supportedClaimId, nAmount, nHeight, nValidAtHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        nameOutPointType entry(name, support.outPoint);
        addSupportToExpirationQueue(support.nHeight + base->nExpirationTime, entry);
        return insertSupportIntoMap(name, support, false);
    }
    else
    {
        return addSupportToQueues(name, support);
    }
}

bool CClaimTrieCache::removeSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight, bool fCheckTakeover) const
{
    bool removed = false;
    CSupportValue support;
    if (removeSupportFromQueue(name, outPoint, support))
        removed = true;
    if (removed == false && removeSupportFromMap(name, outPoint, support, fCheckTakeover))
        removed = true;
    if (removed)
    {
        removeSupportFromExpirationQueue(name, outPoint, nHeight);
        nValidAtHeight = support.nValidAtHeight;
    }
    return removed;
}

void CClaimTrieCache::addSupportToExpirationQueue(int nExpirationHeight, nameOutPointType& entry) const
{
    expirationQueueType::iterator itQueueRow = getSupportExpirationQueueCacheRow(nExpirationHeight, true);
    itQueueRow->second.push_back(entry);
}

void CClaimTrieCache::removeSupportFromExpirationQueue(const std::string& name, const COutPoint& outPoint, int nHeight) const
{
    int expirationHeight = nHeight + base->nExpirationTime;
    expirationQueueType::iterator itQueueRow = getSupportExpirationQueueCacheRow(expirationHeight, false);
    expirationQueueRowType::iterator itQueue;
    if (itQueueRow != supportExpirationQueueCache.end())
    {
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            if (name == itQueue->name && outPoint == itQueue->outPoint)
                break;
        }
    }
    if (itQueue != itQueueRow->second.end())
    {
        itQueueRow->second.erase(itQueue);
    }
}

expirationQueueType::iterator CClaimTrieCache::getSupportExpirationQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    expirationQueueType::iterator itQueueRow = supportExpirationQueueCache.find(nHeight);
    if (itQueueRow == supportExpirationQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        expirationQueueRowType queueRow;
        // If the row exists in the base, copy its claims into the new row.
        bool exists = base->getSupportExpirationQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<expirationQueueType::iterator, bool> ret;
        ret = supportExpirationQueueCache.insert(std::pair<int, expirationQueueRowType >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CClaimTrieCache::undoAddSupport(const std::string& name, const COutPoint& outPoint, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nCurrentHeight);
    int throwaway;
    return removeSupport(name, outPoint, nHeight, throwaway, false);
}

bool CClaimTrieCache::spendSupport(const std::string& name, const COutPoint& outPoint, int nHeight, int& nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, outPoint.hash.GetHex(), outPoint.n, nHeight, nCurrentHeight);
    return removeSupport(name, outPoint, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCache::incrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const
{
    LogPrintf("%s: nCurrentHeight (before increment): %d\n", __func__, nCurrentHeight);
    claimQueueType::iterator itQueueRow = getQueueCacheRow(nCurrentHeight, false);
    if (itQueueRow != claimQueueCache.end())
    {
        for (claimQueueRowType::iterator itEntry = itQueueRow->second.begin(); itEntry != itQueueRow->second.end(); ++itEntry)
        {
            bool found = false;
            queueNameType::iterator itQueueNameRow = getQueueCacheNameRow(itEntry->first, false);
            if (itQueueNameRow != claimQueueNameCache.end())
            {
                for (queueNameRowType::iterator itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
                {
                    if (itQueueName->outPoint == itEntry->second.outPoint && itQueueName->nHeight == nCurrentHeight)
                    {
                        found = true;
                        itQueueNameRow->second.erase(itQueueName);
                        break;
                    }
                }
            }
            if (!found)
            {
                LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in height queue but not in named queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itEntry->first, itEntry->second.outPoint.hash.GetHex(), itEntry->second.outPoint.n, itEntry->second.nValidAtHeight, nCurrentHeight);
                if (itQueueNameRow != claimQueueNameCache.end())
                {
                    LogPrintf("Claims found for that name:\n");
                    for (queueNameRowType::iterator itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
                    {
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itQueueName->outPoint.hash.GetHex(), itQueueName->outPoint.n, itQueueName->nHeight);
                    }
                }
                else
                {
                    LogPrintf("No claims found for that name\n");
                }
            }
            assert(found);
            insertClaimIntoTrie(itEntry->first, itEntry->second, true);
            insertUndo.push_back(nameOutPointHeightType(itEntry->first, itEntry->second.outPoint, itEntry->second.nValidAtHeight));
        }
        itQueueRow->second.clear();
    }
    expirationQueueType::iterator itExpirationRow = getExpirationQueueCacheRow(nCurrentHeight, false);
    if (itExpirationRow != expirationQueueCache.end())
    {
        for (expirationQueueRowType::iterator itEntry = itExpirationRow->second.begin(); itEntry != itExpirationRow->second.end(); ++itEntry)
        {
            CClaimValue claim;
            assert(removeClaimFromTrie(itEntry->name, itEntry->outPoint, claim, true));
            expireUndo.push_back(std::make_pair(itEntry->name, claim));
        }
        itExpirationRow->second.clear();
    }
    supportQueueType::iterator itSupportRow = getSupportQueueCacheRow(nCurrentHeight, false);
    if (itSupportRow != supportQueueCache.end())
    {
        for (supportQueueRowType::iterator itSupport = itSupportRow->second.begin(); itSupport != itSupportRow->second.end(); ++itSupport)
        {
            bool found = false;
            queueNameType::iterator itSupportNameRow = getSupportQueueCacheNameRow(itSupport->first, false);
            if (itSupportNameRow != supportQueueNameCache.end())
            {
                for (queueNameRowType::iterator itSupportName = itSupportNameRow->second.begin(); itSupportName != itSupportNameRow->second.end(); ++itSupportName)
                {
                    if (itSupportName->outPoint == itSupport->second.outPoint && itSupportName->nHeight == itSupport->second.nValidAtHeight)
                    {
                        found = true;
                        itSupportNameRow->second.erase(itSupportName);
                        break;
                    }
                }
            }
            if (!found)
            {
                LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nFound in height queue but not in named queue: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itSupport->first, itSupport->second.outPoint.hash.GetHex(), itSupport->second.outPoint.n, itSupport->second.nValidAtHeight, nCurrentHeight);
                if (itSupportNameRow != supportQueueNameCache.end())
                {
                    LogPrintf("Supports found for that name:\n");
                    for (queueNameRowType::iterator itSupportName = itSupportNameRow->second.begin(); itSupportName != itSupportNameRow->second.end(); ++itSupportName)
                    {
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itSupportName->outPoint.hash.GetHex(), itSupportName->outPoint.n, itSupportName->nHeight);
                    }
                }
                else
                {
                    LogPrintf("No support found for that name\n");
                }
            }
            insertSupportIntoMap(itSupport->first, itSupport->second, true);
            insertSupportUndo.push_back(nameOutPointHeightType(itSupport->first, itSupport->second.outPoint, itSupport->second.nValidAtHeight));
        }
        itSupportRow->second.clear();
    }
    expirationQueueType::iterator itSupportExpirationRow = getSupportExpirationQueueCacheRow(nCurrentHeight, false);
    if (itSupportExpirationRow != supportExpirationQueueCache.end())
    {
        for (expirationQueueRowType::iterator itEntry = itSupportExpirationRow->second.begin(); itEntry != itSupportExpirationRow->second.end(); ++itEntry)
        {
            CSupportValue support;
            assert(removeSupportFromMap(itEntry->name, itEntry->outPoint, support, true));
            expireSupportUndo.push_back(std::make_pair(itEntry->name, support));
        }
        itSupportExpirationRow->second.clear();
    }
    // check each potentially taken over name to see if a takeover occurred.
    // if it did, then check the claim and support insertion queues for 
    // the names that have been taken over, immediately insert all claim and
    // supports for those names, and stick them in the insertUndo or
    // insertSupportUndo vectors, with the nValidAtHeight they had prior to
    // this block.
    // Run through all names that have been taken over
    for (std::set<std::string>::iterator itNamesToCheck = namesToCheckForTakeover.begin(); itNamesToCheck != namesToCheckForTakeover.end(); ++itNamesToCheck)
    {
        // Check if a takeover has occurred
        nodeCacheType::iterator itCachedNode = cache.find(*itNamesToCheck);
        // many possibilities
        // if this node is new, don't put it into the undo -- there will be nothing to restore, after all
        // if all of this node's claims were deleted, it should be put into the undo -- there could be
        // claims in the queue for that name and the takeover height should be the current height
        // if the node is not in the cache, or getbestclaim fails, that means all of its claims were
        // deleted
        // if getOriginalInfoForName returns false, that means it's new and shouldn't go into the undo
        // if both exist, and the current best claim is not the same as or the parent to the new best
        // claim, then ownership has changed and the current height of last takeover should go into
        // the queue
        CClaimValue claimInCache;
        CClaimValue claimInTrie;
        bool haveClaimInCache;
        bool haveClaimInTrie;
        if (itCachedNode == cache.end())
        {
            haveClaimInCache = false;
        }
        else
        {
            haveClaimInCache = itCachedNode->second->getBestClaim(claimInCache);
        }
        haveClaimInTrie = getOriginalInfoForName(*itNamesToCheck, claimInTrie);
        bool takeoverHappened = false;
        if (!haveClaimInTrie)
        {
            takeoverHappened = true;
        }
        else if (!haveClaimInCache)
        {
            takeoverHappened = true;
        }
        else if (claimInCache != claimInTrie)
        {
            if (claimInCache.claimId != claimInTrie.claimId)
            {
                takeoverHappened = true;
            }
        }
        if (takeoverHappened)
        {
            // Get all claims in the queue for that name
            queueNameType::iterator itQueueNameRow = getQueueCacheNameRow(*itNamesToCheck, false);
            if (itQueueNameRow != claimQueueNameCache.end())
            {
                for (queueNameRowType::iterator itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
                {
                    bool found = false;
                    // Pull those claims out of the height-based queue
                    claimQueueType::iterator itQueueRow = getQueueCacheRow(itQueueName->nHeight, false);
                    claimQueueRowType::iterator itQueue;
                    if (itQueueRow != claimQueueCache.end())
                    {
                        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
                        {
                            if (*itNamesToCheck == itQueue->first && itQueue->second.outPoint == itQueueName->outPoint && itQueue->second.nValidAtHeight == itQueueName->nHeight)
                            {
                                found = true;
                                break;
                            }
                        }
                    }
                    if (found)
                    {
                        // Insert them into the queue undo with their previous nValidAtHeight
                        insertUndo.push_back(nameOutPointHeightType(itQueue->first, itQueue->second.outPoint, itQueue->second.nValidAtHeight));
                        // Insert them into the name trie with the new nValidAtHeight
                        itQueue->second.nValidAtHeight = nCurrentHeight;
                        insertClaimIntoTrie(itQueue->first, itQueue->second, false);
                        // Delete them from the height-based queue
                        itQueueRow->second.erase(itQueue);
                    }
                    else
                    {
                        LogPrintf("%s(): An inconsistency was found in the claim queue. Please report this to the developers:\nClaim found in name queue but not in height based queue:\nname: %s, txid: %s, nOut: %d, nValidAtHeight in name based queue: %d, current height: %d\n", __func__, *itNamesToCheck, itQueueName->outPoint.hash.GetHex(), itQueueName->outPoint.n, itQueueName->nHeight, nCurrentHeight);
                    }
                    assert(found);
                }
                // remove all claims from the queue for that name
                itQueueNameRow->second.clear();
            }
            // 
            // Then, get all supports in the queue for that name
            queueNameType::iterator itSupportQueueNameRow = getSupportQueueCacheNameRow(*itNamesToCheck, false);
            if (itSupportQueueNameRow != supportQueueNameCache.end())
            {
                for (queueNameRowType::iterator itSupportQueueName = itSupportQueueNameRow->second.begin(); itSupportQueueName != itSupportQueueNameRow->second.end(); ++itSupportQueueName)
                {
                    // Pull those supports out of the height-based queue
                    supportQueueType::iterator itSupportQueueRow = getSupportQueueCacheRow(itSupportQueueName->nHeight, false);
                    if (itSupportQueueRow != supportQueueCache.end())
                    {
                        supportQueueRowType::iterator itSupportQueue;
                        for (itSupportQueue = itSupportQueueRow->second.begin(); itSupportQueue != itSupportQueueRow->second.end(); ++itSupportQueue)
                        {
                            if (*itNamesToCheck == itSupportQueue->first && itSupportQueue->second.outPoint == itSupportQueueName->outPoint && itSupportQueue->second.nValidAtHeight == itSupportQueueName->nHeight)
                            {
                                break;
                            }
                        }
                        if (itSupportQueue != itSupportQueueRow->second.end())
                        {
                            // Insert them into the support queue undo with the previous nValidAtHeight
                            insertSupportUndo.push_back(nameOutPointHeightType(itSupportQueue->first, itSupportQueue->second.outPoint, itSupportQueue->second.nValidAtHeight));
                            // Insert them into the support map with the new nValidAtHeight
                            itSupportQueue->second.nValidAtHeight = nCurrentHeight;
                            insertSupportIntoMap(itSupportQueue->first, itSupportQueue->second, false);
                            // Delete them from the height-based queue
                            itSupportQueueRow->second.erase(itSupportQueue);
                        }
                        else
                        {
                            // here be problems TODO: show error, assert false
                        }
                    }
                    else
                    {
                        // here be problems
                    }
                }
                // remove all supports from the queue for that name
                itSupportQueueNameRow->second.clear();
            }
            
            // save the old last height so that it can be restored if the block is undone
            if (haveClaimInTrie)
            {
                int nHeightOfLastTakeover;
                assert(getLastTakeoverForName(*itNamesToCheck, nHeightOfLastTakeover));
                takeoverHeightUndo.push_back(std::make_pair(*itNamesToCheck, nHeightOfLastTakeover));
            }
            itCachedNode = cache.find(*itNamesToCheck);
            if (itCachedNode != cache.end())
            {
                cacheTakeoverHeights[*itNamesToCheck] = nCurrentHeight;
            }
        }
    }
    for (nodeCacheType::const_iterator itOriginals = block_originals.begin(); itOriginals != block_originals.end(); ++itOriginals)
    {
        delete itOriginals->second;
    }
    block_originals.clear();
    for (nodeCacheType::const_iterator itCache = cache.begin(); itCache != cache.end(); ++itCache)
    {
        block_originals[itCache->first] = new CClaimTrieNode(*(itCache->second));
    }
    namesToCheckForTakeover.clear();
    nCurrentHeight++;
    return true;
}

bool CClaimTrieCache::decrementBlock(insertUndoType& insertUndo, claimQueueRowType& expireUndo, insertUndoType& insertSupportUndo, supportQueueRowType& expireSupportUndo, std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const
{
    LogPrintf("%s: nCurrentHeight (before decrement): %d\n", __func__, nCurrentHeight);
    nCurrentHeight--;
    
    if (expireSupportUndo.begin() != expireSupportUndo.end())
    {
        expirationQueueType::iterator itSupportExpireRow = getSupportExpirationQueueCacheRow(nCurrentHeight, true);
        for (supportQueueRowType::iterator itSupportExpireUndo = expireSupportUndo.begin(); itSupportExpireUndo != expireSupportUndo.end(); ++itSupportExpireUndo)
        {
            insertSupportIntoMap(itSupportExpireUndo->first, itSupportExpireUndo->second, false);
            itSupportExpireRow->second.push_back(nameOutPointType(itSupportExpireUndo->first, itSupportExpireUndo->second.outPoint));
        }
    }
    
    for (insertUndoType::iterator itSupportUndo = insertSupportUndo.begin(); itSupportUndo != insertSupportUndo.end(); ++itSupportUndo)
    {
        supportQueueType::iterator itSupportRow = getSupportQueueCacheRow(itSupportUndo->nHeight, true);
        CSupportValue support;
        assert(removeSupportFromMap(itSupportUndo->name, itSupportUndo->outPoint, support, false));
        queueNameType::iterator itSupportNameRow = getSupportQueueCacheNameRow(itSupportUndo->name, true);
        itSupportRow->second.push_back(std::make_pair(itSupportUndo->name, support));
        itSupportNameRow->second.push_back(outPointHeightType(support.outPoint, support.nValidAtHeight));
    }
    
    if (expireUndo.begin() != expireUndo.end())
    {
        expirationQueueType::iterator itExpireRow = getExpirationQueueCacheRow(nCurrentHeight, true);
        for (claimQueueRowType::iterator itExpireUndo = expireUndo.begin(); itExpireUndo != expireUndo.end(); ++itExpireUndo)
        {
            insertClaimIntoTrie(itExpireUndo->first, itExpireUndo->second, false);
            itExpireRow->second.push_back(nameOutPointType(itExpireUndo->first, itExpireUndo->second.outPoint));
        }
    }

    for (insertUndoType::iterator itInsertUndo = insertUndo.begin(); itInsertUndo != insertUndo.end(); ++itInsertUndo)
    {
        claimQueueType::iterator itQueueRow = getQueueCacheRow(itInsertUndo->nHeight, true);
        CClaimValue claim;
        assert(removeClaimFromTrie(itInsertUndo->name, itInsertUndo->outPoint, claim, false));
        queueNameType::iterator itQueueNameRow = getQueueCacheNameRow(itInsertUndo->name, true);
        itQueueRow->second.push_back(std::make_pair(itInsertUndo->name, claim));
        itQueueNameRow->second.push_back(outPointHeightType(itInsertUndo->outPoint, itInsertUndo->nHeight)); 
    }
    
    for (std::vector<std::pair<std::string, int> >::iterator itTakeoverHeightUndo = takeoverHeightUndo.begin(); itTakeoverHeightUndo != takeoverHeightUndo.end(); ++itTakeoverHeightUndo)
    {
        cacheTakeoverHeights[itTakeoverHeightUndo->first] = itTakeoverHeightUndo->second;
    }
    return true;
}

bool CClaimTrieCache::finalizeDecrement() const
{
    for (nodeCacheType::iterator itOriginals = block_originals.begin(); itOriginals != block_originals.end(); ++itOriginals)
    {
        delete itOriginals->second;
    }
    block_originals.clear();
    for (nodeCacheType::const_iterator itCache = cache.begin(); itCache != cache.end(); ++itCache)
    {
        block_originals[itCache->first] = new CClaimTrieNode(*(itCache->second));
    }
    return true;
}

bool CClaimTrieCache::getLastTakeoverForName(const std::string& name, int& nLastTakeoverForName) const
{
    if (!fRequireTakeoverHeights)
    {
        nLastTakeoverForName = 0;
        return true;
    }
    std::map<std::string, int>::iterator itHeights = cacheTakeoverHeights.find(name);
    if (itHeights == cacheTakeoverHeights.end())
    {
        return base->getLastTakeoverForName(name, nLastTakeoverForName);
    }
    nLastTakeoverForName = itHeights->second;
    return true;
}

int CClaimTrieCache::getNumBlocksOfContinuousOwnership(const std::string& name) const
{
    const CClaimTrieNode* node = NULL;
    nodeCacheType::const_iterator itCache = cache.find(name);
    if (itCache != cache.end())
    {
        node = itCache->second;
    }
    if (!node)
    {
        node = base->getNodeForName(name);
    }
    if (!node || node->claims.empty())
    {
        return 0;
    }
    int nLastTakeoverHeight;
    assert(getLastTakeoverForName(name, nLastTakeoverHeight));
    return nCurrentHeight - nLastTakeoverHeight;
}

int CClaimTrieCache::getDelayForName(const std::string& name) const
{
    if (!fRequireTakeoverHeights)
    {
        return 0;
    }
    int nBlocksOfContinuousOwnership = getNumBlocksOfContinuousOwnership(name);
    return std::min(nBlocksOfContinuousOwnership / base->nProportionalDelayFactor, 4032);
}

uint256 CClaimTrieCache::getBestBlock()
{
    if (hashBlock.IsNull())
        if (base != NULL)
            hashBlock = base->hashBlock;
    return hashBlock;
}

void CClaimTrieCache::setBestBlock(const uint256& hashBlockIn)
{
    hashBlock = hashBlockIn;
}

bool CClaimTrieCache::clear() const
{
    for (nodeCacheType::iterator itcache = cache.begin(); itcache != cache.end(); ++itcache)
    {
        delete itcache->second;
    }
    cache.clear();
    for (nodeCacheType::iterator itOriginals = block_originals.begin(); itOriginals != block_originals.end(); ++itOriginals)
    {
        delete itOriginals->second;
    }
    block_originals.clear();
    dirtyHashes.clear();
    cacheHashes.clear();
    claimQueueCache.clear();
    claimQueueNameCache.clear();
    expirationQueueCache.clear();
    supportCache.clear();
    supportQueueCache.clear();
    supportQueueNameCache.clear();
    namesToCheckForTakeover.clear();
    cacheTakeoverHeights.clear();
    return true;
}

bool CClaimTrieCache::flush()
{
    if (dirty())
        getMerkleHash();
    bool success = base->update(cache, cacheHashes, cacheTakeoverHeights, getBestBlock(), claimQueueCache, claimQueueNameCache, expirationQueueCache, nCurrentHeight, supportCache, supportQueueCache, supportQueueNameCache, supportExpirationQueueCache);
    if (success)
    {
        success = clear();
    }
    return success;
}

uint256 CClaimTrieCache::getLeafHashForProof(const std::string& currentPosition, unsigned char nodeChar, const CClaimTrieNode* currentNode) const
{
    std::stringstream leafPosition;
    leafPosition << currentPosition << nodeChar;
    hashMapType::iterator cachedHash = cacheHashes.find(leafPosition.str());
    if (cachedHash != cacheHashes.end())
    {
        return cachedHash->second;
    }
    else
    {
        return currentNode->hash;
    }
}

CClaimTrieProof CClaimTrieCache::getProofForName(const std::string& name) const
{
    if (dirty())
        getMerkleHash();
    std::vector<CClaimTrieProofNode> nodes;
    CClaimTrieNode* current = &(base->root);
    nodeCacheType::const_iterator cachedNode;
    bool fNameHasValue = false;
    COutPoint outPoint;
    int nHeightOfLastTakeover = 0;
    for (std::string::const_iterator itName = name.begin(); current; ++itName)
    {
        std::string currentPosition(name.begin(), itName);
        cachedNode = cache.find(currentPosition);
        if (cachedNode != cache.end())
            current = cachedNode->second;
        CClaimValue claim;
        bool fNodeHasValue = current->getBestClaim(claim);
        uint256 valueHash;
        if (fNodeHasValue)
        {
            int nHeightOfLastTakeover;
            assert(getLastTakeoverForName(currentPosition, nHeightOfLastTakeover));
            valueHash = getValueHash(claim.outPoint, nHeightOfLastTakeover);
        }
        std::vector<std::pair<unsigned char, uint256> > children;
        CClaimTrieNode* nextCurrent = NULL;
        for (nodeMapType::const_iterator itChildren = current->children.begin(); itChildren != current->children.end(); ++itChildren)
        {
            if (itName == name.end() || itChildren->first != *itName) // Leaf node
            {
                uint256 childHash = getLeafHashForProof(currentPosition, itChildren->first, itChildren->second);
                children.push_back(std::make_pair(itChildren->first, childHash));
            }
            else // Full node
            {
                nextCurrent = itChildren->second;
                uint256 childHash;
                children.push_back(std::make_pair(itChildren->first, childHash));
            }
        }
        if (currentPosition == name)
        {
            fNameHasValue = fNodeHasValue;
            if (fNameHasValue)
            {
                outPoint = claim.outPoint;
                assert(getLastTakeoverForName(name, nHeightOfLastTakeover));
            }
            valueHash.SetNull();
        }
        CClaimTrieProofNode node(children, fNodeHasValue, valueHash);
        nodes.push_back(node);
        current = nextCurrent;
    }
    return CClaimTrieProof(nodes, fNameHasValue, outPoint,
                           nHeightOfLastTakeover);
}
