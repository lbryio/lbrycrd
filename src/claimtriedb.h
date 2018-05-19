#ifndef BITCOIN_CLAIMTRIE_DB_H
#define BITCOIN_CLAIMTRIE_DB_H

#include "dbwrapper.h"

#include <utility>

#include <map>


class CClaimTrieDb;

/**
 * Interface between buffer and database
 */
class CAbstractBuffer
{
public:
    virtual ~CAbstractBuffer()
    {
    }

    virtual void write(const unsigned char key, CClaimTrieDb* db) = 0;
};

/**
 * This class implements key value storage. It is used by the CClaimTrie
 * class to store queues of claim information. It allows for the storage
 * of values of datatype V that can be retrieved using key datatype K.
 * Changes to the key value storage is buffered until they are written to
 * disk using writeQueues()
 */
class CClaimTrieDb : public CDBWrapper
{
public:
    CClaimTrieDb(bool fMemory = false, bool fWipe = false);

    ~CClaimTrieDb();

    /**
     * Write to disk the buffered changes to the key value storage
     */
    void writeQueues();

    /**
     * Gets a map representation of K type / V type stored by their hash
     * on disk with buffered changes applied.
     * @param[out] map  key / value pairs to read
     */
    template <typename K, typename V, typename C>
    bool getQueueMap(std::map<K, V, C>& map) const;

    /**
     * Gets a value of type V found by key of type K stored by their hash
     * on disk with with buffered changes applied.
     * @param[in] key   key to look for
     * @param[out] row  value which is found
     */
    template <typename K, typename V>
    bool getQueueRow(const K& key, V& row) const;

    /**
     * Update value of type V by key of type K in the buffer through
     * their hash
     * @param[in] key       key to look for
     * @param[in/out] row   update value and gets its last value
     */
    template <typename K, typename V>
    void updateQueueRow(const K& key, V& row);

    /**
     * Check that there are no data stored under key datatype K and value
     * datatype V. Checks both the buffer and disk
     */
    template <typename K, typename V>
    bool keyTypeEmpty() const;

    /**
     * Get a map representation of K type / V type stored by their hash.
     * Look only in the disk, and not the buffer.
     * Returns false if database read fails.
     * @param[out] map  key / value pairs readed only from disk
     */
    template <typename K, typename V, typename C>
    bool seekByKey(std::map<K, V, C>& map) const;

private:
    /**
     * Represents buffer of changes before being stored to disk
     */
    std::map<unsigned char, CAbstractBuffer*> queues;
};

#endif // BITCOIN_CLAIMTRIE_DB_H
