
#include "claimtrie.h"
#include "claimtriedb.h"

#include <vector>
#include <iterator>
#include <boost/scoped_ptr.hpp>

template <typename K, typename V>
struct CCMap : public CCBase
{
    CCMap() : data() {}
    typedef std::vector<std::pair<K, V> > dataType;
    dataType data;
    typename dataType::iterator find(const K &key)
    {
        typename dataType::iterator itData = data.begin();
        while (itData != data.end())
        {
            if (key == itData->first)
            {
                break;
            }
            ++itData;
        }
        return itData;
    }
    void write(const size_t key, CClaimTrieDb *db)
    {
        for (typename dataType::iterator it = data.begin(); it != data.end(); ++it)
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

inline
CClaimTrieDb::CClaimTrieDb(bool fMemory, bool fWipe)
                    : CDBWrapper(GetDataDir() / "claimtrie", 100, fMemory, fWipe, false)
{
}

inline
CClaimTrieDb::~CClaimTrieDb()
{
    for (std::map<size_t, CCBase*>::iterator itQueue = queues.begin(); itQueue != queues.end(); ++itQueue)
    {
        delete itQueue->second;
    }
}

inline
void CClaimTrieDb::writeQueues()
{
    for (std::map<size_t, CCBase*>::iterator itQueue = queues.begin(); itQueue != queues.end(); ++itQueue)
    {
        itQueue->second->write(itQueue->first, this);
    }
}

template <typename K, typename V>
bool CClaimTrieDb::getQueueRow(const K &key, V &row) const
{
    const size_t hash = hashType<K, V>();
    std::map<size_t, CCBase*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end())
    {
        CCMap<K, V> *map = static_cast<CCMap<K, V>*>(itQueue->second);
        typename CCMap<K, V>::dataType::const_iterator itData = map->find(key);
        if (itData != map->data.end())
        {
            row = itData->second;
            return true;
        }
    }
    return Read(std::make_pair(hash, key), row);
}

template <typename K, typename V>
void CClaimTrieDb::updateQueueRow(const K &key, V &row)
{
    const size_t hash = hashType<K, V>();
    std::map<size_t, CCBase*>::iterator itQueue = queues.find(hash);
    if (itQueue == queues.end())
    {
        itQueue = queues.insert(itQueue, std::pair<size_t, CCBase*>(hash, new CCMap<K, V>));
    }
    CCMap<K, V> *map = static_cast<CCMap<K, V>*>(itQueue->second);
    typename CCMap<K, V>::dataType::iterator itData = map->find(key);
    if (itData == map->data.end())
    {
        itData = map->data.insert(itData, std::make_pair(key, V()));
    }
    std::swap(itData->second, row);
}

template <typename K, typename V>
bool CClaimTrieDb::keyTypeEmpty() const
{
    const size_t hash = hashType<K, V>();
    std::map<size_t, CCBase*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end())
    {
        const typename CCMap<K, V>::dataType &data = (static_cast<CCMap<K, V>*>(itQueue->second))->data;
        for (typename CCMap<K, V>::dataType::const_iterator itData = data.begin(); itData != data.end(); ++itData)
        {
            if (!itData->second.empty())
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
            if (hash == key.first)
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

template <typename K, typename V, typename C>
bool CClaimTrieDb::seekByKey(std::map<K, V, C> &map) const
{
    const size_t hash = hashType<K, V>();
    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    bool found = false;

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next())
    {
        std::pair<size_t, K> key;
        if (pcursor->GetKey(key))
        {
            if (hash == key.first)
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

template <typename K, typename V, typename C>
bool CClaimTrieDb::getQueueMap(std::map<K, V, C> &map) const
{
    const size_t hash = hashType<K, V>();
    std::map<size_t, CCBase*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end())
    {
        const typename CCMap<K, V>::dataType &data = (static_cast<CCMap<K, V>*>(itQueue->second))->data;
        std::copy(data.begin(), data.end(), std::inserter(map, map.end()));
    }

    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next())
    {
        std::pair<size_t, K> key;
        if (pcursor->GetKey(key))
        {
            if (hash == key.first)
            {
                typename std::map<K, V, C>::iterator itMap = map.find(key.second);
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
