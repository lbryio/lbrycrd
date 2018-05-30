
#include "claimtriedb.h"
#include <boost/scoped_ptr.hpp>

template <typename K, typename V>
class CCMap : public CCBase
{
    std::map<K, V> data;
public:
    CCMap() : data() {}
    std::map<K, V>& data_map() { return data; }
    void write(size_t key, CClaimTrieDb *db)
    {
        for (typename std::map<K, V>::iterator it = data.begin(); it != data.end(); ++it)
        {
            if (it->second.empty())
            {
                db->Erase(std::make_pair(key, it->first));
            }
            else
            {
                db->Write(std::make_pair(key, it->first), it->second);
            }
        }
        data.clear();
    }
};

CClaimTrieDb::CClaimTrieDb(bool fMemory, bool fWipe)
            : CDBWrapper(GetDataDir() / "claimtrie", 100, fMemory, fWipe, false)
{}

CClaimTrieDb::~CClaimTrieDb()
{
    for (std::map<size_t, CCBase*>::iterator itQueue = queues.begin(); itQueue != queues.end(); ++itQueue)
    {
        delete itQueue->second;
    }
}

void CClaimTrieDb::writeQueues()
{
    for (std::map<size_t, CCBase*>::iterator itQueue = queues.begin(); itQueue != queues.end(); ++itQueue)
    {
        itQueue->second->write(itQueue->first, this);
    }
}

template <typename K, typename V> bool CClaimTrieDb::getQueueRow(const K &key, V &row) const
{
    const size_t hash = boost::typeindex::type_id<std::map<K, V> >().hash_code();
    std::map<size_t, CCBase*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end())
    {
        std::map<K, V> &map = (static_cast<CCMap<K, V>*>(itQueue->second))->data_map();
        typename std::map<K, V>::iterator itMap = map.find(key);
        if (itMap != map.end())
        {
            row = itMap->second;
            return true;
        }
    }
    return Read(std::make_pair(hash, key), row);
}

template <typename K, typename V> void CClaimTrieDb::updateQueueRow(const K &key, V &row)
{
    const size_t hash = boost::typeindex::type_id<std::map<K, V> >().hash_code();
    std::map<size_t, CCBase*>::iterator itQueue = queues.find(hash);
    if (itQueue == queues.end())
    {
        itQueue = queues.insert(itQueue, std::pair<size_t, CCBase*>(hash, new CCMap<K, V>));
    }
    std::map<K, V> &map = (static_cast<CCMap<K, V>*>(itQueue->second))->data_map();
    typename std::map<K, V>::iterator itMap = map.find(key);
    if (itMap == map.end())
    {
        itMap = map.insert(itMap, std::make_pair(key, V()));
    }
    std::swap(itMap->second, row);
}

template <typename K, typename V> bool CClaimTrieDb::keyTypeEmpty() const
{
    const size_t hash = boost::typeindex::type_id<std::map<K, V> >().hash_code();
    std::map<size_t, CCBase*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end())
    {
        std::map<K, V> &map = (static_cast<CCMap<K, V>*>(itQueue->second))->data_map();
        for (typename std::map<K, V>::iterator itMap = map.begin(); itMap != map.end(); ++itMap)
        {
            if (!itMap->second.empty())
            {
                return false;
            }
        }
    }

    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next())
    {
        std::pair<size_t, K> key;
        if (pcursor->GetKey(key))
        {
            if (key.first == hash)
            {
                return false;
            }
        }
        else
        {
            break;
        }
    }
    return true;
}

template <typename K, typename V> bool CClaimTrieDb::seekByKey(std::map<K, V> &map) const
{
    const size_t hash = boost::typeindex::type_id<std::map<K, V> >().hash_code();
    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    bool found = false;

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next())
    {
        std::pair<size_t, K> key;
        if (pcursor->GetKey(key))
        {
            if (key.first == hash)
            {
                V value;
                if (pcursor->GetValue(value))
                {
                    found = true;
                    map.insert(std::make_pair(key.second, value));
                }
                else
                {
                    return false;
                }
            }
        }
    }
    return found;
}

template <typename K, typename V> bool CClaimTrieDb::getQueueMap(std::map<K,V> &map) const
{
    const size_t hash = boost::typeindex::type_id<std::map<K, V> >().hash_code();
    std::map<size_t, CCBase*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end())
    {
        map = (static_cast<CCMap<K, V>*>(itQueue->second))->data_map();
    }

    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next())
    {
        std::pair<size_t, K> key;
        if (pcursor->GetKey(key))
        {
            if (key.first == hash)
            {
                typename std::map<K, V>::iterator itMap = map.find(key.second);
                if (itMap != map.end())
                {
                    continue;
                }
                V value;
                if (pcursor->GetValue(value))
                {
                    map.insert(itMap, std::make_pair(key.second, value));
                }
                else
                {
                    return false;
                }
            }
        }
    }
    return true;
}
