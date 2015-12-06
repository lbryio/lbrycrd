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

uint256 CClaimValue::GetHash() const
{
    CHash256 txHasher;
    txHasher.Write(txhash.begin(), txhash.size());
    std::vector<unsigned char> vchtxHash(txHasher.OUTPUT_SIZE);
    txHasher.Finalize(&(vchtxHash[0]));
        
    CHash256 nOutHasher;
    std::stringstream ss;
    ss << nOut;
    std::string snOut = ss.str();
    nOutHasher.Write((unsigned char*) snOut.data(), snOut.size());
    std::vector<unsigned char> vchnOutHash(nOutHasher.OUTPUT_SIZE);
    nOutHasher.Finalize(&(vchnOutHash[0]));

    CHash256 hasher;
    hasher.Write(vchtxHash.data(), vchtxHash.size());
    hasher.Write(vchnOutHash.data(), vchnOutHash.size());
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Finalize(&(vchHash[0]));
    
    uint256 claimHash(vchHash);
    return claimHash;
}

bool CClaimTrieNode::insertClaim(CClaimValue claim)
{
    LogPrintf("%s: Inserting %s:%d (amount: %d)  into the claim trie\n", __func__, claim.txhash.ToString(), claim.nOut, claim.nAmount);
    claims.push_back(claim);
    return true;
}

bool CClaimTrieNode::removeClaim(uint256& txhash, uint32_t nOut, CClaimValue& claim)
{
    LogPrintf("%s: Removing txid: %s, nOut: %d from the claim trie\n", __func__, txhash.ToString(), nOut);

    std::vector<CClaimValue>::iterator position;
    for (position = claims.begin(); position != claims.end(); ++position)
    {
        if (position->txhash == txhash && position->nOut == nOut)
        {
            std::swap(claim, *position);
            break;
        }
    }
    if (position != claims.end())
        claims.erase(position);
    else
    {
        LogPrintf("CClaimTrieNode::%s() : asked to remove a claim that doesn't exist\n", __func__);
        LogPrintf("CClaimTrieNode::%s() : claims that do exist:\n", __func__);
        for (unsigned int i = 0; i < claims.size(); i++)
        {
            LogPrintf("\ttxid: %s, nOut: %d\n", claims[i].txhash.ToString(), claims[i].nOut);
        }
        return false;
    }
    return true;
}

bool CClaimTrieNode::getBestClaim(CClaimValue& claim) const
{
    if (claims.empty())
        return false;
    else
    {
        claim = claims.front();
        return true;
    }
}

bool CClaimTrieNode::haveClaim(const uint256& txhash, uint32_t nOut) const
{
    for (std::vector<CClaimValue>::const_iterator itclaim = claims.begin(); itclaim != claims.end(); ++itclaim)
        if (itclaim->txhash == txhash && itclaim->nOut == nOut)
            return true;
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
            if (itsupport->supportTxhash == itclaim->txhash && itsupport->supportnOut == itclaim->nOut)
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
    boost::scoped_ptr<CLevelDBIterator> pcursor(const_cast<CLevelDBWrapper*>(&db)->NewIterator());
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
    for (claimQueueType::const_iterator itRow = dirtyExpirationQueueRows.begin(); itRow != dirtyExpirationQueueRows.end(); ++itRow)
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

bool CClaimTrie::haveClaim(const std::string& name, const uint256& txhash, uint32_t nOut) const
{
    const CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end())
            return false;
        current = itchildren->second;
    }
    return current->haveClaim(txhash, nOut);
}

bool CClaimTrie::haveSupport(const std::string& name, const uint256& txhash, uint32_t nOut) const
{
    supportMapEntryType node;
    if (!getSupportNode(name, node))
    {
        return false;
    }
    for (supportMapEntryType::const_iterator itnode = node.begin(); itnode != node.end(); ++itnode)
    {
        if (itnode->txhash == txhash && itnode->nOut == nOut)
            return true;
    }
    return false;
}

bool CClaimTrie::haveClaimInQueue(const std::string& name, const uint256& txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    std::vector<CClaimValue> nameRow;
    if (!getQueueNameRow(name, nameRow))
    {
        return false;
    }
    std::vector<CClaimValue>::const_iterator itNameRow;
    for (itNameRow = nameRow.begin(); itNameRow != nameRow.end(); ++itNameRow)
    {
        if (itNameRow->txhash == txhash && itNameRow->nOut == nOut && itNameRow->nHeight == nHeight)
        {
            nValidAtHeight = itNameRow->nValidAtHeight;
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
            if (itRow->first == name && itRow->second.txhash == txhash && itRow->second.nOut == nOut && itRow->second.nHeight == nHeight)
            {
                if (itRow->second.nValidAtHeight != nValidAtHeight)
                {
                    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, txhash.GetHex(), nOut, nValidAtHeight, itRow->second.nValidAtHeight, nCurrentHeight);
                }
                return true;
            }
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, txhash.GetHex(), nOut, nValidAtHeight, nCurrentHeight);
    return false;   
}

bool CClaimTrie::haveSupportInQueue(const std::string& name, const uint256& txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    std::vector<CSupportValue> nameRow;
    if (!getSupportQueueNameRow(name, nameRow))
    {
        return false;
    }
    std::vector<CSupportValue>::const_iterator itNameRow;
    for (itNameRow = nameRow.begin(); itNameRow != nameRow.end(); ++itNameRow)
    {
        if (itNameRow->txhash == txhash && itNameRow->nOut == nOut && itNameRow->nHeight == nHeight)
        {
            nValidAtHeight = itNameRow->nValidAtHeight;
            break;
        }
    }
    if (itNameRow == nameRow.end())
    {
        return false;
    }
    supportQueueRowType row;
    if (getSupportQueueRow(nHeight, row))
    {
        for (supportQueueRowType::const_iterator itRow = row.begin(); itRow != row.end(); ++itRow)
        {
            if (itRow->first == name && itRow->second.txhash == txhash && itRow->second.nOut == nOut && itRow->second.nHeight == nHeight)
            {
                if (itRow->second.nValidAtHeight != nValidAtHeight)
                {
                    LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nDifferent nValidAtHeight between named queue and height queue\n: name: %s, txid: %s, nOut: %d, nValidAtHeight in named queue: %d, nValidAtHeight in height queue: %d current height: %d\n", __func__, name, txhash.GetHex(), nOut, nValidAtHeight, itRow->second.nValidAtHeight, nCurrentHeight);
                }
                return true;
            }
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, txhash.GetHex(), nOut, nValidAtHeight, nCurrentHeight);
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
        return current->getBestClaim(claim);
    return false;
}

bool CClaimTrie::getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const
{
    const CClaimTrieNode* current = getNodeForName(name);
    if (current)
    {
        lastTakeoverHeight = current->nHeightOfLastTakeover;
        return true;
    }
    return false;
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
        uint256 claimHash = claim.GetHash();
        vchToHash.insert(vchToHash.end(), claimHash.begin(), claimHash.end());
        std::vector<unsigned char> heightToHash = heightToVch(node->nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), heightToHash.begin(), heightToHash.end());
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

bool CClaimTrie::getQueueNameRow(const std::string& name, std::vector<CClaimValue>& row) const
{
    claimQueueNamesType::const_iterator itQueueNameRow = dirtyQueueNameRows.find(name);
    if (itQueueNameRow != dirtyQueueNameRows.end())
    {
        row = itQueueNameRow->second;
        return true;
    }
    return db.Read(std::make_pair(CLAIM_QUEUE_NAME_ROW, name), row);
}

bool CClaimTrie::getExpirationQueueRow(int nHeight, claimQueueRowType& row) const
{
    claimQueueType::const_iterator itQueueRow = dirtyExpirationQueueRows.find(nHeight);
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

void CClaimTrie::updateQueueNameRow(const std::string& name, std::vector<CClaimValue>& row)
{
    claimQueueNamesType::iterator itQueueRow = dirtyQueueNameRows.find(name);
    if (itQueueRow == dirtyQueueNameRows.end())
    {
        std::vector<CClaimValue> newRow;
        std::pair<claimQueueNamesType::iterator, bool> ret;
        ret = dirtyQueueNameRows.insert(std::pair<std::string, std::vector<CClaimValue> >(name, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateExpirationRow(int nHeight, claimQueueRowType& row)
{
    claimQueueType::iterator itQueueRow = dirtyExpirationQueueRows.find(nHeight);
    if (itQueueRow == dirtyExpirationQueueRows.end())
    {
        claimQueueRowType newRow;
        std::pair<claimQueueType::iterator, bool> ret;
        ret = dirtyExpirationQueueRows.insert(std::pair<int, claimQueueRowType >(nHeight, newRow));
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

void CClaimTrie::updateSupportNameQueue(const std::string& name, std::vector<CSupportValue>& row)
{
    supportQueueNamesType::iterator itQueueRow = dirtySupportQueueNameRows.find(name);
    if (itQueueRow == dirtySupportQueueNameRows.end())
    {
        std::vector<CSupportValue> newRow;
        std::pair<supportQueueNamesType::iterator, bool> ret;
        ret = dirtySupportQueueNameRows.insert(std::pair<std::string, std::vector<CSupportValue> >(name, newRow));
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

bool CClaimTrie::getSupportQueueNameRow(const std::string& name, std::vector<CSupportValue>& row) const
{
    supportQueueNamesType::const_iterator itQueueNameRow = dirtySupportQueueNameRows.find(name);
    if (itQueueNameRow != dirtySupportQueueNameRows.end())
    {
        row = itQueueNameRow->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT_QUEUE_NAME_ROW, name), row);
}

bool CClaimTrie::update(nodeCacheType& cache, hashMapType& hashes, std::map<std::string, int>& takeoverHeights, const uint256& hashBlockIn, claimQueueType& queueCache, claimQueueNamesType& queueNameCache, claimQueueType& expirationQueueCache, int nNewHeight, supportMapType& supportCache, supportQueueType& supportQueueCache, supportQueueNamesType& supportQueueNameCache)
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
    for (claimQueueNamesType::iterator itQueueNameCacheRow = queueNameCache.begin(); itQueueNameCacheRow != queueNameCache.end(); ++itQueueNameCacheRow)
    {
        updateQueueNameRow(itQueueNameCacheRow->first, itQueueNameCacheRow->second);
    }
    for (claimQueueType::iterator itExpirationRow = expirationQueueCache.begin(); itExpirationRow != expirationQueueCache.end(); ++itExpirationRow)
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
    for (supportQueueNamesType::iterator itSupportNameQueue = supportQueueNameCache.begin(); itSupportNameQueue != supportQueueNameCache.end(); ++itSupportNameQueue)
    {
        updateSupportNameQueue(itSupportNameQueue->first, itSupportNameQueue->second);
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

void CClaimTrie::BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CClaimTrieNode* pNode) const
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

void CClaimTrie::BatchWriteQueueRows(CLevelDBBatch& batch)
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

void CClaimTrie::BatchWriteQueueNameRows(CLevelDBBatch& batch)
{
    for (claimQueueNamesType::iterator itQueue = dirtyQueueNameRows.begin(); itQueue != dirtyQueueNameRows.end(); ++itQueue)
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

void CClaimTrie::BatchWriteExpirationQueueRows(CLevelDBBatch& batch)
{
    for (claimQueueType::iterator itQueue = dirtyExpirationQueueRows.begin(); itQueue != dirtyExpirationQueueRows.end(); ++itQueue)
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

void CClaimTrie::BatchWriteSupportNodes(CLevelDBBatch& batch)
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

void CClaimTrie::BatchWriteSupportQueueRows(CLevelDBBatch& batch)
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

void CClaimTrie::BatchWriteSupportQueueNameRows(CLevelDBBatch& batch)
{
    for (supportQueueNamesType::iterator itQueue = dirtySupportQueueNameRows.begin(); itQueue != dirtySupportQueueNameRows.end(); ++itQueue)
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

bool CClaimTrie::WriteToDisk()
{
    CLevelDBBatch batch(&db.GetObfuscateKey());
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
    boost::scoped_ptr<CLevelDBIterator> pcursor(const_cast<CLevelDBWrapper*>(&db)->NewIterator());
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
        uint256 claimHash = claim.GetHash();
        vchToHash.insert(vchToHash.end(), claimHash.begin(), claimHash.end());
        int nHeightOfLastTakeover;
        assert(getLastTakeoverForName(sPos, nHeightOfLastTakeover));
        std::vector<unsigned char> heightToHash = heightToVch(nHeightOfLastTakeover);
        vchToHash.insert(vchToHash.end(), heightToHash.begin(), heightToHash.end());
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

bool CClaimTrieCache::insertClaimIntoTrie(const std::string name, CClaimValue claim, bool fCheckTakeover) const
{
    assert(base);
    CClaimTrieNode* currentNode = &(base->root);
    nodeCacheType::iterator cachedNode;
    cachedNode = cache.find("");
    if (cachedNode != cache.end())
        currentNode = cachedNode->second;
    if (currentNode == NULL)
    {
        currentNode = new CClaimTrieNode();
        cache[""] = currentNode;
    }
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
            currentNode = new CClaimTrieNode(*currentNode);
            cache[sCurrentSubstring] = currentNode;
        }
        CClaimTrieNode* newNode = new CClaimTrieNode();
        currentNode->children[*itCur] = newNode;
        cache[sNextSubstring] = newNode;
        currentNode = newNode;
    }

    cachedNode = cache.find(name);
    if (cachedNode != cache.end())
    {
        assert(cachedNode->second == currentNode);
    }
    else
    {
        currentNode = new CClaimTrieNode(*currentNode);
        cache[name] = currentNode;
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

bool CClaimTrieCache::removeClaimFromTrie(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight, bool fCheckTakeover) const
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
        currentNode = new CClaimTrieNode(*currentNode);
        cache[name] = currentNode;
    }
    bool fChanged = false;
    assert(currentNode != NULL);
    CClaimValue claim;
    bool success = false;
    
    if (currentNode->claims.empty())
    {
        LogPrintf("%s: Asked to remove claim from node without claims\n", __func__);
        return false;
    }
    CClaimValue currentTop = currentNode->claims.front();

    success = currentNode->removeClaim(txhash, nOut, claim);
    
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
        LogPrintf("%s: Removing a claim was unsuccessful. name = %s, txhash = %s, nOut = %d", __func__, name.c_str(), txhash.GetHex(), nOut);
    }
    else
    {
        nValidAtHeight = claim.nValidAtHeight;
    }
    assert(success);
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
                tnCurrent = new CClaimTrieNode(*tnCurrent);
                cache[sCurrentSubstring] = tnCurrent;
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

claimQueueNamesType::iterator CClaimTrieCache::getQueueCacheNameRow(const std::string& name, bool createIfNotExists) const
{
    claimQueueNamesType::iterator itQueueNameRow = claimQueueNameCache.find(name);
    if (itQueueNameRow == claimQueueNameCache.end())
    {
        // Have to make a new name row and put it in the cache, if createIfNotExists is true
        std::vector<CClaimValue> queueNameRow;
        // If the row exists in the base, copy its claims into the new row.
        bool exists = base->getQueueNameRow(name, queueNameRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueNameRow;
        // Stick the new row in the cache
        std::pair<claimQueueNamesType::iterator, bool> ret;
        ret = claimQueueNameCache.insert(std::pair<std::string, std::vector<CClaimValue> >(name, queueNameRow));
        assert(ret.second);
        itQueueNameRow = ret.first;
    }
    return itQueueNameRow;
}

bool CClaimTrieCache::addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CClaimValue claim(txhash, nOut, nAmount, nHeight, nHeight + getDelayForName(name));
    return addClaimToQueues(name, claim);
}

bool CClaimTrieCache::addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, uint256 prevTxhash, uint32_t nPrevOut) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CClaimValue currentClaim;
    if (base->getInfoForName(name, currentClaim))
    {
        if (currentClaim.txhash == prevTxhash && currentClaim.nOut == nPrevOut)
        {
            LogPrintf("%s: This is an update to a best claim. Previous claim txhash: %s, nOut: %d\n", __func__, prevTxhash.GetHex(), nPrevOut);
            CClaimValue newClaim(txhash, nOut, nAmount, nHeight, nHeight, prevTxhash, nPrevOut);
            return addClaimToQueues(name, newClaim);
        }
    }
    return addClaim(name, txhash, nOut, nAmount, nHeight);
}

bool CClaimTrieCache::undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nValidAtHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nValidAtHeight, nCurrentHeight);
    CClaimValue claim(txhash, nOut, nAmount, nHeight, nValidAtHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        claimQueueEntryType entry(name, claim);
        addToExpirationQueue(entry);
        return insertClaimIntoTrie(name, claim, false);
    }
    else
    {
        return addClaimToQueues(name, claim);
    }
}

bool CClaimTrieCache::addClaimToQueues(const std::string name, CClaimValue& claim) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, claim.nValidAtHeight);
    claimQueueEntryType entry(name, claim);
    claimQueueType::iterator itQueueRow = getQueueCacheRow(claim.nValidAtHeight, true);
    claimQueueNamesType::iterator itQueueNameRow = getQueueCacheNameRow(name, true);
    itQueueRow->second.push_back(entry);
    itQueueNameRow->second.push_back(claim);
    addToExpirationQueue(entry);
    return true;
}

bool CClaimTrieCache::removeClaimFromQueue(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight) const
{
    claimQueueNamesType::iterator itQueueNameRow = getQueueCacheNameRow(name, false);
    if (itQueueNameRow == claimQueueNameCache.end())
    {
        return false;
    }
    std::vector<CClaimValue>::iterator itQueueName;
    for (itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
    {
        if (itQueueName->txhash == txhash && itQueueName->nOut == nOut)
        {
            nValidAtHeight = itQueueName->nValidAtHeight;
            break;
        }
    }
    if (itQueueName == itQueueNameRow->second.end())
    {
        return false;
    }
    claimQueueType::iterator itQueueRow = getQueueCacheRow(nValidAtHeight, false);
    if (itQueueRow != claimQueueCache.end())
    {
        claimQueueRowType::iterator itQueue;
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            if (name == itQueue->first && itQueue->second.txhash == txhash && itQueue->second.nOut == nOut)
            {
                break;
            }
        }
        if (itQueue != itQueueRow->second.end())
        {
            itQueueNameRow->second.erase(itQueueName);
            itQueueRow->second.erase(itQueue);
            return true;
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named queue but not in height queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, txhash.GetHex(), nOut, nValidAtHeight, nCurrentHeight);
    return false;
}

bool CClaimTrieCache::undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const
{
    int throwaway;
    return removeClaim(name, txhash, nOut, nHeight, throwaway, false);
}

bool CClaimTrieCache::spendClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    return removeClaim(name, txhash, nOut, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCache::removeClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight, bool fCheckTakeover) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %s, nHeight: %s, nCurrentHeight: %s\n", __func__, name, txhash.GetHex(), nOut, nHeight, nCurrentHeight);
    bool removed = false;
    if (removeClaimFromQueue(name, txhash, nOut, nValidAtHeight))
        removed = true;
    if (removed == false && removeClaimFromTrie(name, txhash, nOut, nValidAtHeight, fCheckTakeover))
        removed = true;
    if (removed == true)
        removeFromExpirationQueue(name, txhash, nOut, nHeight);
    return removed;
}

void CClaimTrieCache::addToExpirationQueue(claimQueueEntryType& entry) const
{
    int expirationHeight = entry.second.nHeight + base->nExpirationTime;
    claimQueueType::iterator itQueueRow = getExpirationQueueCacheRow(expirationHeight, true);
    itQueueRow->second.push_back(entry);
}

void CClaimTrieCache::removeFromExpirationQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const
{
    int expirationHeight = nHeight + base->nExpirationTime;
    claimQueueType::iterator itQueueRow = getExpirationQueueCacheRow(expirationHeight, false);
    claimQueueRowType::iterator itQueue;
    if (itQueueRow != claimQueueCache.end())
    {
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            CClaimValue& claim = itQueue->second;
            if (name == itQueue->first && claim.txhash == txhash && claim.nOut == nOut)
                break;
        }
    }
    if (itQueue != itQueueRow->second.end())
    {
        itQueueRow->second.erase(itQueue);
    }
}

claimQueueType::iterator CClaimTrieCache::getExpirationQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    claimQueueType::iterator itQueueRow = expirationQueueCache.find(nHeight);
    if (itQueueRow == expirationQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        claimQueueRowType queueRow;
        // If the row exists in the base, copy its claims into the new row.
        bool exists = base->getExpirationQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<claimQueueType::iterator, bool> ret;
        ret = expirationQueueCache.insert(std::pair<int, claimQueueRowType >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CClaimTrieCache::reorderTrieNode(const std::string name, bool fCheckTakeover) const
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

bool CClaimTrieCache::getSupportsForName(const std::string name, supportMapEntryType& node) const
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

bool CClaimTrieCache::insertSupportIntoMap(const std::string name, CSupportValue support, bool fCheckTakeover) const
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

bool CClaimTrieCache::removeSupportFromMap(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int& nValidAtHeight, bool fCheckTakeover) const
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
        if (itSupport->txhash == txhash && itSupport->nOut == nOut && itSupport->supportTxhash == supportedTxhash && itSupport->supportnOut == supportednOut && itSupport->nHeight == nHeight)
        {
            nValidAtHeight = itSupport->nValidAtHeight;
            break;
        }
    }
    if (itSupport != cachedNode->second.end())
    {
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

supportQueueNamesType::iterator CClaimTrieCache::getSupportQueueCacheNameRow(const std::string& name, bool createIfNotExists) const
{
    supportQueueNamesType::iterator itQueueNameRow = supportQueueNameCache.find(name);
    if (itQueueNameRow == supportQueueNameCache.end())
    {
        std::vector<CSupportValue> queueNameRow;
        bool exists = base->getSupportQueueNameRow(name, queueNameRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueNameRow;
        // Stick the new row in the name cache
        std::pair<supportQueueNamesType::iterator, bool> ret;
        ret = supportQueueNameCache.insert(std::pair<std::string, std::vector<CSupportValue> >(name, queueNameRow));
        assert(ret.second);
        itQueueNameRow = ret.first;
    }
    return itQueueNameRow;
}

bool CClaimTrieCache::addSupportToQueue(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, nValidAtHeight);
    CSupportValue support(txhash, nOut, supportedTxhash, supportednOut, nAmount, nHeight, nValidAtHeight);
    supportQueueEntryType entry(name, support);
    supportQueueType::iterator itQueueRow = getSupportQueueCacheRow(nValidAtHeight, true);
    supportQueueNamesType::iterator itQueueNameRow = getSupportQueueCacheNameRow(name, true);
    itQueueRow->second.push_back(entry);
    itQueueNameRow->second.push_back(support);
    return true;
}

bool CClaimTrieCache::removeSupportFromQueue(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int& nValidAtHeight) const
{
    supportQueueNamesType::iterator itQueueNameRow = getSupportQueueCacheNameRow(name, false);
    if (itQueueNameRow == supportQueueNameCache.end())
    {
        return false;
    }
    std::vector<CSupportValue>::iterator itQueueName;
    for (itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
    {
        if (itQueueName->txhash == txhash && itQueueName->nOut == nOut && itQueueName->supportTxhash == supportedTxhash && itQueueName->supportnOut == supportednOut)
        {
            nValidAtHeight = itQueueName->nValidAtHeight;
            break;
        }
    }
    if (itQueueName == itQueueNameRow->second.end())
    {
        return false;
    }
    supportQueueType::iterator itQueueRow = getSupportQueueCacheRow(nValidAtHeight, false);
    if (itQueueRow != supportQueueCache.end())
    {
        supportQueueRowType::iterator itQueue;
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            CSupportValue& support = itQueue->second;
            if (name == itQueue->first && support.txhash == txhash && support.nOut == nOut && support.supportTxhash == supportedTxhash && support.supportnOut == supportednOut)
            {
                nValidAtHeight = support.nValidAtHeight;
                break;
            }
        }
        if (itQueue != itQueueRow->second.end())
        {
            itQueueNameRow->second.erase(itQueueName);
            itQueueRow->second.erase(itQueue);
            return true;
        }
    }
    LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in named support queue but not in height support queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, name, txhash.GetHex(), nOut, nValidAtHeight, nCurrentHeight);
    return false;
}

bool CClaimTrieCache::addSupport(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, uint32_t supportednOut, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CClaimValue claim;
    if (base->getInfoForName(name, claim))
    {
        if (claim.txhash == supportedTxhash && claim.nOut == supportednOut)
        {
            LogPrintf("%s: This is a support to a best claim.\n", __func__);
            return addSupportToQueue(name, txhash, nOut, nAmount, supportedTxhash, supportednOut, nHeight, nHeight);
        }
    }
    return addSupportToQueue(name, txhash, nOut, nAmount, supportedTxhash, supportednOut, nHeight, nHeight + getDelayForName(name));
}

bool CClaimTrieCache::undoSpendSupport(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        CSupportValue support(txhash, nOut, supportedTxhash, supportednOut, nAmount, nHeight, nValidAtHeight);
        supportQueueEntryType entry(name, support);
        return insertSupportIntoMap(name, support, false);
    }
    else
    {
        return addSupportToQueue(name, txhash, nOut, nAmount, supportedTxhash, supportednOut, nHeight, nValidAtHeight);
    }
}

bool CClaimTrieCache::removeSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int& nValidAtHeight, bool fCheckTakeover) const
{
    bool removed = false;
    if (removeSupportFromQueue(name, txhash, nOut, supportedTxhash, supportednOut, nValidAtHeight))
        removed = true;
    if (removed == false && removeSupportFromMap(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, nValidAtHeight, fCheckTakeover))
        removed = true;
    return removed;
}

bool CClaimTrieCache::undoAddSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    int throwaway;
    return removeSupport(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, throwaway, false);
}

bool CClaimTrieCache::spendSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int& nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    return removeSupport(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, nValidAtHeight, true);
}

bool CClaimTrieCache::incrementBlock(claimQueueRowType& insertUndo, claimQueueRowType& expireUndo, supportQueueRowType& insertSupportUndo, std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const
{
    LogPrintf("%s: nCurrentHeight (before increment): %d\n", __func__, nCurrentHeight);
    claimQueueType::iterator itQueueRow = getQueueCacheRow(nCurrentHeight, false);
    if (itQueueRow != claimQueueCache.end())
    {
        for (claimQueueRowType::iterator itEntry = itQueueRow->second.begin(); itEntry != itQueueRow->second.end(); ++itEntry)
        {
            bool found = false;
            claimQueueNamesType::iterator itQueueNameRow = getQueueCacheNameRow(itEntry->first, false);
            if (itQueueNameRow != claimQueueNameCache.end())
            {
                for (std::vector<CClaimValue>::iterator itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
                {
                    if (*itQueueName == itEntry->second)
                    {
                        found = true;
                        itQueueNameRow->second.erase(itQueueName);
                        break;
                    }
                }
            }
            if (!found)
            {
                LogPrintf("%s: An inconsistency was found in the claim queue. Please report this to the developers:\nFound in height queue but not in named queue: name: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itEntry->first, itEntry->second.txhash.GetHex(), itEntry->second.nOut, itEntry->second.nValidAtHeight, nCurrentHeight);
                if (itQueueNameRow != claimQueueNameCache.end())
                {
                    LogPrintf("Claims found for that name:\n");
                    for (std::vector<CClaimValue>::iterator itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
                    {
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itQueueName->txhash.GetHex(), itQueueName->nOut, itQueueName->nValidAtHeight);
                    }
                }
                else
                {
                    LogPrintf("No claims found for that name\n");
                }
            }
            insertClaimIntoTrie(itEntry->first, itEntry->second, true);
            insertUndo.push_back(*itEntry);
        }
        itQueueRow->second.clear();
    }
    claimQueueType::iterator itExpirationRow = getExpirationQueueCacheRow(nCurrentHeight, false);
    if (itExpirationRow != expirationQueueCache.end())
    {
        for (claimQueueRowType::iterator itEntry = itExpirationRow->second.begin(); itEntry != itExpirationRow->second.end(); ++itEntry)
        {
            int nValidAtHeight;
            assert(removeClaimFromTrie(itEntry->first, itEntry->second.txhash, itEntry->second.nOut, nValidAtHeight, true));
            expireUndo.push_back(*itEntry);
        }
        itExpirationRow->second.clear();
    }
    supportQueueType::iterator itSupportRow = getSupportQueueCacheRow(nCurrentHeight, false);
    if (itSupportRow != supportQueueCache.end())
    {
        for (supportQueueRowType::iterator itSupport = itSupportRow->second.begin(); itSupport != itSupportRow->second.end(); ++itSupport)
        {
            bool found = false;
            supportQueueNamesType::iterator itSupportNameRow = getSupportQueueCacheNameRow(itSupport->first, false);
            if (itSupportNameRow != supportQueueNameCache.end())
            {
                for (std::vector<CSupportValue>::iterator itSupportName = itSupportNameRow->second.begin(); itSupportName != itSupportNameRow->second.end(); ++itSupportName)
                {
                    if (*itSupportName == itSupport->second)
                    {
                        found = true;
                        itSupportNameRow->second.erase(itSupportName);
                        break;
                    }
                }
            }
            if (!found)
            {
                LogPrintf("%s: An inconsistency was found in the support queue. Please report this to the developers:\nFound in height queue but not in named queue: %s, txid: %s, nOut: %d, nValidAtHeight: %d, current height: %d\n", __func__, itSupport->first, itSupport->second.txhash.GetHex(), itSupport->second.nOut, itSupport->second.nValidAtHeight, nCurrentHeight);
                if (itSupportNameRow != supportQueueNameCache.end())
                {
                    LogPrintf("Supports found for that name:\n");
                    for (std::vector<CSupportValue>::iterator itSupportName = itSupportNameRow->second.begin(); itSupportName != itSupportNameRow->second.end(); ++itSupportName)
                    {
                        LogPrintf("\ttxid: %s, nOut: %d, nValidAtHeight: %d\n", itSupportName->txhash.GetHex(), itSupportName->nOut, itSupportName->nValidAtHeight);
                    }
                }
                else
                {
                    LogPrintf("No support found for that name\n");
                }
            }
            insertSupportIntoMap(itSupport->first, itSupport->second, true);
            insertSupportUndo.push_back(*itSupport);
        }
        itSupportRow->second.clear();
    }
    // check each potentially taken over name to see if a takeover occurred.
    // if it did, then check the claim and support insertion queues for 
    // the names that have been taken over, immediately insert all claim and
    // supports for those names, and stick them in the insertUndo or
    // insertSupportUndo vectors, with the nValidAtHeight they had prior to
    // this block
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
        // if base->getInfoForName returns false, that means it's new and shouldn't go into the undo
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
        haveClaimInTrie = base->getInfoForName(*itNamesToCheck, claimInTrie);
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
            if (!claimInCache.fIsUpdate || claimInCache.updateToTxhash != claimInTrie.txhash || claimInCache.updateToNOut != claimInTrie.nOut)
            {
                takeoverHappened = true;
            }
        }
        if (takeoverHappened && !base->fConstantDelay)
        {
            // Get all claims in the queue for that name
            claimQueueNamesType::iterator itQueueNameRow = getQueueCacheNameRow(*itNamesToCheck, false);
            if (itQueueNameRow != claimQueueNameCache.end())
            {
                for (std::vector<CClaimValue>::iterator itQueueName = itQueueNameRow->second.begin(); itQueueName != itQueueNameRow->second.end(); ++itQueueName)
                {
                    // Pull those claims out of the height-based queue
                    claimQueueType::iterator itQueueRow = getQueueCacheRow(itQueueName->nValidAtHeight, false);
                    if (itQueueRow != claimQueueCache.end())
                    {
                        claimQueueRowType::iterator itQueue;
                        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
                        {
                            if (*itNamesToCheck == itQueue->first && itQueue->second == *itQueueName)
                            {
                                break;
                            }
                        }
                        if (itQueue != itQueueRow->second.end())
                        {
                            // Insert them into the queue undo with their previous nValidAtHeight
                            insertUndo.push_back(*itQueue);
                            // Insert them into the name trie with the new nValidAtHeight
                            itQueue->second.nValidAtHeight = nCurrentHeight;
                            insertClaimIntoTrie(itQueue->first, itQueue->second, false);
                            // Delete them from the height-based queue
                            itQueueRow->second.erase(itQueue);
                        }
                        else
                        {
                            // here be problems
                        }
                    }
                    else
                    {
                        // here be problems
                    }
                }
                // remove all claims from the queue for that name
                itQueueNameRow->second.clear();
            }
            // 
            // Then, get all supports in the queue for that name
            supportQueueNamesType::iterator itSupportQueueNameRow = getSupportQueueCacheNameRow(*itNamesToCheck, false);
            if (itSupportQueueNameRow != supportQueueNameCache.end())
            {
                for (std::vector<CSupportValue>::iterator itSupportQueueName = itSupportQueueNameRow->second.begin(); itSupportQueueName != itSupportQueueNameRow->second.end(); ++itSupportQueueName)
                {
                    // Pull those supports out of the height-based queue
                    supportQueueType::iterator itSupportQueueRow = getSupportQueueCacheRow(itSupportQueueName->nValidAtHeight, false);
                    if (itSupportQueueRow != supportQueueCache.end())
                    {
                        supportQueueRowType::iterator itSupportQueue;
                        for (itSupportQueue = itSupportQueueRow->second.begin(); itSupportQueue != itSupportQueueRow->second.end(); ++itSupportQueue)
                        {
                            if (*itNamesToCheck == itSupportQueue->first && itSupportQueue->second == *itSupportQueueName)
                            {
                                break;
                            }
                        }
                        if (itSupportQueue != itSupportQueueRow->second.end())
                        {
                            // Insert them into the support queue undo with the previous nValidAtHeight
                            insertSupportUndo.push_back(*itSupportQueue);
                            // Insert them into the support map with the new nValidAtHeight
                            itSupportQueue->second.nValidAtHeight = nCurrentHeight;
                            insertSupportIntoMap(itSupportQueue->first, itSupportQueue->second, false);
                            // Delete them from the height-based queue
                            itSupportQueueRow->second.erase(itSupportQueue);
                        }
                        else
                        {
                            // here be problems
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
        }
        if (takeoverHappened)
        {
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
    namesToCheckForTakeover.clear();
    nCurrentHeight++;
    return true;
}

bool CClaimTrieCache::decrementBlock(claimQueueRowType& insertUndo, claimQueueRowType& expireUndo, supportQueueRowType& insertSupportUndo, std::vector<std::pair<std::string, int> >& takeoverHeightUndo) const
{
    LogPrintf("%s: nCurrentHeight (before decrement): %d\n", __func__, nCurrentHeight);
    nCurrentHeight--;
    for (claimQueueRowType::iterator itInsertUndo = insertUndo.begin(); itInsertUndo != insertUndo.end(); ++itInsertUndo)
    {
        claimQueueType::iterator itQueueRow = getQueueCacheRow(itInsertUndo->second.nValidAtHeight, true);
        int nValidHeightInTrie;
        assert(removeClaimFromTrie(itInsertUndo->first, itInsertUndo->second.txhash, itInsertUndo->second.nOut, nValidHeightInTrie, false));
        claimQueueNamesType::iterator itQueueNameRow = getQueueCacheNameRow(itInsertUndo->first, true);
        itQueueRow->second.push_back(*itInsertUndo);
        itQueueNameRow->second.push_back(itInsertUndo->second); 
    }
    if (expireUndo.begin() != expireUndo.end())
    {
        claimQueueType::iterator itExpireRow = getExpirationQueueCacheRow(nCurrentHeight, true);
        for (claimQueueRowType::iterator itExpireUndo = expireUndo.begin(); itExpireUndo != expireUndo.end(); ++itExpireUndo)
        {
            insertClaimIntoTrie(itExpireUndo->first, itExpireUndo->second, false);
            itExpireRow->second.push_back(*itExpireUndo);
        }
    }
    for (supportQueueRowType::iterator itSupportUndo = insertSupportUndo.begin(); itSupportUndo != insertSupportUndo.end(); ++itSupportUndo)
    {
        supportQueueType::iterator itSupportRow = getSupportQueueCacheRow(itSupportUndo->second.nValidAtHeight, true);
        int nValidHeightInMap;
        assert(removeSupportFromMap(itSupportUndo->first, itSupportUndo->second.txhash, itSupportUndo->second.nOut, itSupportUndo->second.supportTxhash, itSupportUndo->second.supportnOut, itSupportUndo->second.nHeight, nValidHeightInMap, false));
        supportQueueNamesType::iterator itSupportNameRow = getSupportQueueCacheNameRow(itSupportUndo->first, true);
        itSupportRow->second.push_back(*itSupportUndo);
        itSupportNameRow->second.push_back(itSupportUndo->second);
    }

    for (std::vector<std::pair<std::string, int> >::iterator itTakeoverHeightUndo = takeoverHeightUndo.begin(); itTakeoverHeightUndo != takeoverHeightUndo.end(); ++itTakeoverHeightUndo)
    {
        cacheTakeoverHeights[itTakeoverHeightUndo->first] = itTakeoverHeightUndo->second;
    }
    return true;
}

bool CClaimTrieCache::getLastTakeoverForName(const std::string& name, int& lastTakeoverHeight) const
{
    std::map<std::string, int>::iterator itHeights = cacheTakeoverHeights.find(name);
    if (itHeights == cacheTakeoverHeights.end())
    {
        if (base->getLastTakeoverForName(name, lastTakeoverHeight))
        {
            return true;
        }
        else
        {
            if (fRequireTakeoverHeights)
            {
                return false;
            }
            else
            {
                lastTakeoverHeight = 0;
                return true;
            }
        }
    }
    lastTakeoverHeight = itHeights->second;
    return true;
}

int CClaimTrieCache::getDelayForName(const std::string name) const
{
    if (base->fConstantDelay)
    {
        return base->nConstantDelayHeight;
    }
    else
    {
        int nHeightOfLastTakeover;
        if (getLastTakeoverForName(name, nHeightOfLastTakeover))
        {
            return nHeightOfLastTakeover >> base->nProportionalDelayBits;
        }
        else
        {
            return 0;
        }
    }
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
    bool success = base->update(cache, cacheHashes, cacheTakeoverHeights, getBestBlock(), claimQueueCache, claimQueueNameCache, expirationQueueCache, nCurrentHeight, supportCache, supportQueueCache, supportQueueNameCache);
    if (success)
    {
        success = clear();
    }
    return success;
}
