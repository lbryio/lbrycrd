// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/table_cache.h"

#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/version_edit.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/coding.h"
#include "leveldb/perf_count.h"

namespace leveldb {

static void DeleteEntry(const Slice& key, void* value) {
  TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
  if (0==dec_and_fetch(&tf->user_count))
  {
    if (NULL!=tf->doublecache)
      tf->doublecache->SubFileSize(tf->table->GetFileSize());
    delete tf->table;
    delete tf->file;
    delete tf;
  }   // if
}

static void UnrefEntry(void* arg1, void* arg2) {
  Cache* cache = reinterpret_cast<Cache*>(arg1);
  Cache::Handle* h = reinterpret_cast<Cache::Handle*>(arg2);
  cache->Release(h);
}

TableCache::TableCache(const std::string& dbname,
                       const Options* options,
                       Cache * file_cache,
                       DoubleCache & doublecache)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      cache_(file_cache),
      doublecache_(doublecache)
{
}

TableCache::~TableCache() {
}

Status TableCache::FindTable(uint64_t file_number, uint64_t file_size, int level,
                             Cache::Handle** handle, bool is_compaction,
                             bool for_iterator) {
  Status s;
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  Slice key(buf, sizeof(buf));
  *handle = cache_->Lookup(key);
  if (*handle == NULL) {
    std::string fname = TableFileName(*options_, file_number, level);
    RandomAccessFile* file = NULL;
    Table* table = NULL;
    s = env_->NewRandomAccessFile(fname, &file);
    if (s.ok()) {
      s = Table::Open(*options_, file, file_size, &table);

      // Riak:  support opportunity to manage Linux page cache
      if (is_compaction)
          file->SetForCompaction(file_size);
    }

    if (!s.ok()) {
      assert(table == NULL);
      delete file;
      // We do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else {
      TableAndFile* tf = new TableAndFile;
      tf->file = file;
      tf->table = table;
      tf->doublecache = &doublecache_;
      tf->file_number = file_number;
      tf->level = level;

      *handle = cache_->Insert(key, tf, table->TableObjectSize(), &DeleteEntry);
      gPerfCounters->Inc(ePerfTableOpened);
      doublecache_.AddFileSize(table->GetFileSize());

      // temporary hardcoding to match number of levels defined as
      //  overlapped in version_set.cc
      if (level<config::kNumOverlapLevels)
          cache_->Addref(*handle);
    }
  }
  else
  {
    Table *table = reinterpret_cast<TableAndFile*>(cache_->Value(*handle))->table;

    // this is NOT first access, see if bloom filter can load now
    if (!for_iterator && table->ReadFilter())
    {
      // TableAndFile now going to be present in two cache entries
      //  1. retrieve old entry within file cache
      TableAndFile* tf = reinterpret_cast<TableAndFile*>(cache_->Value(*handle));
      inc_and_fetch(&tf->user_count);

      //  2. must clean file size, do not want double count
      if (NULL!=tf->doublecache)
        tf->doublecache->SubFileSize(tf->table->GetFileSize());

      //  3. release current reference (and possible special overlap reference)
      cache_->Release(*handle);
      if (tf->level<config::kNumOverlapLevels)
        cache_->Release(*handle);

      //  4. create second table cache entry using TableObjectSize that now includes
      //     bloom filter size
      *handle = cache_->Insert(key, tf, table->TableObjectSize(), &DeleteEntry);

      //  5. set double reference if an overlapped file (prevents from being flushed)
      if (level<config::kNumOverlapLevels)
        cache_->Addref(*handle);
    }   // if

    // for Linux, let fadvise start precaching
    if (is_compaction)
    {
        RandomAccessFile *file = reinterpret_cast<TableAndFile*>(cache_->Value(*handle))->file;
        file->SetForCompaction(file_size);
    }   // if

    gPerfCounters->Inc(ePerfTableCached);
  }   // else
  return s;
}

Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number,
                                  uint64_t file_size,
                                  int level,
                                  Table** tableptr) {
  if (tableptr != NULL) {
    *tableptr = NULL;
  }

  Cache::Handle* handle = NULL;
  Status s = FindTable(file_number, file_size, level, &handle, options.IsCompaction(), true);

  if (!s.ok()) {
    return NewErrorIterator(s);
  }

  Table* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
  Iterator* result = table->NewIterator(options);
  result->RegisterCleanup(&UnrefEntry, cache_, handle);
  if (tableptr != NULL) {
    *tableptr = table;
  }
  return result;
}

Status TableCache::Get(const ReadOptions& options,
                       uint64_t file_number,
                       uint64_t file_size,
                       int level,
                       const Slice& k,
                       void* arg,
                       bool (*saver)(void*, const Slice&, const Slice&)) {
  Cache::Handle* handle = NULL;
  Status s = FindTable(file_number, file_size, level, &handle);

  if (s.ok()) {
    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    s = t->InternalGet(options, k, arg, saver);
    cache_->Release(handle);
  }
  return s;
}

void TableCache::Evict(uint64_t file_number, bool is_overlapped) {
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);

  // overlapped files have extra reference to prevent their purge,
  //  release that reference now
  if (is_overlapped)
  {
      Cache::Handle *handle;

      // the Lookup call adds a reference too, back out both
      handle=cache_->Lookup(Slice(buf, sizeof(buf)));

      // with multiple background threads, file might already be
      //  evicted
      if (NULL!=handle)
      {
          cache_->Release(handle);  // release for Lookup() call just made
          cache_->Release(handle);  // release for extra reference
      }   // if
  }   // if

  cache_->Erase(Slice(buf, sizeof(buf)));
}

/**
 * Riak specific routine to return table statistic ONLY if table metadata
 *  already within cache ... otherwise return 0.
 */
uint64_t
TableCache::GetStatisticValue(
    uint64_t file_number,
    unsigned Index)
{
    uint64_t ret_val;
    char buf[sizeof(file_number)];
    Cache::Handle *handle;

    ret_val=0;
    EncodeFixed64(buf, file_number);
    Slice key(buf, sizeof(buf));
    handle = cache_->Lookup(key);

    if (NULL != handle)
    {
        TableAndFile * tf;

        tf=reinterpret_cast<TableAndFile*>(cache_->Value(handle));
        ret_val=tf->table->GetSstCounters().Value(Index);
        cache_->Release(handle);
    }   // if

    return(ret_val);

}   // TableCache::GetStatisticValue

}  // namespace leveldb
