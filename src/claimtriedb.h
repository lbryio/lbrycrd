#ifndef BITCOIN_CLAIMTRIE_DB_H
#define BITCOIN_CLAIMTRIE_DB_H

#include "dbwrapper.h"

#include <utility>

#include <map>
#include <boost/type_index.hpp>

class CClaimTrieDb;

class CCBase
{
public:
    virtual ~CCBase() {}
    virtual void write(size_t key, CClaimTrieDb *db) = 0;
};

class CClaimTrieDb : public CDBWrapper
{
public:
    CClaimTrieDb(bool fMemory = false, bool fWipe = false);
    ~CClaimTrieDb();

    void writeQueues();

    template <typename K, typename V> bool getQueueMap(std::map<K,V> &map) const;

    template <typename K, typename V> bool getQueueRow(const K &key, V &row) const;
    template <typename K, typename V> void updateQueueRow(const K &key, V &row);

    template <typename K, typename V> bool keyTypeEmpty() const;
    template <typename K, typename V> bool seekByKey(std::map<K,V> &map) const;

private:
    std::map<size_t, CCBase*> queues;
};

#endif // BITCOIN_CLAIMTRIE_DB_H
