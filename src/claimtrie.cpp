#include "claimtrie.h"
#include "leveldbwrapper.h"

#include <boost/scoped_ptr.hpp>

uint256 CNodeValue::GetHash() const
{
    CHash256 valTxHasher;
    valTxHasher.Write(txhash.begin(), txhash.size());
    std::vector<unsigned char> vchValTxHash(valTxHasher.OUTPUT_SIZE);
    valTxHasher.Finalize(&(vchValTxHash[0]));
        
    CHash256 valnOutHasher;
    std::stringstream ss;
    ss << nOut;
    std::string snOut = ss.str();
    valnOutHasher.Write((unsigned char*) snOut.data(), snOut.size());
    std::vector<unsigned char> vchValnOutHash(valnOutHasher.OUTPUT_SIZE);
    valnOutHasher.Finalize(&(vchValnOutHash[0]));

    CHash256 valHasher;
    valHasher.Write(vchValTxHash.data(), vchValTxHash.size());
    valHasher.Write(vchValnOutHash.data(), vchValnOutHash.size());
    std::vector<unsigned char> vchValHash(valHasher.OUTPUT_SIZE);
    valHasher.Finalize(&(vchValHash[0]));
    
    uint256 valHash(vchValHash);
    return valHash;
}

bool CClaimTrieNode::insertValue(CNodeValue val)//, bool * pfChanged)
{
    LogPrintf("%s: Inserting %s:%d (amount: %d)  into the claim trie\n", __func__, val.txhash.ToString(), val.nOut, val.nAmount);
    values.push_back(val);
    return true;
}

bool CClaimTrieNode::removeValue(uint256& txhash, uint32_t nOut, CNodeValue& val)//, bool * pfChanged)
{
    LogPrintf("%s: Removing txid: %s, nOut: %d from the claim trie\n", __func__, txhash.ToString(), nOut);

    std::vector<CNodeValue>::iterator position;
    for (position = values.begin(); position != values.end(); ++position)
    {
        if (position->txhash == txhash && position->nOut == nOut)
        {
            std::swap(val, *position);
            break;
        }
    }
    if (position != values.end())
        values.erase(position);
    else
    {
        LogPrintf("CClaimTrieNode::%s() : asked to remove a value that doesn't exist\n", __func__);
        LogPrintf("CClaimTrieNode::%s() : values that do exist:\n", __func__);
        for (unsigned int i = 0; i < values.size(); i++)
        {
            LogPrintf("\ttxid: %s, nOut: %d\n", values[i].txhash.ToString(), values[i].nOut);
        }
        return false;
    }
    return true;
}

bool CClaimTrieNode::getBestValue(CNodeValue& value) const
{
    if (values.empty())
        return false;
    else
    {
        value = values.front();
        return true;
    }
}

bool CClaimTrieNode::haveValue(const uint256& txhash, uint32_t nOut) const
{
    for (std::vector<CNodeValue>::const_iterator itval = values.begin(); itval != values.end(); ++itval)
        if (itval->txhash == txhash && itval->nOut == nOut)
            return true;
    return false;
}

void CClaimTrieNode::reorderValues(supportMapNodeType& supports)
{
    std::vector<CNodeValue>::iterator itVal;
    
    for (itVal = values.begin(); itVal != values.end(); ++itVal)
    {
        itVal->nEffectiveAmount = itVal->nAmount;
    }

    for (supportMapNodeType::iterator itSupport = supports.begin(); itSupport != supports.end(); ++itSupport)
    {
        for (itVal = values.begin(); itVal != values.end(); ++itVal)
        {
            if (itSupport->supportTxhash == itVal->txhash && itSupport->supportnOut == itVal->nOut)
            {
                itVal->nEffectiveAmount += itSupport->nAmount;
                break;
            }
        }
    }
    
    std::make_heap(values.begin(), values.end());
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
    for (valueQueueType::const_iterator itRow = dirtyQueueRows.begin(); itRow != dirtyQueueRows.end(); ++itRow)
    {
        if (!itRow->second.empty())
            return false;
    }
    int dummy;
    return keyTypeEmpty(QUEUE_ROW, dummy);
}

bool CClaimTrie::expirationQueueEmpty() const
{
    for (valueQueueType::const_iterator itRow = dirtyExpirationQueueRows.begin(); itRow != dirtyExpirationQueueRows.end(); ++itRow)
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
    for (supportValueQueueType::const_iterator itRow = dirtySupportQueueRows.begin(); itRow != dirtySupportQueueRows.end(); ++itRow)
    {
        if (!itRow->second.empty())
            return false;
    }
    int dummy;
    return keyTypeEmpty(SUPPORT_QUEUE, dummy);
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
    return current->haveValue(txhash, nOut);
}


bool CClaimTrie::haveSupport(const std::string& name, const uint256& txhash, uint32_t nOut) const
{
    supportMapNodeType node;
    if (!getSupportNode(name, node))
    {
        return false;
    }
    for (supportMapNodeType::const_iterator itnode = node.begin(); itnode != node.end(); ++itnode)
    {
        if (itnode->txhash == txhash && itnode->nOut == nOut)
            return true;
    }
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
    if (!(current->values.empty()))
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
    unsigned int claims_in_subtrie = current->values.size();
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
    for (std::vector<CNodeValue>::const_iterator itval = current->values.begin(); itval != current->values.end(); ++itval)
    {
        value_in_subtrie += itval->nAmount;
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

bool CClaimTrie::getInfoForName(const std::string& name, CNodeValue& val) const
{
    const CClaimTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end())
            return false;
        current = itchildren->second;
    }
    return current->getBestValue(val);
}

bool CClaimTrie::checkConsistency()
{
    if (empty())
        return true;
    return recursiveCheckConsistency(&root);
}

bool CClaimTrie::recursiveCheckConsistency(CClaimTrieNode* node)
{
    std::vector<unsigned char> vchToHash;

    for (nodeMapType::iterator it = node->children.begin(); it != node->children.end(); ++it)
    {
        if (recursiveCheckConsistency(it->second))
        {
            vchToHash.push_back(it->first);
            vchToHash.insert(vchToHash.end(), it->second->hash.begin(), it->second->hash.end());
        }
        else
            return false;
    }

    CNodeValue val;
    bool hasValue = node->getBestValue(val);

    if (hasValue)
    {
        uint256 valHash = val.GetHash();
        vchToHash.insert(vchToHash.end(), valHash.begin(), valHash.end());
    }

    CHash256 hasher;
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Write(vchToHash.data(), vchToHash.size());
    hasher.Finalize(&(vchHash[0]));
    uint256 calculatedHash(vchHash);
    return calculatedHash == node->hash;
}

bool CClaimTrie::getQueueRow(int nHeight, std::vector<CValueQueueEntry>& row)
{
    valueQueueType::iterator itQueueRow = dirtyQueueRows.find(nHeight);
    if (itQueueRow != dirtyQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(QUEUE_ROW, nHeight), row);
}

bool CClaimTrie::getExpirationQueueRow(int nHeight, std::vector<CValueQueueEntry>& row)
{
    valueQueueType::iterator itQueueRow = dirtyExpirationQueueRows.find(nHeight);
    if (itQueueRow != dirtyExpirationQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(EXP_QUEUE_ROW, nHeight), row);
}

void CClaimTrie::updateQueueRow(int nHeight, std::vector<CValueQueueEntry>& row)
{
    valueQueueType::iterator itQueueRow = dirtyQueueRows.find(nHeight);
    if (itQueueRow == dirtyQueueRows.end())
    {
        std::vector<CValueQueueEntry> newRow;
        std::pair<valueQueueType::iterator, bool> ret;
        ret = dirtyQueueRows.insert(std::pair<int, std::vector<CValueQueueEntry> >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateExpirationRow(int nHeight, std::vector<CValueQueueEntry>& row)
{
    valueQueueType::iterator itQueueRow = dirtyExpirationQueueRows.find(nHeight);
    if (itQueueRow == dirtyExpirationQueueRows.end())
    {
        std::vector<CValueQueueEntry> newRow;
        std::pair<valueQueueType::iterator, bool> ret;
        ret = dirtyExpirationQueueRows.insert(std::pair<int, std::vector<CValueQueueEntry> >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

void CClaimTrie::updateSupportMap(const std::string& name, supportMapNodeType& node)
{
    supportMapType::iterator itNode = dirtySupportNodes.find(name);
    if (itNode == dirtySupportNodes.end())
    {
        supportMapNodeType newNode;
        std::pair<supportMapType::iterator, bool> ret;
        ret = dirtySupportNodes.insert(std::pair<std::string, supportMapNodeType>(name, newNode));
        assert(ret.second);
        itNode = ret.first;
    }
    itNode->second.swap(node);
}

void CClaimTrie::updateSupportQueue(int nHeight, std::vector<CSupportValueQueueEntry>& row)
{
    supportValueQueueType::iterator itQueueRow = dirtySupportQueueRows.find(nHeight);
    if (itQueueRow == dirtySupportQueueRows.end())
    {
        std::vector<CSupportValueQueueEntry> newRow;
        std::pair<supportValueQueueType::iterator, bool> ret;
        ret = dirtySupportQueueRows.insert(std::pair<int, std::vector<CSupportValueQueueEntry> >(nHeight, newRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    itQueueRow->second.swap(row);
}

bool CClaimTrie::getSupportNode(std::string name, supportMapNodeType& node)
{
    supportMapType::iterator itNode = dirtySupportNodes.find(name);
    if (itNode != dirtySupportNodes.end())
    {
        node = itNode->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT, name), node);
}

bool CClaimTrie::getSupportNode(std::string name, supportMapNodeType& node) const
{
    supportMapType::const_iterator itNode = dirtySupportNodes.find(name);
    if (itNode != dirtySupportNodes.end())
    {
        node = itNode->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT, name), node);
}

bool CClaimTrie::getSupportQueueRow(int nHeight, std::vector<CSupportValueQueueEntry>& row)
{
    supportValueQueueType::iterator itQueueRow = dirtySupportQueueRows.find(nHeight);
    if (itQueueRow != dirtySupportQueueRows.end())
    {
        row = itQueueRow->second;
        return true;
    }
    return db.Read(std::make_pair(SUPPORT_QUEUE, nHeight), row);
}

bool CClaimTrie::update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlockIn, valueQueueType& queueCache, valueQueueType& expirationQueueCache, int nNewHeight, supportMapType& supportCache, supportValueQueueType& supportQueueCache)
{
    bool success = true;
    for (nodeCacheType::iterator itcache = cache.begin(); itcache != cache.end(); ++itcache)
    {
        success = updateName(itcache->first, itcache->second);
        if (!success)
            return false;
    }
    for (hashMapType::iterator ithash = hashes.begin(); ithash != hashes.end(); ++ithash)
    {
        success = updateHash(ithash->first, ithash->second);
        if (!success)
            return false;
    }
    for (valueQueueType::iterator itQueueCacheRow = queueCache.begin(); itQueueCacheRow != queueCache.end(); ++itQueueCacheRow)
    {
        updateQueueRow(itQueueCacheRow->first, itQueueCacheRow->second);
    }
    for (valueQueueType::iterator itExpirationRow = expirationQueueCache.begin(); itExpirationRow != expirationQueueCache.end(); ++itExpirationRow)
    {
        updateExpirationRow(itExpirationRow->first, itExpirationRow->second);
    }
    for (supportMapType::iterator itSupportCache = supportCache.begin(); itSupportCache != supportCache.end(); ++itSupportCache)
    {
        updateSupportMap(itSupportCache->first, itSupportCache->second);
    }
    for (supportValueQueueType::iterator itSupportQueue = supportQueueCache.begin(); itSupportQueue != supportQueueCache.end(); ++itSupportQueue)
    {
        updateSupportQueue(itSupportQueue->first, itSupportQueue->second);
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
    current->values.swap(updatedNode->values);
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

void CClaimTrie::BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CClaimTrieNode* pNode) const
{
    uint32_t num_values = 0;
    if (pNode)
        num_values = pNode->values.size();
    LogPrintf("%s: Writing %s to disk with %d values\n", __func__, name, num_values);
    if (pNode)
        batch.Write(std::make_pair(TRIE_NODE, name), *pNode);
    else
        batch.Erase(std::make_pair(TRIE_NODE, name));
}

void CClaimTrie::BatchWriteQueueRows(CLevelDBBatch& batch)
{
    for (valueQueueType::iterator itQueue = dirtyQueueRows.begin(); itQueue != dirtyQueueRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(QUEUE_ROW, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(QUEUE_ROW, itQueue->first), itQueue->second);
        }
    }
}

void CClaimTrie::BatchWriteExpirationQueueRows(CLevelDBBatch& batch)
{
    for (valueQueueType::iterator itQueue = dirtyExpirationQueueRows.begin(); itQueue != dirtyExpirationQueueRows.end(); ++itQueue)
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
    for (supportValueQueueType::iterator itQueue = dirtySupportQueueRows.begin(); itQueue != dirtySupportQueueRows.end(); ++itQueue)
    {
        if (itQueue->second.empty())
        {
            batch.Erase(std::make_pair(SUPPORT_QUEUE, itQueue->first));
        }
        else
        {
            batch.Write(std::make_pair(SUPPORT_QUEUE, itQueue->first), itQueue->second);
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
    BatchWriteExpirationQueueRows(batch);
    dirtyExpirationQueueRows.clear();
    BatchWriteSupportNodes(batch);
    dirtySupportNodes.clear();
    BatchWriteSupportQueueRows(batch);
    dirtySupportQueueRows.clear();
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
    
    CNodeValue val;
    bool hasValue = tnCurrent->getBestValue(val);

    if (hasValue)
    {
        uint256 valHash = val.GetHash();
        vchToHash.insert(vchToHash.end(), valHash.begin(), valHash.end());
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

bool CClaimTrieCache::insertClaimIntoTrie(const std::string name, CNodeValue val) const
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
    if (currentNode->values.empty())
    {
        fChanged = true;
        currentNode->insertValue(val);//, &fChanged);
    }
    else
    {
        CNodeValue currentTop = currentNode->values.front();
        currentNode->insertValue(val);
        supportMapNodeType node;
        getSupportsForName(name, node);
        currentNode->reorderValues(node);
        if (currentTop != currentNode->values.front())
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
    }
    return true;
}

bool CClaimTrieCache::removeClaimFromTrie(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight) const
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
    CNodeValue val;
    bool success = false;
    
    if (currentNode->values.empty())
    {
        LogPrintf("%s: Asked to remove value from node without values\n", __func__);
        return false;
    }
    CNodeValue currentTop = currentNode->values.front();

    success = currentNode->removeValue(txhash, nOut, val);//, &fChanged);
    
    if (!currentNode->values.empty())
    {
        supportMapNodeType node;
        getSupportsForName(name, node);
        currentNode->reorderValues(node);
        if (currentTop != currentNode->values.front())
            fChanged = true;
    }
    else
        fChanged = true;
    
    if (!success)
    {
        LogPrintf("%s: Removing a value was unsuccessful. name = %s, txhash = %s, nOut = %d", __func__, name.c_str(), txhash.GetHex(), nOut);
    }
    else
    {
        nValidAtHeight = val.nValidAtHeight;
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

valueQueueType::iterator CClaimTrieCache::getQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    valueQueueType::iterator itQueueRow = valueQueueCache.find(nHeight);
    if (itQueueRow == valueQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        std::vector<CValueQueueEntry> queueRow;
        // If the row exists in the base, copy its values into the new row.
        bool exists = base->getQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<valueQueueType::iterator, bool> ret;
        ret = valueQueueCache.insert(std::pair<int, std::vector<CValueQueueEntry> >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CClaimTrieCache::addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    return addClaimToQueues(name, txhash, nOut, nAmount, nHeight, nHeight + DEFAULT_DELAY);
}

bool CClaimTrieCache::addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, uint256 prevTxhash, uint32_t nPrevOut) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CNodeValue val;
    if (base->getInfoForName(name, val))
    {
        if (val.txhash == prevTxhash && val.nOut == nPrevOut)
        {
            LogPrintf("%s: This is an update to a best claim. Previous claim txhash: %s, nOut: %d\n", __func__, prevTxhash.GetHex(), nPrevOut);
            return addClaimToQueues(name, txhash, nOut, nAmount, nHeight, nHeight);
        }
    }
    return addClaim(name, txhash, nOut, nAmount, nHeight);
}

bool CClaimTrieCache::undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nValidAtHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nValidAtHeight, nCurrentHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        CNodeValue val(txhash, nOut, nAmount, nHeight, nValidAtHeight);
        CValueQueueEntry entry(name, val);
        addToExpirationQueue(entry);
        return insertClaimIntoTrie(name, val);
    }
    else
    {
        return addClaimToQueues(name, txhash, nOut, nAmount, nHeight, nValidAtHeight);
    }
}

bool CClaimTrieCache::addClaimToQueues(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, nValidAtHeight);
    CNodeValue val(txhash, nOut, nAmount, nHeight, nValidAtHeight);
    CValueQueueEntry entry(name, val);
    valueQueueType::iterator itQueueRow = getQueueCacheRow(nValidAtHeight, true);
    itQueueRow->second.push_back(entry);
    addToExpirationQueue(entry);
    return true;
}

bool CClaimTrieCache::removeClaimFromQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeightToCheck, int& nValidAtHeight) const
{
    valueQueueType::iterator itQueueRow = getQueueCacheRow(nHeightToCheck, false);
    if (itQueueRow == valueQueueCache.end())
    {
        return false;
    }
    std::vector<CValueQueueEntry>::iterator itQueue;
    for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
    {
        CNodeValue& val = itQueue->val;
        if (name == itQueue->name && val.txhash == txhash && val.nOut == nOut)
        {
            nValidAtHeight = val.nValidAtHeight;
            break;
        }
    }
    if (itQueue != itQueueRow->second.end())
    {
        itQueueRow->second.erase(itQueue);
        return true;
    }
    return false;
}

bool CClaimTrieCache::undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const
{
    int throwaway;
    return removeClaim(name, txhash, nOut, nHeight, throwaway);
}

bool CClaimTrieCache::spendClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    return removeClaim(name, txhash, nOut, nHeight, nValidAtHeight);
}

bool CClaimTrieCache::removeClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %s, nHeight: %s, nCurrentHeight: %s\n", __func__, name, txhash.GetHex(), nOut, nHeight, nCurrentHeight);
    bool removed = false;
    if (nHeight + DEFAULT_DELAY >= nCurrentHeight)
    {
        if (removeClaimFromQueue(name, txhash, nOut, nHeight + DEFAULT_DELAY, nValidAtHeight))
            removed = true;
        else if (removeClaimFromQueue(name, txhash, nOut, nHeight, nValidAtHeight))
            removed = true;
    }
    //if (removed == false && removeClaimFromQueue(name, txhash, nOut, nHeight, nCurrentHeight))
    //    removed = true;
    if (removed == false && removeClaimFromTrie(name, txhash, nOut, nValidAtHeight))
        removed = true;
    if (removed == true)
        removeFromExpirationQueue(name, txhash, nOut, nHeight);
    return removed;
}

void CClaimTrieCache::addToExpirationQueue(CValueQueueEntry& entry) const
{
    int expirationHeight = entry.val.nHeight + base->nExpirationTime;
    valueQueueType::iterator itQueueRow = getExpirationQueueCacheRow(expirationHeight, true);
    itQueueRow->second.push_back(entry);
}

void CClaimTrieCache::removeFromExpirationQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const
{
    int expirationHeight = nHeight + base->nExpirationTime;
    valueQueueType::iterator itQueueRow = getExpirationQueueCacheRow(expirationHeight, false);
    std::vector<CValueQueueEntry>::iterator itQueue;
    if (itQueueRow != valueQueueCache.end())
    {
        for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
        {
            CNodeValue& val = itQueue->val;
            if (name == itQueue->name && val.txhash == txhash && val.nOut == nOut)
                break;
        }
    }
    if (itQueue != itQueueRow->second.end())
    {
        itQueueRow->second.erase(itQueue);
    }
}

valueQueueType::iterator CClaimTrieCache::getExpirationQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    valueQueueType::iterator itQueueRow = expirationQueueCache.find(nHeight);
    if (itQueueRow == expirationQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        std::vector<CValueQueueEntry> queueRow;
        // If the row exists in the base, copy its values into the new row.
        bool exists = base->getExpirationQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<valueQueueType::iterator, bool> ret;
        ret = expirationQueueCache.insert(std::pair<int, std::vector<CValueQueueEntry> >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CClaimTrieCache::reorderTrieNode(const std::string name) const
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
    if (cachedNode->second->values.empty())
    {
        // Nothing in there to reorder
        return true;
    }
    else
    {
        CNodeValue currentTop = cachedNode->second->values.front();
        supportMapNodeType node;
        getSupportsForName(name, node);
        cachedNode->second->reorderValues(node);
        if (cachedNode->second->values.front() != currentTop)
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
    }
    return true;
}

bool CClaimTrieCache::getSupportsForName(const std::string name, supportMapNodeType& node) const
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

bool CClaimTrieCache::insertSupportIntoMap(const std::string name, CSupportNodeValue val) const
{
    supportMapType::iterator cachedNode;
    // If this node is already in the cache, use that
    cachedNode = supportCache.find(name);
    // If not, copy the one from base if it exists, and use that
    if (cachedNode == supportCache.end())
    {
        supportMapNodeType node;
        base->getSupportNode(name, node);
        std::pair<supportMapType::iterator, bool> ret;
        ret = supportCache.insert(std::pair<std::string, supportMapNodeType>(name, node));
        assert(ret.second);
        cachedNode = ret.first;
    }
    cachedNode->second.push_back(val);
    // See if this changed the biggest bid
    return reorderTrieNode(name);
}

bool CClaimTrieCache::removeSupportFromMap(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int& nValidAtHeight) const
{
    supportMapType::iterator cachedNode;
    cachedNode = supportCache.find(name);
    if (cachedNode == supportCache.end())
    {
        supportMapNodeType node;
        if (!base->getSupportNode(name, node))
        {
            // clearly, this support does not exist
            return false;
        }
        std::pair<supportMapType::iterator, bool> ret;
        ret = supportCache.insert(std::pair<std::string, supportMapNodeType>(name, node));
        assert(ret.second);
        cachedNode = ret.first;
    }
    supportMapNodeType::iterator itSupport;
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
        return reorderTrieNode(name);
    }
    else
    {
        LogPrintf("CClaimTrieCache::%s() : asked to remove a support that doesn't exist\n", __func__);
        return false;
    }
}

supportValueQueueType::iterator CClaimTrieCache::getSupportQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    supportValueQueueType::iterator itQueueRow = supportQueueCache.find(nHeight);
    if (itQueueRow == supportQueueCache.end())
    {
        std::vector<CSupportValueQueueEntry> queueRow;
        bool exists = base->getSupportQueueRow(nHeight, queueRow);
        if (!exists)
            if (!createIfNotExists)
                return itQueueRow;
        // Stick the new row in the cache
        std::pair<supportValueQueueType::iterator, bool> ret;
        ret = supportQueueCache.insert(std::pair<int, std::vector<CSupportValueQueueEntry> >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CClaimTrieCache::addSupportToQueue(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, nValidAtHeight);
    CSupportNodeValue val(txhash, nOut, supportedTxhash, supportednOut, nAmount, nHeight, nValidAtHeight);
    CSupportValueQueueEntry entry(name, val);
    supportValueQueueType::iterator itQueueRow = getSupportQueueCacheRow(nValidAtHeight, true);
    itQueueRow->second.push_back(entry);
    return true;
}

bool CClaimTrieCache::removeSupportFromQueue(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeightToCheck, int& nValidAtHeight) const
{
    supportValueQueueType::iterator itQueueRow = getSupportQueueCacheRow(nHeightToCheck, false);
    if (itQueueRow == supportQueueCache.end())
    {
        return false;
    }
    std::vector<CSupportValueQueueEntry>::iterator itQueue;
    for (itQueue = itQueueRow->second.begin(); itQueue != itQueueRow->second.end(); ++itQueue)
    {
        CSupportNodeValue& val = itQueue->val;
        if (name == itQueue->name && val.txhash == txhash && val.nOut == nOut && val.supportTxhash == supportedTxhash && val.supportnOut == supportednOut)
         {
             nValidAtHeight = val.nValidAtHeight;
             break;
         }
    }
    if (itQueue != itQueueRow->second.end())
    {
        itQueueRow->second.erase(itQueue);
        return true;
    }
    return false;
}

bool CClaimTrieCache::addSupport(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, uint32_t supportednOut, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CNodeValue val;
    if (base->getInfoForName(name, val))
    {
        if (val.txhash == supportedTxhash && val.nOut == supportednOut)
        {
            LogPrintf("%s: This is a support to a best claim.\n", __func__);
            return addSupportToQueue(name, txhash, nOut, nAmount, supportedTxhash, supportednOut, nHeight, nHeight);
        }
    }
    return addSupportToQueue(name, txhash, nOut, nAmount, supportedTxhash, supportednOut, nHeight, nHeight + DEFAULT_DELAY);
}

bool CClaimTrieCache::undoSpendSupport(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        CSupportNodeValue val(txhash, nOut, supportedTxhash, supportednOut, nAmount, nHeight, nValidAtHeight);
        CSupportValueQueueEntry entry(name, val);
        return insertSupportIntoMap(name, val);
    }
    else
    {
        return addSupportToQueue(name, txhash, nOut, nAmount, supportedTxhash, supportednOut, nHeight, nValidAtHeight);
    }
}

bool CClaimTrieCache::removeSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int& nValidAtHeight) const
{
    bool removed = false;
    if (nHeight + DEFAULT_DELAY >= nCurrentHeight)
    {
        if (removeSupportFromQueue(name, txhash, nOut, supportedTxhash, supportednOut, nHeight + DEFAULT_DELAY, nValidAtHeight))
            removed = true;
        else if (removeSupportFromQueue(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, nValidAtHeight))
            removed = true;
    }
    if (removed == false && removeSupportFromMap(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, nValidAtHeight))
        removed = true;
    return removed;
}

bool CClaimTrieCache::undoAddSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    int throwaway;
    return removeSupport(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, throwaway);
}

bool CClaimTrieCache::spendSupport(const std::string name, uint256 txhash, uint32_t nOut, uint256 supportedTxhash, uint32_t supportednOut, int nHeight, int& nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, supportedTxhash: %s, supportednOut: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, supportedTxhash.GetHex(), supportednOut, nHeight, nCurrentHeight);
    return removeSupport(name, txhash, nOut, supportedTxhash, supportednOut, nHeight, nValidAtHeight);
}

bool CClaimTrieCache::incrementBlock(CClaimTrieQueueUndo& insertUndo, CClaimTrieQueueUndo& expireUndo, CSupportValueQueueUndo& insertSupportUndo) const
{
    LogPrintf("%s: nCurrentHeight (before increment): %d\n", __func__, nCurrentHeight);
    valueQueueType::iterator itQueueRow = getQueueCacheRow(nCurrentHeight, false);
    if (itQueueRow != valueQueueCache.end())
    {
        for (std::vector<CValueQueueEntry>::iterator itEntry = itQueueRow->second.begin(); itEntry != itQueueRow->second.end(); ++itEntry)
        {
            insertClaimIntoTrie(itEntry->name, itEntry->val);
            insertUndo.push_back(*itEntry);
        }
        itQueueRow->second.clear();
    }
    valueQueueType::iterator itExpirationRow = getExpirationQueueCacheRow(nCurrentHeight, false);
    if (itExpirationRow != expirationQueueCache.end())
    {
        for (std::vector<CValueQueueEntry>::iterator itEntry = itExpirationRow->second.begin(); itEntry != itExpirationRow->second.end(); ++itEntry)
        {
            int nValidAtHeight;
            assert(base->nExpirationTime > DEFAULT_DELAY);
            assert(removeClaimFromTrie(itEntry->name, itEntry->val.txhash, itEntry->val.nOut, nValidAtHeight));
            expireUndo.push_back(*itEntry);
        }
        itExpirationRow->second.clear();
    }
    supportValueQueueType::iterator itSupportRow = getSupportQueueCacheRow(nCurrentHeight, false);
    if (itSupportRow != supportQueueCache.end())
    {
        for (std::vector<CSupportValueQueueEntry>::iterator itSupport = itSupportRow->second.begin(); itSupport != itSupportRow->second.end(); ++itSupport)
        {
            insertSupportIntoMap(itSupport->name, itSupport->val);
            insertSupportUndo.push_back(*itSupport);
        }
        itSupportRow->second.clear();
    }
    nCurrentHeight++;
    return true;
}

bool CClaimTrieCache::decrementBlock(CClaimTrieQueueUndo& insertUndo, CClaimTrieQueueUndo& expireUndo, CSupportValueQueueUndo& insertSupportUndo) const
{
    LogPrintf("%s: nCurrentHeight (before decrement): %d\n", __func__, nCurrentHeight);
    nCurrentHeight--;
    if (insertUndo.begin() != insertUndo.end())
    {
        valueQueueType::iterator itQueueRow = getQueueCacheRow(nCurrentHeight, true);
        for (CClaimTrieQueueUndo::iterator itInsertUndo = insertUndo.begin(); itInsertUndo != insertUndo.end(); ++itInsertUndo)
        {
            int nValidHeightInTrie;
            assert(removeClaimFromTrie(itInsertUndo->name, itInsertUndo->val.txhash, itInsertUndo->val.nOut, nValidHeightInTrie));
            assert(nValidHeightInTrie == itInsertUndo->val.nValidAtHeight);
            itQueueRow->second.push_back(*itInsertUndo);
        }
    }
    if (expireUndo.begin() != expireUndo.end())
    {
        valueQueueType::iterator itExpireRow = getExpirationQueueCacheRow(nCurrentHeight, true);
        for (CClaimTrieQueueUndo::iterator itExpireUndo = expireUndo.begin(); itExpireUndo != expireUndo.end(); ++itExpireUndo)
        {
            insertClaimIntoTrie(itExpireUndo->name, itExpireUndo->val);
            itExpireRow->second.push_back(*itExpireUndo);
        }
    }
    if (insertSupportUndo.begin() != insertSupportUndo.end())
    {
        supportValueQueueType::iterator itSupportRow = getSupportQueueCacheRow(nCurrentHeight, true);
        for (CSupportValueQueueUndo::iterator itSupportUndo = insertSupportUndo.begin(); itSupportUndo != insertSupportUndo.end(); ++itSupportUndo)
        {
            int nValidHeightInMap;
            assert(removeSupportFromMap(itSupportUndo->name, itSupportUndo->val.txhash, itSupportUndo->val.nOut, itSupportUndo->val.supportTxhash, itSupportUndo->val.supportnOut, itSupportUndo->val.nHeight, nValidHeightInMap));
            assert(nValidHeightInMap == itSupportUndo->val.nValidAtHeight);
            itSupportRow->second.push_back(*itSupportUndo);
        }
    }
    return true;
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
    valueQueueCache.clear();
    expirationQueueCache.clear();
    supportCache.clear();
    supportQueueCache.clear();
    return true;
}

bool CClaimTrieCache::flush()
{
    if (dirty())
        getMerkleHash();
    bool success = base->update(cache, cacheHashes, getBestBlock(), valueQueueCache, expirationQueueCache, nCurrentHeight, supportCache, supportQueueCache);
    if (success)
    {
        success = clear();
    }
    return success;
}
