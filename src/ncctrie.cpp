#include "ncctrie.h"
#include "leveldbwrapper.h"

#include <boost/scoped_ptr.hpp>

std::string CNodeValue::ToString()
{
    std::stringstream ss;
    ss << nOut;
    return txhash.ToString() + ss.str();
}

bool CNCCTrieNode::insertValue(CNodeValue val, bool * pfChanged)
{
    LogPrintf("%s: Inserting %s:%d (amount: %d)  into the ncc trie\n", __func__, val.txhash.ToString(), val.nOut, val.nAmount);
    bool fChanged = false;
    
    if (values.empty())
    {
        values.push_back(val);
        fChanged = true;
    }
    else
    {
        CNodeValue currentTop = values.front();
        values.push_back(val);
        std::make_heap(values.begin(), values.end());
        if (currentTop != values.front())
            fChanged = true;
    }
    if (pfChanged)
        *pfChanged = fChanged;
    return true;
}

bool CNCCTrieNode::removeValue(uint256& txhash, uint32_t nOut, CNodeValue& val, bool * pfChanged)
{
    LogPrintf("%s: Removing %s from the ncc trie\n", __func__, val.ToString());
    bool fChanged = false;

    CNodeValue currentTop = values.front();

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
        LogPrintf("CNCCTrieNode::removeValue() : asked to remove a value that doesn't exist\n");
        LogPrintf("CNCCTrieNode::removeValue() : value that doesn't exist: %s.\n", val.ToString());
        LogPrintf("CNCCTrieNode::removeValue() : values that do exist:\n");
        for (unsigned int i = 0; i < values.size(); i++)
        {
            LogPrintf("%s\n", values[i].ToString());
        }
        return false;
    }
    if (!values.empty())
    {
        std::make_heap(values.begin(), values.end());
        if (currentTop != values.front())
            fChanged = true;
    }
    else
        fChanged = true;
    if (pfChanged)
        *pfChanged = fChanged;
    return true;
}

bool CNCCTrieNode::getBestValue(CNodeValue& value) const
{
    if (values.empty())
        return false;
    else
    {
        value = values.front();
        return true;
    }
}

bool CNCCTrieNode::haveValue(const uint256& txhash, uint32_t nOut) const
{
    for (std::vector<CNodeValue>::const_iterator itval = values.begin(); itval != values.end(); ++itval)
        if (itval->txhash == txhash && itval->nOut == nOut)
            return true;
    return false;
}

uint256 CNCCTrie::getMerkleHash()
{
    return root.hash;
}

bool CNCCTrie::empty() const
{
    return root.empty();
}

bool CNCCTrie::queueEmpty() const
{
    return valueQueue.empty();
}

void CNCCTrie::clear()
{
    clear(&root);
}

void CNCCTrie::clear(CNCCTrieNode* current)
{
    for (nodeMapType::const_iterator itchildren = current->children.begin(); itchildren != current->children.end(); ++itchildren)
    {
        clear(itchildren->second);
        delete itchildren->second;
    }
}

bool CNCCTrie::haveClaim(const std::string& name, const uint256& txhash, uint32_t nOut) const
{
    const CNCCTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end())
            return false;
        current = itchildren->second;
    }
    return current->haveValue(txhash, nOut);
}

bool CNCCTrie::recursiveDumpToJSON(const std::string& name, const CNCCTrieNode* current, json_spirit::Array& ret) const
{
    using namespace json_spirit;
    Object objNode;
    objNode.push_back(Pair("name", name));
    objNode.push_back(Pair("hash", current->hash.GetHex()));
    CNodeValue val;
    if (current->getBestValue(val))
    {
        objNode.push_back(Pair("txid", val.txhash.GetHex()));
        objNode.push_back(Pair("n", (int)val.nOut));
        objNode.push_back(Pair("value", val.nAmount));
        objNode.push_back(Pair("height", val.nHeight));
    }
    ret.push_back(objNode);
    for (nodeMapType::const_iterator it = current->children.begin(); it != current->children.end(); ++it)
    {
        std::stringstream ss;
        ss << name << it->first;
        if (!recursiveDumpToJSON(ss.str(), it->second, ret))
            return false;
    }
    return true;
}

json_spirit::Array CNCCTrie::dumpToJSON() const
{
    json_spirit::Array ret;
    if (!recursiveDumpToJSON("", &root, ret))
        LogPrintf("%s: Something went wrong dumping to JSON", __func__);
    return ret;
}

bool CNCCTrie::getInfoForName(const std::string& name, CNodeValue& val) const
{
    const CNCCTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::const_iterator itchildren = current->children.find(*itname);
        if (itchildren == current->children.end())
            return false;
        current = itchildren->second;
    }
    return current->getBestValue(val);
}

bool CNCCTrie::checkConsistency()
{
    if (empty())
        return true;
    return recursiveCheckConsistency(&root);
}

bool CNCCTrie::recursiveCheckConsistency(CNCCTrieNode* node)
{
    std::string stringToHash;

    for (nodeMapType::iterator it = node->children.begin(); it != node->children.end(); ++it)
    {
        std::stringstream ss;
        ss << it->first;
        if (recursiveCheckConsistency(it->second))
        {
            stringToHash += ss.str();
            stringToHash += it->second->hash.ToString();
        }
        else
            return false;
    }

    CNodeValue val;
    bool hasValue = node->getBestValue(val);

    if (hasValue)
    {
        CHash256 valHasher;
        std::vector<unsigned char> vchValHash(valHasher.OUTPUT_SIZE);
        valHasher.Write((const unsigned char*) val.ToString().data(), val.ToString().size());
        valHasher.Finalize(&(vchValHash[0]));
        uint256 valHash(vchValHash);
        stringToHash += valHash.ToString();
    }

    CHash256 hasher;
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Write((const unsigned char*) stringToHash.data(), stringToHash.size());
    hasher.Finalize(&(vchHash[0]));
    uint256 calculatedHash(vchHash);
    return calculatedHash == node->hash;
}

valueQueueType::iterator CNCCTrie::getQueueRow(int nHeight, bool createIfNotExists)
{
    valueQueueType::iterator itQueueRow = valueQueue.find(nHeight);
    if (itQueueRow == valueQueue.end())
    {
        if (!createIfNotExists)
            return itQueueRow;
        std::vector<CValueQueueEntry> queueRow;
        std::pair<valueQueueType::iterator, bool> ret;
        ret = valueQueue.insert(std::pair<int, std::vector<CValueQueueEntry> >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

void CNCCTrie::deleteQueueRow(int nHeight)
{
    valueQueueType::iterator itQueueRow = valueQueue.find(nHeight);
    if (itQueueRow != valueQueue.end())
    {
        valueQueue.erase(itQueueRow);
    }
}

bool CNCCTrie::update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlockIn, valueQueueType& queueCache, int nNewHeight)
{
    // General strategy: the cache is ordered by length, ensuring child
    // nodes are always inserted after their parents. Insert each node
    // one at a time. When updating a node, swap its values with those
    // of the cached node and delete all characters (and their children
    // and so forth) which don't exist in the updated node. When adding
    // a new node, make sure that its <character, CNCCTrieNode*> pair
    // gets into the parent's children.
    // Then, update all of the given hashes.
    // This can probably be optimized by checking each substring against
    // the caches each time, but that will come after this is shown to
    // work correctly.
    // Disk strategy: keep a map of <string: dirty node>, where
    // any nodes that are changed get put into the map, and any nodes
    // to be deleted will simply be empty (no value, no children). Nodes
    // whose hashes change will also be inserted into the map.
    // As far as the queue goes, just keep a list of dirty queue entries.
    // When the time comes, send all of that to disk in one batch, and
    // empty the map/list.
    
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
        if (itQueueCacheRow->second.empty())
        {
            deleteQueueRow(itQueueCacheRow->first);
        }
        else
        {
            valueQueueType::iterator itQueueRow = getQueueRow(itQueueCacheRow->first, true);
            itQueueRow->second.swap(itQueueCacheRow->second);
        }
        vDirtyQueueRows.push_back(itQueueCacheRow->first);
    }
    hashBlock = hashBlockIn;
    nCurrentHeight = nNewHeight;
    return true;
}

void CNCCTrie::markNodeDirty(const std::string &name, CNCCTrieNode* node)
{
    std::pair<nodeCacheType::iterator, bool> ret;
    ret = dirtyNodes.insert(std::pair<std::string, CNCCTrieNode*>(name, node));
    if (ret.second == false)
        ret.first->second = node;
}

bool CNCCTrie::updateName(const std::string &name, CNCCTrieNode* updatedNode)
{
    CNCCTrieNode* current = &root;
    for (std::string::const_iterator itname = name.begin(); itname != name.end(); ++itname)
    {
        nodeMapType::iterator itchild = current->children.find(*itname);
        if (itchild == current->children.end())
        {
            if (itname + 1 == name.end())
            {
                CNCCTrieNode* newNode = new CNCCTrieNode();
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

bool CNCCTrie::recursiveNullify(CNCCTrieNode* node, std::string& name)
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

bool CNCCTrie::updateHash(const std::string& name, uint256& hash)
{
    CNCCTrieNode* current = &root;
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

void CNCCTrie::BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CNCCTrieNode* pNode) const
{
    LogPrintf("%s: Writing %s to disk with %d values\n", __func__, name, pNode->values.size());
    if (pNode)
        batch.Write(std::make_pair('n', name), *pNode);
    else
        batch.Erase(std::make_pair('n', name));
}

void CNCCTrie::BatchWriteQueueRow(CLevelDBBatch& batch, int nRowNum)
{
    valueQueueType::iterator itQueueRow = getQueueRow(nRowNum, false);
    if (itQueueRow != valueQueue.end())
        batch.Write(std::make_pair('r', nRowNum), itQueueRow->second);
    else
        batch.Erase(std::make_pair('r', nRowNum));
}

bool CNCCTrie::WriteToDisk()
{
    CLevelDBBatch batch;
    for (nodeCacheType::iterator itcache = dirtyNodes.begin(); itcache != dirtyNodes.end(); ++itcache)
        BatchWriteNode(batch, itcache->first, itcache->second);
    dirtyNodes.clear();
    for (std::vector<int>::iterator itRowNum = vDirtyQueueRows.begin(); itRowNum != vDirtyQueueRows.end(); ++itRowNum)
        BatchWriteQueueRow(batch, *itRowNum);
    vDirtyQueueRows.clear();
    batch.Write('h', hashBlock);
    batch.Write('t', nCurrentHeight);
    return db.WriteBatch(batch);
}

bool CNCCTrie::InsertFromDisk(const std::string& name, CNCCTrieNode* node)
{
    if (name.size() == 0)
    {
        root = *node;
        return true;
    }
    CNCCTrieNode* current = &root;
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

bool CNCCTrie::ReadFromDisk(bool check)
{
    if (!db.Read('h', hashBlock))
        LogPrintf("%s: Couldn't read the best block's hash\n", __func__);
    if (!db.Read('t', nCurrentHeight))
        LogPrintf("%s: Couldn't read the current height\n", __func__);
    boost::scoped_ptr<leveldb::Iterator> pcursor(const_cast<CLevelDBWrapper*>(&db)->NewIterator());
    pcursor->SeekToFirst();
    
    while (pcursor->Valid())
    {
        try
        {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data()+slKey.size(), SER_DISK, CLIENT_VERSION);
            char chType;
            ssKey >> chType;
            if (chType == 'n')
            {
                leveldb::Slice slValue = pcursor->value();
                std::string name;
                ssKey >> name;
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                CNCCTrieNode* node = new CNCCTrieNode();
                ssValue >> *node;
                if (!InsertFromDisk(name, node))
                    return false;
            }
            else if (chType == 'r')
            {
                leveldb::Slice slValue = pcursor->value();
                int nHeight;
                ssKey >> nHeight;
                CDataStream ssValue(slValue.data(), slValue.data()+slValue.size(), SER_DISK, CLIENT_VERSION);
                valueQueueType::iterator itQueueRow = getQueueRow(nHeight, true);
                ssValue >> itQueueRow->second;
            }
            pcursor->Next();
        }
        catch (const std::exception& e)
        {
            return error("%s: Deserialize or I/O error - %s", __func__, e.what());
        }

    }
    if (check)
    {
        LogPrintf("Checking NCC trie consistency...");
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

bool CNCCTrieCache::recursiveComputeMerkleHash(CNCCTrieNode* tnCurrent, std::string sPos) const
{
    if (sPos == "" && tnCurrent->empty())
    {
        cacheHashes[""] = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
        return true;
    }
    std::string stringToHash;

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
        stringToHash += ss.str();
        hashMapType::iterator ithash = cacheHashes.find(sNextPos);
        if (ithash != cacheHashes.end())
            stringToHash += ithash->second.ToString();
        else
            stringToHash += it->second->hash.ToString();
    }
    
    CNodeValue val;
    bool hasValue = tnCurrent->getBestValue(val);

    if (hasValue)
    {
        CHash256 valHasher;
        std::vector<unsigned char> vchValHash(valHasher.OUTPUT_SIZE);
        valHasher.Write((const unsigned char*) val.ToString().data(), val.ToString().size());
        valHasher.Finalize(&(vchValHash[0]));
        uint256 valHash(vchValHash);
        stringToHash += valHash.ToString();
    }

    CHash256 hasher;
    std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
    hasher.Write((const unsigned char*) stringToHash.data(), stringToHash.size());
    hasher.Finalize(&(vchHash[0]));
    cacheHashes[sPos] = uint256(vchHash);
    std::set<std::string>::iterator itDirty = dirtyHashes.find(sPos);
    if (itDirty != dirtyHashes.end())
        dirtyHashes.erase(itDirty);
    return true;
}

uint256 CNCCTrieCache::getMerkleHash() const
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

bool CNCCTrieCache::empty() const
{
    return base->empty() && cache.empty();
}

bool CNCCTrieCache::insertClaimIntoTrie(const std::string name, CNodeValue val) const
{
    assert(base);
    CNCCTrieNode* currentNode = &(base->root);
    nodeCacheType::iterator cachedNode;
    cachedNode = cache.find("");
    if (cachedNode != cache.end())
        currentNode = cachedNode->second;
    if (currentNode == NULL)
    {
        currentNode = new CNCCTrieNode();
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
            currentNode = new CNCCTrieNode(*currentNode);
            cache[sCurrentSubstring] = currentNode;
        }
        CNCCTrieNode* newNode = new CNCCTrieNode();
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
        currentNode = new CNCCTrieNode(*currentNode);
        cache[name] = currentNode;
    }
    bool fChanged = false;
    currentNode->insertValue(val, &fChanged);
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

bool CNCCTrieCache::removeClaimFromTrie(const std::string name, uint256 txhash, uint32_t nOut, int& nValidAtHeight) const
{
    assert(base);
    CNCCTrieNode* currentNode = &(base->root);
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
        currentNode = new CNCCTrieNode(*currentNode);
        cache[name] = currentNode;
    }
    bool fChanged = false;
    assert(currentNode != NULL);
    CNodeValue val;
    bool success = currentNode->removeValue(txhash, nOut, val, &fChanged);
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
    CNCCTrieNode* rootNode = &(base->root);
    cachedNode = cache.find("");
    if (cachedNode != cache.end())
        rootNode = cachedNode->second;
    return recursivePruneName(rootNode, 0, name);
}

bool CNCCTrieCache::recursivePruneName(CNCCTrieNode* tnCurrent, unsigned int nPos, std::string sName, bool* pfNullified) const
{
    bool fNullified = false;
    std::string sCurrentSubstring = sName.substr(0, nPos);
    if (nPos < sName.size())
    {
        std::string sNextSubstring = sName.substr(0, nPos + 1);
        unsigned char cNext = sName.at(nPos);
        CNCCTrieNode* tnNext = NULL;
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
                tnCurrent = new CNCCTrieNode(*tnCurrent);
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

valueQueueType::iterator CNCCTrieCache::getQueueCacheRow(int nHeight, bool createIfNotExists) const
{
    valueQueueType::iterator itQueueRow = valueQueueCache.find(nHeight);
    if (itQueueRow == valueQueueCache.end())
    {
        // Have to make a new row it put in the cache, if createIfNotExists is true
        std::vector<CValueQueueEntry> queueRow;
        // If the row exists in the base, copy its values into the new row.
        valueQueueType::iterator itBaseQueueRow = base->valueQueue.find(nHeight);
        if (itBaseQueueRow == base->valueQueue.end())
        {
            if (!createIfNotExists)
                return itQueueRow;
        }
        else
            queueRow = itBaseQueueRow->second;
        // Stick the new row in the cache
        std::pair<valueQueueType::iterator, bool> ret;
        ret = valueQueueCache.insert(std::pair<int, std::vector<CValueQueueEntry> >(nHeight, queueRow));
        assert(ret.second);
        itQueueRow = ret.first;
    }
    return itQueueRow;
}

bool CNCCTrieCache::getInfoForName(const std::string name, CNodeValue& val) const
{
    nodeCacheType::iterator itcache = cache.find(name);
    if (itcache != cache.end())
    {
        return itcache->second->getBestValue(val);
    }
    else
    {
        return base->getInfoForName(name, val);
    }
}

bool CNCCTrieCache::addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    return addClaimToQueue(name, txhash, nOut, nAmount, nHeight, nHeight + DEFAULT_DELAY);
}

bool CNCCTrieCache::addClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, uint256 prevTxhash, uint32_t nPrevOut) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nCurrentHeight);
    assert(nHeight == nCurrentHeight);
    CNodeValue val;
    if (getInfoForName(name, val))
    {
        if (val.txhash == prevTxhash && val.nOut == nPrevOut)
        {
            LogPrintf("%s: This is an update to a best claim. Previous claim txhash: %s, nOut: %d\n", __func__, prevTxhash.GetHex(), nPrevOut);
            return addClaimToQueue(name, txhash, nOut, nAmount, nHeight, nHeight);
        }
    }
    return addClaim(name, txhash, nOut, nAmount, nHeight);
}

bool CNCCTrieCache::undoSpendClaim(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %d, nAmount: %d, nHeight: %d, nValidAtHeight: %d, nCurrentHeight: %d\n", __func__, name, txhash.GetHex(), nOut, nAmount, nHeight, nValidAtHeight, nCurrentHeight);
    if (nValidAtHeight < nCurrentHeight)
    {
        CNodeValue val(txhash, nOut, nAmount, nHeight, nValidAtHeight);
        CValueQueueEntry entry(name, val);
        insertClaimIntoTrie(name, CNodeValue(txhash, nOut, nAmount, nHeight, nValidAtHeight));
    }
    else
    {
        addClaimToQueue(name, txhash, nOut, nAmount, nHeight, nValidAtHeight);
    }
    return true;
}

bool CNCCTrieCache::addClaimToQueue(const std::string name, uint256 txhash, uint32_t nOut, CAmount nAmount, int nHeight, int nValidAtHeight) const
{
    LogPrintf("%s: nValidAtHeight: %d\n", __func__, nValidAtHeight);
    CNodeValue val(txhash, nOut, nAmount, nHeight, nValidAtHeight);
    CValueQueueEntry entry(name, val);
    valueQueueType::iterator itQueueRow = getQueueCacheRow(nValidAtHeight, true);
    itQueueRow->second.push_back(entry);
    return true;
}

bool CNCCTrieCache::removeClaimFromQueue(const std::string name, uint256 txhash, uint32_t nOut, int nHeightToCheck, int& nValidAtHeight) const
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

bool CNCCTrieCache::undoAddClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight) const
{
    int throwaway;
    return removeClaim(name, txhash, nOut, nHeight, throwaway);
}

bool CNCCTrieCache::spendClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    return removeClaim(name, txhash, nOut, nHeight, nValidAtHeight);
}

bool CNCCTrieCache::removeClaim(const std::string name, uint256 txhash, uint32_t nOut, int nHeight, int& nValidAtHeight) const
{
    LogPrintf("%s: name: %s, txhash: %s, nOut: %s, nHeight: %s, nCurrentHeight: %s\n", __func__, name, txhash.GetHex(), nOut, nHeight, nCurrentHeight);
    if (nHeight + DEFAULT_DELAY >= nCurrentHeight)
    {
        if (removeClaimFromQueue(name, txhash, nOut, nHeight + DEFAULT_DELAY, nValidAtHeight))
            return true;
        if (removeClaimFromQueue(name, txhash, nOut, nHeight, nValidAtHeight))
            return true;
    }
    if (removeClaimFromQueue(name, txhash, nOut, nHeight, nCurrentHeight))
        return true;
    return removeClaimFromTrie(name, txhash, nOut, nValidAtHeight);
}

bool CNCCTrieCache::incrementBlock(CNCCTrieQueueUndo& undo) const
{
    LogPrintf("%s: nCurrentHeight (before increment): %d\n", __func__, nCurrentHeight);
    valueQueueType::iterator itQueueRow = getQueueCacheRow(nCurrentHeight, false);
    nCurrentHeight++;
    if (itQueueRow == valueQueueCache.end())
    {
        return true;
    }
    for (std::vector<CValueQueueEntry>::iterator itEntry = itQueueRow->second.begin(); itEntry != itQueueRow->second.end(); ++itEntry)
    {
        insertClaimIntoTrie(itEntry->name, itEntry->val);
        undo.push_back(*itEntry);
    }
    itQueueRow->second.clear();
    return true;
}

bool CNCCTrieCache::decrementBlock(CNCCTrieQueueUndo& undo) const
{
    LogPrintf("%s: nCurrentHeight (before decrement): %d\n", __func__, nCurrentHeight);
    nCurrentHeight--;
    valueQueueType::iterator itQueueRow = getQueueCacheRow(nCurrentHeight, true);
    for (CNCCTrieQueueUndo::iterator itUndo = undo.begin(); itUndo != undo.end(); ++itUndo)
    {
        int nValidHeightInTrie;
        assert(removeClaimFromTrie(itUndo->name, itUndo->val.txhash, itUndo->val.nOut, nValidHeightInTrie));
        assert(nValidHeightInTrie == itUndo->val.nValidAtHeight);
        itQueueRow->second.push_back(*itUndo);
    }
    return true;
}

uint256 CNCCTrieCache::getBestBlock()
{
    if (hashBlock.IsNull())
        if (base != NULL)
            hashBlock = base->hashBlock;
    return hashBlock;
}

void CNCCTrieCache::setBestBlock(const uint256& hashBlockIn)
{
    hashBlock = hashBlockIn;
}

bool CNCCTrieCache::clear() const
{
    for (nodeCacheType::iterator itcache = cache.begin(); itcache != cache.end(); ++itcache)
    {
        delete itcache->second;
    }
    cache.clear();
    dirtyHashes.clear();
    cacheHashes.clear();
    valueQueueCache.clear();
    return true;
}

bool CNCCTrieCache::flush()
{
    if (dirty())
        getMerkleHash();
    bool success = base->update(cache, cacheHashes, getBestBlock(), valueQueueCache, nCurrentHeight);
    if (success)
    {
        success = clear();
    }
    return success;
}
