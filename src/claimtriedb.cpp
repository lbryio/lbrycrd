
#include "claimtriedb.h"
#include "claimtrie.h"

#include <boost/scoped_ptr.hpp>
#include <iterator>
#include <list>

/**
 * Function to provide unique char of types pair
 */
template <class T1, class T2>
unsigned char hashType();

#define TEMPLATE_HASH_TYPE(type1, type2, ret) \
    template <>                               \
    unsigned char hashType<type1, type2>()    \
    {                                         \
        return ret;                           \
    }

TEMPLATE_HASH_TYPE(std::string, CClaimTrieNode, 'n')
TEMPLATE_HASH_TYPE(uint160, CClaimIndexElement, 'i')
TEMPLATE_HASH_TYPE(int, claimQueueRowType, 'r')
TEMPLATE_HASH_TYPE(std::string, queueNameRowType, 'm')
TEMPLATE_HASH_TYPE(int, expirationQueueRowType, 'e')
TEMPLATE_HASH_TYPE(std::string, supportMapEntryType, 's')
TEMPLATE_HASH_TYPE(int, supportQueueRowType, 'u')
TEMPLATE_HASH_TYPE(std::string, supportQueueNameRowType, 'p')
TEMPLATE_HASH_TYPE(int, supportExpirationQueueRowType, 'x')

/*
 * Concrete implementation of buffer
 */
template <typename K, typename V>
class CActualBuffer : public CAbstractBuffer
{
public:
    CActualBuffer() : data()
    {
    }

    typedef std::list<std::pair<K, V> > dataType;

    typename dataType::iterator find(const K& key)
    {
        typename dataType::iterator itData = data.begin();
        while (itData != data.end()) {
            if (key == itData->first) break;
            ++itData;
        }
        return itData;
    }

    typename dataType::iterator end()
    {
        return data.end();
    }

    typename dataType::iterator push_back(const std::pair<K, V>& element)
    {
        return data.insert(data.end(), element);
    }

    void write(const unsigned char key, CClaimTrieDb* db)
    {
        for (typename dataType::iterator it = data.begin(); it != data.end(); ++it) {
            if (it->second.empty()) {
                db->Erase(std::make_pair(key, it->first));
            } else {
                db->Write(std::make_pair(key, it->first), it->second);
            }
        }
        data.clear();
    }

    dataType data;
};

CClaimTrieDb::CClaimTrieDb(bool fMemory, bool fWipe)
    : CDBWrapper(GetDataDir() / "claimtrie", 100, fMemory, fWipe, false)
{
}

CClaimTrieDb::~CClaimTrieDb()
{
    for (std::map<unsigned char, CAbstractBuffer*>::iterator itQueue = queues.begin(); itQueue != queues.end(); ++itQueue) {
        delete itQueue->second;
    }
}

void CClaimTrieDb::writeQueues()
{
    for (std::map<unsigned char, CAbstractBuffer*>::iterator itQueue = queues.begin(); itQueue != queues.end(); ++itQueue) {
        itQueue->second->write(itQueue->first, this);
    }
}

template <typename K, typename V>
bool CClaimTrieDb::getQueueRow(const K& key, V& row) const
{
    const unsigned char hash = hashType<K, V>();
    std::map<unsigned char, CAbstractBuffer*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end()) {
        CActualBuffer<K, V>* map = static_cast<CActualBuffer<K, V>*>(itQueue->second);
        typename CActualBuffer<K, V>::dataType::const_iterator itData = map->find(key);
        if (itData != map->end()) {
            row = itData->second;
            return true;
        }
    }
    return Read(std::make_pair(hash, key), row);
}

template <typename K, typename V>
void CClaimTrieDb::updateQueueRow(const K& key, V& row)
{
    const unsigned char hash = hashType<K, V>();
    std::map<unsigned char, CAbstractBuffer*>::iterator itQueue = queues.find(hash);
    if (itQueue == queues.end()) {
        itQueue = queues.insert(itQueue, std::pair<unsigned char, CAbstractBuffer*>(hash, new CActualBuffer<K, V>));
    }
    CActualBuffer<K, V>* map = static_cast<CActualBuffer<K, V>*>(itQueue->second);
    typename CActualBuffer<K, V>::dataType::iterator itData = map->find(key);
    if (itData == map->end()) {
        itData = map->push_back(std::make_pair(key, V()));
    }
    std::swap(itData->second, row);
}

template <typename K, typename V>
bool CClaimTrieDb::keyTypeEmpty() const
{
    const unsigned char hash = hashType<K, V>();
    std::map<unsigned char, CAbstractBuffer*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end()) {
        const typename CActualBuffer<K, V>::dataType& data = (static_cast<CActualBuffer<K, V>*>(itQueue->second))->data;
        for (typename CActualBuffer<K, V>::dataType::const_iterator itData = data.begin(); itData != data.end(); ++itData) {
            if (!itData->second.empty()) return false;
        }
    }

    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<unsigned char, K> key;
        if (pcursor->GetKey(key)) {
            if (hash == key.first) return false;
        } else {
            break;
        }
    }
    return true;
}

template <typename K, typename V, typename C>
bool CClaimTrieDb::seekByKey(std::map<K, V, C>& map) const
{
    const unsigned char hash = hashType<K, V>();
    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<unsigned char, K> key;
        if (pcursor->GetKey(key)) {
            if (hash == key.first) {
                V value;
                if (!pcursor->GetValue(value)) return false;
                map.insert(std::make_pair(key.second, value));
            }
        }
    }
    return true;
}

template <typename K, typename V, typename C>
bool CClaimTrieDb::getQueueMap(std::map<K, V, C>& map) const
{
    const unsigned char hash = hashType<K, V>();
    std::map<unsigned char, CAbstractBuffer*>::const_iterator itQueue = queues.find(hash);
    if (itQueue != queues.end()) {
        const typename CActualBuffer<K, V>::dataType& data = (static_cast<CActualBuffer<K, V>*>(itQueue->second))->data;
        std::copy(data.begin(), data.end(), std::inserter(map, map.end()));
    }

    boost::scoped_ptr<CDBIterator> pcursor(const_cast<CClaimTrieDb*>(this)->NewIterator());

    for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()) {
        std::pair<unsigned char, K> key;
        if (pcursor->GetKey(key)) {
            if (hash == key.first) {
                typename std::map<K, V, C>::iterator itMap = map.find(key.second);
                if (itMap != map.end()) continue;
                V value;
                if (!pcursor->GetValue(value)) return false;
                map.insert(itMap, std::make_pair(key.second, value));
            }
        }
    }
    return true;
}

/*
 * Explicit template instantiation for every pair of types
 */
#define TEMPLATE_INSTANTIATE2(type1, type2)                                        \
    template bool CClaimTrieDb::getQueueMap<>(std::map<type1, type2> & map) const; \
    template bool CClaimTrieDb::getQueueRow<>(const type1& key, type2& row) const; \
    template void CClaimTrieDb::updateQueueRow<>(const type1& key, type2& row);    \
    template bool CClaimTrieDb::keyTypeEmpty<type1, type2>() const;                \
    template bool CClaimTrieDb::seekByKey<>(std::map<type1, type2> & map) const

#define TEMPLATE_INSTANTIATE3(type1, type2, type3)                                        \
    template bool CClaimTrieDb::getQueueMap<>(std::map<type1, type2, type3> & map) const; \
    template bool CClaimTrieDb::getQueueRow<>(const type1& key, type2& row) const;        \
    template void CClaimTrieDb::updateQueueRow<>(const type1& key, type2& row);           \
    template bool CClaimTrieDb::keyTypeEmpty<type1, type2>() const;                       \
    template bool CClaimTrieDb::seekByKey<>(std::map<type1, type2, type3> & map) const

TEMPLATE_INSTANTIATE3(std::string, CClaimTrieNode, nodeNameCompare);
TEMPLATE_INSTANTIATE2(uint160, CClaimIndexElement);
TEMPLATE_INSTANTIATE2(int, claimQueueRowType);
TEMPLATE_INSTANTIATE2(std::string, queueNameRowType);
TEMPLATE_INSTANTIATE2(int, expirationQueueRowType);
TEMPLATE_INSTANTIATE2(std::string, supportMapEntryType);
TEMPLATE_INSTANTIATE2(int, supportQueueRowType);
TEMPLATE_INSTANTIATE2(std::string, supportQueueNameRowType);
TEMPLATE_INSTANTIATE2(int, supportExpirationQueueRowType);
