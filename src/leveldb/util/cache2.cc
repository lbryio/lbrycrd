// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

//
// mildly modified version of Google's original cache.cc to support
//  Riak's flexcache.cc
//


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "db/table_cache.h"
#include "db/version_edit.h"
#include "leveldb/atomics.h"
#include "leveldb/env.h"
#include "util/cache2.h"
#include "port/port.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {

//namespace {

// LRU cache implementation

// An entry is a variable length heap-allocated structure.  Entries
// are kept in a circular doubly linked list ordered by access time.
struct LRUHandle2 {
  void* value;
  void (*deleter)(const Slice&, void* value);
  LRUHandle2* next_hash;
  LRUHandle2* next;
  LRUHandle2* prev;
  size_t charge;      // TODO(opt): Only allow uint32_t?
  size_t key_length;
  uint32_t refs;
  uint32_t hash;      // Hash of key(); used for fast sharding and comparisons
  time_t expire_seconds; // zero (no expire) or time when this object expires
  char key_data[1];   // Beginning of key

  Slice key() const {
    // For cheaper lookups, we allow a temporary Handle object
    // to store a pointer to a key in "value".
    if (next == this) {
      return *(reinterpret_cast<Slice*>(value));
    } else {
      return Slice(key_data, key_length);
    }
  }
};

// We provide our own simple hash table since it removes a whole bunch
// of porting hacks and is also faster than some of the built-in hash
// table implementations in some of the compiler/runtime combinations
// we have tested.  E.g., readrandom speeds up by ~5% over the g++
// 4.4.3's builtin hashtable.
class HandleTable {
 public:
  HandleTable() : length_(0), elems_(0), list_(NULL) { Resize(); }
  ~HandleTable() { delete[] list_; }

  LRUHandle2* Lookup(const Slice& key, uint32_t hash) {
    return *FindPointer(key, hash);
  }

  LRUHandle2* Insert(LRUHandle2* h) {
    LRUHandle2** ptr = FindPointer(h->key(), h->hash);
    LRUHandle2* old = *ptr;
    h->next_hash = (old == NULL ? NULL : old->next_hash);
    *ptr = h;
    if (old == NULL) {
      ++elems_;
      if (elems_ > length_) {
        // Since each cache entry is fairly large, we aim for a small
        // average linked list length (<= 1).
        Resize();
      }
    }
    return old;
  }

  LRUHandle2* Remove(const Slice& key, uint32_t hash) {
    LRUHandle2** ptr = FindPointer(key, hash);
    LRUHandle2* result = *ptr;
    if (result != NULL) {
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
  // The table consists of an array of buckets where each bucket is
  // a linked list of cache entries that hash into the bucket.
  uint32_t length_;
  uint32_t elems_;
  LRUHandle2** list_;

  // Return a pointer to slot that points to a cache entry that
  // matches key/hash.  If there is no such cache entry, return a
  // pointer to the trailing slot in the corresponding linked list.
  LRUHandle2** FindPointer(const Slice& key, uint32_t hash) {
    LRUHandle2** ptr = &list_[hash & (length_ - 1)];
    while (*ptr != NULL &&
           ((*ptr)->hash != hash || key != (*ptr)->key())) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void Resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    LRUHandle2** new_list = new LRUHandle2*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      LRUHandle2* h = list_[i];
      while (h != NULL) {
        LRUHandle2* next = h->next_hash;
        /*Slice key =*/ h->key();  // eliminate unused var warning, but allow for side-effects
        uint32_t hash = h->hash;
        LRUHandle2** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};


// A single shard of sharded cache.
class LRUCache2 : public Cache {
 public:
  LRUCache2();
  ~LRUCache2();

  static inline uint32_t HashSlice(const Slice& s) {
    return Hash(s.data(), s.size(), 0);
  }
  // Separate from constructor so caller can easily make an array of LRUCache2

  // Cache2 methods to allow direct use for single shard
  virtual Cache::Handle* Insert(const Slice& key,
                        void* value, size_t charge,
                        void (*deleter)(const Slice& key, void* value))
        {return(Insert(key, HashSlice(key), value, charge, deleter));};

  virtual Cache::Handle* Lookup(const Slice& key)
        {return(Lookup(key, HashSlice(key)));};

  virtual void Release(Cache::Handle* handle);
  virtual bool ReleaseOne();
  virtual void Erase(const Slice& key)
       {Erase(key, HashSlice(key));};
  virtual void* Value(Handle* handle) {
    return reinterpret_cast<LRUHandle2*>(handle)->value;
  }

  virtual uint64_t NewId() {
      return inc_and_fetch(&last_id_);
  }

  virtual size_t EntryOverheadSize() {return(sizeof(LRUHandle2));};

  // Like Cache methods, but with an extra "hash" parameter.
  Cache::Handle* Insert(const Slice& key, uint32_t hash,
                        void* value, size_t charge,
                        void (*deleter)(const Slice& key, void* value));
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);

  void Erase(const Slice& key, uint32_t hash);

  virtual void Addref(Cache::Handle* handle);

  void SetParent(ShardedLRUCache2 * Parent, bool IsFileCache)
    {parent_=Parent; is_file_cache_=IsFileCache;};

  LRUHandle2 * LRUHead() {return(&lru_);}

  void LRUErase(LRUHandle2 * cursor)
  {
    LRU_Remove(cursor);
    table_.Remove(cursor->key(), cursor->hash);
    Unref(cursor);
  }

 private:
  void LRU_Remove(LRUHandle2* e);
  void LRU_Append(LRUHandle2* e);
  void Unref(LRUHandle2* e);

  // Initialized before use.
  class ShardedLRUCache2 * parent_;
  bool is_file_cache_;

  // mutex_ protects the following state.
  port::Spin spin_;
  uint64_t last_id_;

  // Dummy head of LRU list.
  // lru.prev is newest entry, lru.next is oldest entry.
  LRUHandle2 lru_;

  HandleTable table_;
};

LRUCache2::LRUCache2()
  : parent_(NULL), is_file_cache_(true), last_id_(0)
{
  // Make empty circular linked list
  lru_.next = &lru_;
  lru_.prev = &lru_;
  lru_.expire_seconds=0;
}

LRUCache2::~LRUCache2() {
  for (LRUHandle2* e = lru_.next; e != &lru_; ) {
    LRUHandle2* next = e->next;

    assert(e->refs == 1);  // Error if caller has an unreleased handle
    Unref(e);
    e = next;
  }
}

void LRUCache2::LRU_Remove(LRUHandle2* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

void LRUCache2::LRU_Append(LRUHandle2* e) {
  // Make "e" newest entry by inserting just before lru_
  e->next = &lru_;
  e->prev = lru_.prev;
  e->prev->next = e;
  e->next->prev = e;
}

//Cache::Handle* LRUCache2::Lookup(const Slice& key, uint32_t hash);

void LRUCache2::Release(Cache::Handle* handle) {
  SpinLock l(&spin_);
  Unref(reinterpret_cast<LRUHandle2*>(handle));
}

void LRUCache2::Addref(Cache::Handle* handle) {
  SpinLock l(&spin_);
  LRUHandle2 * e;

  e=reinterpret_cast<LRUHandle2*>(handle);
  if (NULL!=e && 1 <= e->refs)
      ++e->refs;
}


void LRUCache2::Erase(const Slice& key, uint32_t hash) {
  SpinLock l(&spin_);
  LRUHandle2* e = table_.Remove(key, hash);
  if (e != NULL) {
    LRU_Remove(e);
    Unref(e);
  }
}

//}  // end anonymous namespace


static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;

class ShardedLRUCache2 : public Cache {
public:
  volatile uint64_t usage_;        // cache2's usage is across all shards,
                                   //  simplifies FlexCache management

private:
  LRUCache2 shard_[kNumShards];
  port::Spin id_spin_;
  DoubleCache & parent_;
  bool is_file_cache_;
  size_t next_shard_;
  volatile uint64_t last_id_;

  static inline uint32_t HashSlice(const Slice& s) {
    return Hash(s.data(), s.size(), 0);
  }

  static uint32_t Shard(uint32_t hash) {
    return hash >> (32 - kNumShardBits);
  }

 public:
  explicit ShardedLRUCache2(class DoubleCache & Parent, bool IsFileCache)
      : usage_(0), parent_(Parent), is_file_cache_(IsFileCache), next_shard_(0), last_id_(0) {
    for (int s = 0; s < kNumShards; s++)
    {
        shard_[s].SetParent(this, IsFileCache);
    }

  }
  virtual ~ShardedLRUCache2() { }
  volatile uint64_t GetUsage() const {return(usage_);};
  volatile uint64_t * GetUsagePtr() {return(&usage_);};
  volatile uint64_t GetCapacity() {return(parent_.GetCapacity(is_file_cache_));}
  time_t GetFileTimeout() {return(parent_.GetFileTimeout());};

  virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                         void (*deleter)(const Slice& key, void* value)) {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }
  virtual Handle* Lookup(const Slice& key) {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Lookup(key, hash);
  }
  virtual void Addref(Handle* handle) {
    LRUHandle2* h = reinterpret_cast<LRUHandle2*>(handle);
    shard_[Shard(h->hash)].Addref(handle);
  }
  virtual void Release(Handle* handle) {
    LRUHandle2* h = reinterpret_cast<LRUHandle2*>(handle);
    shard_[Shard(h->hash)].Release(handle);
  }
  virtual void Erase(const Slice& key) {
    const uint32_t hash = HashSlice(key);
    shard_[Shard(hash)].Erase(key, hash);
  }
  virtual void* Value(Handle* handle) {
    return reinterpret_cast<LRUHandle2*>(handle)->value;
  }
  virtual uint64_t NewId() {
      return inc_and_fetch(&last_id_);
  }
  virtual size_t EntryOverheadSize() {return(sizeof(LRUHandle2));};

  // reduce usage of all shards to fit within current capacity limit
  void Resize()
  {
      size_t end_shard;
      bool one_deleted;

      SpinLock l(&id_spin_);
      end_shard=next_shard_;
      one_deleted=true;

      while((parent_.GetCapacity(is_file_cache_) < usage_) && one_deleted)
      {
          one_deleted=false;

          // round robin delete ... later, could delete from most full or such
          //   but keep simple since using spin lock
          do
          {
              one_deleted=shard_[next_shard_].ReleaseOne();
              next_shard_=(next_shard_ +1) % kNumShards;
          } while(end_shard!=next_shard_ && !one_deleted);

      }   // while

      return;

  } // ShardedLRUCache2::Resize


  // let doublecache know state of cache space
  void SetFreeSpaceWarning(size_t FileMetaSize)
  {
      bool plenty_space;

      plenty_space=(GetUsage() + 5*FileMetaSize < GetCapacity());

      parent_.SetPlentySpace(plenty_space);
  }   // SetFreeSpaceWarning


  // Only used on file cache.  Remove entries that are too old
  void PurgeExpiredFiles()
  {
      if (is_file_cache_)
      {
          int loop;
          time_t now;

          now=Env::Default()->NowMicros() / 1000000L;

          SpinLock l(&id_spin_);

          for (loop=0; loop<kNumShards; ++loop)
          {
              LRUHandle2 * next, * cursor;

              for (cursor=shard_[loop].LRUHead()->next;
                   cursor->expire_seconds <= now && cursor != shard_[loop].LRUHead();
                   cursor=next)
              {
                  // take next pointer before potentially destroying cursor
                  next=cursor->next;

                  // only delete cursor if it will actually destruct and
                  //   return value to usage_
                  if (cursor->refs <= 1 && 0!=cursor->expire_seconds)
                  {
                      shard_[loop].LRUErase(cursor);
                  }   // if
              }   // for
          }   // for
      }   // if

      return;

  } // ShardedLRUCache2::PurgeExpiredFiles

  // Walk all cache entries, calling functor Acc for each
  bool
  WalkCache(
      CacheAccumulator & Acc)
  {
      int loop;
      bool good(true);

      SpinLock l(&id_spin_);

      for (loop=0; loop<kNumShards && good; ++loop)
      {
          LRUHandle2 * cursor;

          for (cursor=shard_[loop].LRUHead()->next;
               cursor != shard_[loop].LRUHead() && good;
               cursor=cursor->next)
          {
              good=Acc(cursor->value);
          }   // for
      }   // for

      return(good);

  } // ShardedLRUCache2::WalkCache

};  //ShardedLRUCache2


/**
 * Initialize cache pair based upon current conditions
 */
DoubleCache::DoubleCache(
    const Options & options)
    : m_FileCache(NULL), m_BlockCache(NULL),
      m_IsInternalDB(options.is_internal_db), m_PlentySpace(true),
      m_Overhead(0), m_TotalAllocation(0),
      m_FileTimeout(10*24*60*60),  // default is 10 days
      m_BlockCacheThreshold(options.block_cache_threshold),
      m_SizeCachedFiles(0)
{
    // fixed allocation for recovery log and info LOG: 20M each
    //  (with 64 or open databases, this is a serious number)
    // and fixed allocation for two write buffers

    m_Overhead=options.write_buffer_size*2
        + options.env->RecoveryMmapSize(&options) + 4096;
    m_TotalAllocation=gFlexCache.GetDBCacheCapacity(m_IsInternalDB);

    if (m_Overhead < m_TotalAllocation)
        m_TotalAllocation -= m_Overhead;
    else
        m_TotalAllocation=0;

    // build two new caches
    Flush();

}   // DoubleCache::DoubleCache


DoubleCache::~DoubleCache()
{
    delete m_FileCache;
    delete m_BlockCache;

}   // DoubleCache::DoubleCache


/**
 * Resize each of the caches based upon new global conditions
 */
void
DoubleCache::ResizeCaches()
{
    m_TotalAllocation=gFlexCache.GetDBCacheCapacity(m_IsInternalDB);
    if (m_Overhead < m_TotalAllocation)
        m_TotalAllocation -= m_Overhead;
    else
        m_TotalAllocation=0;

    // worst case is size reduction, take from block cache first
    m_BlockCache->Resize();
    m_FileCache->Resize();

    return;

}   // DoubleCache::ResizeCaches()


/**
 * Calculate limit to file or block cache based upon global conditions
 */
size_t
DoubleCache::GetCapacity(
    bool IsFileCache,
    bool EstimatePageCache)
{
    size_t  ret_val;

    ret_val=0;

    if (2*1024*1024L < m_TotalAllocation)
    {
        // file capacity is "fixed", it is always the entire
        //  cache allocation less minimum block size
        if (IsFileCache)
        {
            ret_val=m_TotalAllocation - (2*1024*1024L);
        }   // if

        // block cache capacity is whatever file cache is not
        //  not using, or its minimum ... whichever is larger
        else
        {
            uint64_t temp;

            // usage could vary between two calls,
            //   get it once and use same twice
            temp=m_FileCache->GetUsage();

            if (temp<m_TotalAllocation)
            {
                // block cache gets whatever is left after
                //  file cache usage
                ret_val=m_TotalAllocation - temp;

                if (EstimatePageCache)
                {
                    // if block cache allocation exceeds threshold,
                    //  give up some to page cache
                    if (m_BlockCacheThreshold < ret_val)
                    {
                        uint32_t spare;

                        spare=ret_val-m_BlockCacheThreshold;

                        // use m_SizeCachedFiles as approximation of page cache
                        //  space needed for full files, i.e. prefer page cache to block cache
                        //  (must use temp since m_SizeCachedFiles is volatile)
                        temp = m_SizeCachedFiles;
                        if (temp < spare)
                            spare -= temp;
                        else
                            spare=0;

                        ret_val=m_BlockCacheThreshold + spare;
                    }   // if
                }   // if

                // always allow for 2Mbyte minimum
                //   (this minimum overrides m_BlockCacheThreshold)
                if (ret_val < (2*1024*1024L))
                    ret_val=(2*1024*1024L);
            }   // if
        }   // else
    }   // if

    return(ret_val);

}   // DoubleCache::GetCapacity


/**
 * Wipe out existing caches (if any), create two new ones
 *  WARNING:  this is really for UNIT TESTS.  DBImpl and TableCache
 *  save a copy of the pointers below and will not know of a change.
 *  The old pointer technology is holdover from original implementation.
 */
void
DoubleCache::Flush()
{
    delete m_FileCache;
    delete m_BlockCache;

    m_FileCache=new ShardedLRUCache2(*this, true);
    m_BlockCache=new ShardedLRUCache2(*this, false);

    return;

}   // DoubleCache::Flush


/**
 * Make room in block cache by killing off file cache
 *  entries that have been unused for a while
 */
void
DoubleCache::PurgeExpiredFiles()
{
    m_FileCache->PurgeExpiredFiles();

    return;

}   // DoubleCache::PurgExpiredFiles


//
// Definitions moved so they could access ShardedLRUCache members
//  (subtle hint to Google that every object should have .h file
//    because future reuse is unknowable ... and this ain't Java)
//
Cache::Handle* LRUCache2::Lookup(const Slice& key, uint32_t hash) {
  SpinLock l(&spin_);
  LRUHandle2* e = table_.Lookup(key, hash);
  if (e != NULL) {
    e->refs++;
    LRU_Remove(e);
    LRU_Append(e);

    // establish time limit on files in file cache (like 10 days)
    //  so they do not go stale and steal from block cache
    if (is_file_cache_)
    {
        e->expire_seconds=Env::Default()->NowMicros() / 1000000L
            + parent_->GetFileTimeout();
    }   // if
  }
  return reinterpret_cast<Cache::Handle*>(e);
}



//
// Definitions moved so they could access ShardedLRUCache members
//  (subtle hint to Google that every object should have .h file
//    because future reuse is unknowable)
//
void LRUCache2::Unref(LRUHandle2* e) {
  assert(e->refs > 0);
  e->refs--;
  if (e->refs <= 0) {
      sub_and_fetch(parent_->GetUsagePtr(), (uint64_t)e->charge);

      if (is_file_cache_)
          gPerfCounters->Add(ePerfFileCacheRemove, e->charge);
      else
          gPerfCounters->Add(ePerfBlockCacheRemove, e->charge);

      (*e->deleter)(e->key(), e->value);
      free(e);
  }
}


Cache::Handle* LRUCache2::Insert(
    const Slice& key, uint32_t hash, void* value, size_t charge,
    void (*deleter)(const Slice& key, void* value)) {

    size_t this_size;

    this_size=sizeof(LRUHandle2)-1 + key.size();
    LRUHandle2* e = reinterpret_cast<LRUHandle2*>(
        malloc(this_size));

    e->value = value;
    e->deleter = deleter;
    e->charge = charge + this_size;  // assumes charge is always byte size
    e->key_length = key.size();
    e->hash = hash;
    e->refs = 2;  // One from LRUCache2, one for the returned handle
    e->expire_seconds=0;
    memcpy(e->key_data, key.data(), key.size());

    // establish time limit on files in file cache (like 10 days)
    //  so they do not go stale and steal from block cache
    if (is_file_cache_)
    {
        e->expire_seconds=Env::Default()->NowMicros() / 1000000L
            + parent_->GetFileTimeout();
    }   // if

    if (is_file_cache_)
        gPerfCounters->Add(ePerfFileCacheInsert, e->charge);
    else
        gPerfCounters->Add(ePerfBlockCacheInsert, e->charge);


    {
        SpinLock l(&spin_);

        LRU_Append(e);
        add_and_fetch(parent_->GetUsagePtr(), (uint64_t)e->charge);

        LRUHandle2* old = table_.Insert(e);
        if (old != NULL) {
            LRU_Remove(old);
            Unref(old);
        }
    }   // SpinLock

    // call parent to rebalance across all shards, not just this one
    if (parent_->GetCapacity() <parent_->GetUsage())
        parent_->Resize();

    // let parent adjust free space warning level
    if (is_file_cache_)
        parent_->SetFreeSpaceWarning(e->charge);



  return reinterpret_cast<Cache::Handle*>(e);
}


bool
LRUCache2::ReleaseOne()
{
    bool ret_flag;
    LRUHandle2 * next, * cursor;
    SpinLock lock(&spin_);

    ret_flag=false;

    for (cursor=lru_.next; !ret_flag && parent_->GetUsage() > parent_->GetCapacity() && cursor != &lru_; cursor=next)
    {
        // take next pointer before potentially destroying cursor
        next=cursor->next;

        // only delete cursor if it will actually destruct and
        //   return value to usage_
        if (cursor->refs <= 1)
        {
            LRU_Remove(cursor);
            table_.Remove(cursor->key(), cursor->hash);
            Unref(cursor);
            ret_flag=true;
        }   // if
    }   // for

    return(ret_flag);

}   // LRUCache2::ReleaseOne

}  // namespace leveldb

