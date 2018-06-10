#ifndef BITCOIN_CLAIMTRIE_DB_H
#define BITCOIN_CLAIMTRIE_DB_H

#include "dbwrapper.h"

#include <utility>

#include <map>

/**
 * additional macros to complete hash algorithm
 */
#define  len(s)          (sizeof(s)/sizeof(*s)-1)
#define   H1(s, i, x)    (x*65599u+(uint8_t)s[(i)<len(s)?len(s)-1-(i):len(s)])
#define   H4(s, i, x)  H1(s, i,  H1(s, i+ 1,  H1(s, i+  2,  H1(s, i+  3, x))))
#define  H16(s, i, x)  H4(s, i,  H4(s, i+ 4,  H4(s, i+  8,  H4(s, i+ 12, x))))
#define  H64(s, i, x) H16(s, i, H16(s, i+16, H16(s, i+ 32, H16(s, i+ 48, x))))
#define H256(s, i, x) H64(s, i, H64(s, i+64, H64(s, i+128, H64(s, i+192, x))))

/**
 * Compile time hash calculation algorithm
 * @param[in] s    compile time string to compute its hash
 */
#define HASH(s)          ((uint32_t)((H256(s, 0, 0) ^ (H256(s, 0, 0) >> 16))))

/**
 * function to provide unique hash of 2 types
 */
template <class T1, class T2>
size_t hashType();

/**
 * explicit template specialization,
 * it should be provided where it's need a type hash
 */
#define HASH_VALUE(T1, T2) \
template<> inline          \
size_t hashType<T1, T2>()  \
{                          \
    return HASH(#T1 #T2);  \
}

class CClaimTrieDb;

/**
 * Interface between queue and database
 */
class CCBase
{
public:
    virtual ~CCBase() {}
    virtual void write(const size_t key, CClaimTrieDb *db) = 0;
};

/**
 * Represents access to claims in database
 */
class CClaimTrieDb : public CDBWrapper
{
public:
    CClaimTrieDb(bool fMemory = false, bool fWipe = false);

    ~CClaimTrieDb();

    /**
     * Writes all durty queues to disk
     */
    void writeQueues();

    /**
     * Gets a map representation of K type / V type stored by their hash
     * @param[out] map  key / value pairs readed from queues and disk
     */
    template <typename K, typename V, typename C>
    bool getQueueMap(std::map<K, V, C> &map) const;

    /**
     * Gets a value of type V found by key of type K stored by their hash
     * @param[in] key   key to looking for in dirty queues and disk
     * @param[out] row  value which is found
     */
    template <typename K, typename V>
    bool getQueueRow(const K &key, V &row) const;

    /**
     * Update value of type V by key of type K through their hash
     * @param[in] key       key to looking for in dirty queues and disk
     * @param[in/out] row   update value and gets its last value
     */
    template <typename K, typename V>
    void updateQueueRow(const K &key, V &row);

    /**
     * Check whatever exists values stored by hash
     */
    template <typename K, typename V>
    bool keyTypeEmpty() const;

    /**
     * Get a map representation of K type / V type stored by theirs hash
     * @param[out] map  key / value pairs readed only from disk
     */
    template <typename K, typename V, typename C>
    bool seekByKey(std::map<K, V, C> &map) const;

private:
    /**
     * Migrate old keys to new hash algorithm
     */
    void doMigrate();

    /**
     * Actual migation of key
     * @param[in] old_key   old key to replace it
     */
    template <typename K, typename V>
    void migrate(char old_key);

    /**
     * Represents dirty queues before stored to disk
     */
    std::map<size_t, CCBase*> queues;

    /**
     * Help purpose for unit testing
     */
    friend class claimtriedb_test_helper;
};

#endif // BITCOIN_CLAIMTRIE_DB_H
