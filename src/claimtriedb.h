#ifndef BITCOIN_CLAIMTRIE_DB_H
#define BITCOIN_CLAIMTRIE_DB_H

#include "dbwrapper.h"

#include <utility>

#include <map>

#define  len(s)          (sizeof(s)/sizeof(*s)-1)
#define   H1(s, i, x)    (x*65599u+(uint8_t)s[(i)<len(s)?len(s)-1-(i):len(s)])
#define   H4(s, i, x)  H1(s, i,  H1(s, i+ 1,  H1(s, i+  2,  H1(s, i+  3, x))))
#define  H16(s, i, x)  H4(s, i,  H4(s, i+ 4,  H4(s, i+  8,  H4(s, i+ 12, x))))
#define  H64(s, i, x) H16(s, i, H16(s, i+16, H16(s, i+ 32, H16(s, i+ 48, x))))
#define H256(s, i, x) H64(s, i, H64(s, i+64, H64(s, i+128, H64(s, i+192, x))))

#define HASH(s)          ((uint32_t)((H256(s, 0, 0) ^ (H256(s, 0, 0) >> 16))))

template <class T1, class T2>
size_t hashType();

#define HASH_VALUE(T1, T2) \
template<> inline          \
size_t hashType<T1, T2>()  \
{                          \
    return HASH(#T1 #T2);  \
}

class CClaimTrieDb;

class CCBase
{
public:
    virtual ~CCBase() {}
    virtual void write(const size_t key, CClaimTrieDb *db) = 0;
};

class CClaimTrieDb : public CDBWrapper
{
public:
    CClaimTrieDb(bool fMemory = false, bool fWipe = false);

    ~CClaimTrieDb();

    void writeQueues();

    template <typename K, typename V, typename C>
    bool getQueueMap(std::map<K, V, C> &map) const;

    template <typename K, typename V>
    bool getQueueRow(const K &key, V &row) const;

    template <typename K, typename V>
    void updateQueueRow(const K &key, V &row);

    template <typename K, typename V>
    bool keyTypeEmpty() const;

    template <typename K, typename V, typename C>
    bool seekByKey(std::map<K, V, C> &map) const;

private:
    std::map<size_t, CCBase*> queues;
};

#endif // BITCOIN_CLAIMTRIE_DB_H
