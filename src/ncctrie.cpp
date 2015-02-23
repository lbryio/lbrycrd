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

bool CNCCTrieNode::removeValue(CNodeValue val, bool * pfChanged)
{
    bool fChanged = false;

    CNodeValue currentTop = values.front();

    std::vector<CNodeValue>::iterator position = std::find(values.begin(), values.end(), val);
    if (position != values.end())
        values.erase(position);
    else
    {
        LogPrintf("CNCCTrieNode::removeValue() : asked to remove a value that doesn't exist");
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

bool CNCCTrieNode::getValue(CNodeValue& value) const
{
    if (values.empty())
        return false;
    else
    {
        value = values.front();
        return true;
    }
}

uint256 CNCCTrie::getMerkleHash()
{
    return root.hash;
}

bool CNCCTrie::empty() const
{
    return root.empty();
}

bool CNCCTrie::recursiveDumpToJSON(const std::string& name, const CNCCTrieNode* current, json_spirit::Array& ret) const
{
    using namespace json_spirit;
    Object objNode;
    objNode.push_back(Pair("name", name));
    objNode.push_back(Pair("hash", current->hash.GetHex()));
    CNodeValue val;
    if (current->getValue(val))
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
    return current->getValue(val);
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
    bool hasValue = node->getValue(val);

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

bool CNCCTrie::update(nodeCacheType& cache, hashMapType& hashes, const uint256& hashBlockIn)
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
    // As far as saving to disk goes, the idea is to use the list of
    // hashes to construct a list of (pointers to) nodes that have been
    // altered in the update, and to construct a list of names of nodes
    // that have been deleted, and to use a leveldb batch to write them
    // all to disk. As of right now, txundo stuff will be handled by
    // appending extra data to the normal txundo, which will call the
    // normal insert/remove names, but obviously the opposite and in
    // reverse order (though the order shouldn't ever matter).
    bool success = true;
    std::vector<std::string> deletedNames;
    for (nodeCacheType::iterator itcache = cache.begin(); itcache != cache.end(); ++itcache)
    {
        success = updateName(itcache->first, itcache->second, deletedNames);
        if (!success)
            return false;
    }
    nodeCacheType changedNodes;
    for (hashMapType::iterator ithash = hashes.begin(); ithash != hashes.end(); ++ithash)
    {
        CNCCTrieNode* pNode;
        success = updateHash(ithash->first, ithash->second, &pNode);
        if (!success)
            return false;
        changedNodes[ithash->first] = pNode;
    }
    BatchWrite(changedNodes, deletedNames, hashBlockIn);
    hashBlock = hashBlockIn;
    return true;
}

bool CNCCTrie::updateName(const std::string &name, CNCCTrieNode* updatedNode, std::vector<std::string>& deletedNames)
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
            if (!recursiveNullify(itchild->second, newName, deletedNames))
                return false;
            current->children.erase(itchild++);
        }
        else
            ++itchild;
    }
    return true;
}

bool CNCCTrie::recursiveNullify(CNCCTrieNode* node, std::string& name, std::vector<std::string>& deletedNames)
{
    assert(node != NULL);
    for (nodeMapType::iterator itchild = node->children.begin(); itchild != node->children.end(); ++itchild)
    {
        std::stringstream ss;
        ss << name << itchild->first;
        std::string newName = ss.str();
        if (!recursiveNullify(itchild->second, newName, deletedNames))
            return false;
    }
    node->children.clear();
    delete node;
    deletedNames.push_back(name);
    return true;
}

bool CNCCTrie::updateHash(const std::string& name, uint256& hash, CNCCTrieNode** pNodeRet)
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
    *pNodeRet = current;
    return true;
}

void BatchWriteNode(CLevelDBBatch& batch, const std::string& name, const CNCCTrieNode* pNode)
{
    batch.Write(std::make_pair('n', name), *pNode);
}

void BatchEraseNode(CLevelDBBatch& batch, const std::string& name)
{
    batch.Erase(std::make_pair('n', name));
}

bool CNCCTrie::BatchWrite(nodeCacheType& changedNodes, std::vector<std::string>& deletedNames, const uint256& hashBlockIn)
{
    CLevelDBBatch batch;
    for (nodeCacheType::iterator itcache = changedNodes.begin(); itcache != changedNodes.end(); ++itcache)
        BatchWriteNode(batch, itcache->first, itcache->second);
    for (std::vector<std::string>::iterator itname = deletedNames.begin(); itname != deletedNames.end(); ++itname)
        BatchEraseNode(batch, *itname);
    batch.Write('h', hashBlockIn);
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
        if( checkConsistency())
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
    bool hasValue = tnCurrent->getValue(val);

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

bool CNCCTrieCache::insertName(const std::string name, uint256 txhash, int nOut, CAmount nAmount, int nHeight) const
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
    currentNode->insertValue(CNodeValue(txhash, nOut, nAmount, nHeight), &fChanged);
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

bool CNCCTrieCache::removeName(const std::string name, uint256 txhash, int nOut) const
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
    bool success = currentNode->removeValue(CNodeValue(txhash, nOut), &fChanged);
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
    return true;
}

bool CNCCTrieCache::flush()
{
    if (dirty())
        getMerkleHash();
    bool success = base->update(cache, cacheHashes, hashBlock);
    if (success)
    {
        success = clear();
    }
    return success;
}
