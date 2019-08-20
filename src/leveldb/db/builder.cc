// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "db/builder.h"

#include "db/filename.h"
#include "db/dbformat.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "db/version_set.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"

namespace leveldb {

Status BuildTable(const std::string& dbname,
                  Env* env,
                  const Options& options,
                  const Comparator * user_comparator,
                  TableCache* table_cache,
                  Iterator* iter,
                  FileMetaData* meta,
                  SequenceNumber smallest_snapshot) {
  Status s;
  size_t keys_seen, keys_retired;

  keys_seen=0;
  keys_retired=0;

  meta->file_size = 0;
  iter->SeekToFirst();

  KeyRetirement retire(user_comparator, smallest_snapshot, &options);

  std::string fname = TableFileName(options, meta->number, meta->level);
  if (iter->Valid()) {
    WritableFile* file;

    s = env->NewWritableFile(fname, &file,
                                 env->RecoveryMmapSize(&options));
    if (!s.ok()) {
      return s;
    }

    // tune fadvise to keep all of this lower level file in page cache
    //  (compaction of unsorted files causes severe cache misses)
    file->SetMetadataOffset(1);

    TableBuilder* builder = new TableBuilder(options, file);
    meta->smallest.DecodeFrom(iter->key());
    for (; iter->Valid(); iter->Next()) {
      ++keys_seen;
      Slice key = iter->key();
      if (!retire(key))
      {
          meta->largest.DecodeFrom(key);
          builder->Add(key, iter->value());
          ++meta->num_entries;
      }   // if
      else
      {
          ++keys_retired;
      }   // else
    }

    // Finish and check for builder errors
    if (s.ok()) {
      s = builder->Finish();
      if (s.ok()) {
        meta->file_size = builder->FileSize();
        meta->exp_write_low = builder->GetExpiryWriteLow();
        meta->exp_write_high = builder->GetExpiryWriteHigh();
        meta->exp_explicit_high = builder->GetExpiryExplicitHigh();
        assert(meta->file_size > 0);
      }
    } else {
      builder->Abandon();
    }
    delete builder;

    // Finish and check for file errors
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = NULL;

    if (s.ok()) {
      // Verify that the table is usable
      Table * table_ptr;
      Iterator* it = table_cache->NewIterator(ReadOptions(),
                                              meta->number,
                                              meta->file_size,
                                              meta->level,
                                              &table_ptr);
      s = it->status();

      // Riak specific: bloom filter is no longer read by default,
      //  force read on highly used overlapped table files
      if (s.ok() && VersionSet::IsLevelOverlapped(meta->level))
          table_ptr->ReadFilter();

      // table_ptr is owned by it and therefore invalidated by this delete
      delete it;
    }
  }

  // Check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  if (s.ok() && meta->file_size > 0) {
    // Keep it
      if (0!=keys_retired)
      {
          Log(options.info_log, "Level-0 table #%" PRIu64 ": %zd keys seen, %zd keys retired, %zd keys expired",
              meta->number, keys_seen, retire.GetDroppedCount(), retire.GetExpiredCount());
      }   // if
  } else {
    env->DeleteFile(fname);
  }
  return s;
}

}  // namespace leveldb
