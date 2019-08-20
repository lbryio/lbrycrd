// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_IMPL_H_
#define STORAGE_LEVELDB_DB_DB_IMPL_H_

#include <deque>
#include <set>
#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "util/cache2.h"

namespace leveldb {

class MemTable;
class TableCache;
class Version;
class VersionEdit;
class VersionSet;

class DBImpl : public DB {
 public:
  DBImpl(const Options& options, const std::string& dbname);
  virtual ~DBImpl();

  // Implementations of the DB interface
  virtual Status Put(const WriteOptions&, const Slice& key, const Slice& value, const KeyMetaData * meta=NULL);
  virtual Status Delete(const WriteOptions&, const Slice& key);
  virtual Status Write(const WriteOptions& options, WriteBatch* updates);
  virtual Status Get(const ReadOptions& options,
                     const Slice& key,
                     std::string* value,
                     KeyMetaData * meta=NULL);
  virtual Status Get(const ReadOptions& options,
                     const Slice& key,
                     Value* value,
                     KeyMetaData * meta=NULL);
  virtual Iterator* NewIterator(const ReadOptions&);
  virtual const Snapshot* GetSnapshot();
  virtual void ReleaseSnapshot(const Snapshot* snapshot);
  virtual bool GetProperty(const Slice& property, std::string* value);
  virtual void GetApproximateSizes(const Range* range, int n, uint64_t* sizes);
  virtual void CompactRange(const Slice* begin, const Slice* end);
  virtual Status VerifyLevels();
  virtual void CheckAvailableCompactions();
  virtual Logger* GetLogger() const { return options_.info_log; }

  // Extra methods (for testing) that are not in the public DB interface

  const Options & GetOptions() const { return options_; };

  // Compact any files in the named level that overlap [*begin,*end]
  void TEST_CompactRange(int level, const Slice* begin, const Slice* end);

  // Force current memtable contents to be compacted, waits for completion
  Status CompactMemTableSynchronous();
  Status TEST_CompactMemTable();       // wraps CompactMemTableSynchronous (historical)

  // Return an internal iterator over the current state of the database.
  // The keys of this iterator are internal keys (see format.h).
  // The returned iterator should be deleted when no longer needed.
  Iterator* TEST_NewInternalIterator();

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t TEST_MaxNextLevelOverlappingBytes();

  // These are routines that DBListImpl calls across all open databases
  void ResizeCaches() {double_cache.ResizeCaches();};
  size_t GetCacheCapacity() {return(double_cache.GetCapacity(false));}
  void PurgeExpiredFileCache() {double_cache.PurgeExpiredFiles();};

  // in util/hot_backup.cc
  void HotBackup();
  bool PurgeWriteBuffer();
  bool WriteBackupManifest();
  bool CreateBackupLinks(Version * Version, Options & BackupOptions);
  bool CopyLOGSegment(long FileEnd);
  void HotBackupComplete();

  void BackgroundCall2(Compaction * Compact);
  void BackgroundImmCompactCall();
  bool IsCompactionScheduled();
  uint32_t RunningCompactionCount() {mutex_.AssertHeld(); return(running_compactions_);};

 protected:
  friend class DB;
  struct CompactionState;
  struct Writer;

  Iterator* NewInternalIterator(const ReadOptions&,
                                SequenceNumber* latest_snapshot);

  Status NewDB();

  // Recover the descriptor from persistent storage.  May do a significant
  // amount of work to recover recently logged updates.  Any changes to
  // be made to the descriptor are added to *edit.
  Status Recover(VersionEdit* edit);

  // Riak routine:  pause DB::Open if too many compactions
  //  stacked up immediately.  Happens in some repairs and
  //  some Riak upgrades
  void CheckCompactionState();

  void MaybeIgnoreError(Status* s) const;

  // Delete any unneeded files and stale in-memory entries.
  void DeleteObsoleteFiles();
  void KeepOrDelete(const std::string & Filename, int level, const std::set<uint64_t> & Live);

  // Compact the in-memory write buffer to disk.  Switches to a new
  // log-file/memtable and writes a new descriptor iff successful.
  Status CompactMemTable();

  Status RecoverLogFile(uint64_t log_number,
                        VersionEdit* edit,
                        SequenceNumber* max_sequence);

  Status WriteLevel0Table(volatile MemTable* mem, VersionEdit* edit, Version* base);

  Status MakeRoomForWrite(bool force /* TRUE forces memtable rotation to disk (for testing) */);
  Status NewRecoveryLog(uint64_t NewLogNumber);

  WriteBatch* BuildBatchGroup(Writer** last_writer);

  void MaybeScheduleCompaction();

  Status BackgroundCompaction(Compaction * Compact=NULL);
  Status BackgroundExpiry(Compaction * Compact=NULL);

  void CleanupCompaction(CompactionState* compact);
  Status DoCompactionWork(CompactionState* compact);
  int64_t PrioritizeWork(bool IsLevel0);

  Status OpenCompactionOutputFile(CompactionState* compact, size_t sample_value_size);
  bool Send2PageCache(CompactionState * compact);
  size_t MaybeRaiseBlockSize(Compaction & CompactionStuff, size_t SampleValueSize);
  Status FinishCompactionOutputFile(CompactionState* compact, Iterator* input);
  Status InstallCompactionResults(CompactionState* compact);

  // initialized before options so its block_cache is available
  class DoubleCache double_cache;

  // Constant after construction
  Env* const env_;
  const InternalKeyComparator internal_comparator_;
  const InternalFilterPolicy internal_filter_policy_;
  const Options options_;  // options_.comparator == &internal_comparator_
  bool owns_info_log_;
  bool owns_cache_;
  const std::string dbname_;

  // table_cache_ provides its own synchronization
  TableCache* table_cache_;


  // Lock over the persistent DB state.  Non-NULL iff successfully acquired.
  FileLock* db_lock_;

  // State below is protected by mutex_
  port::Mutex mutex_;
  port::Mutex throttle_mutex_;   // used by write throttle to force sequential waits on callers
  port::AtomicPointer shutting_down_;

  port::CondVar bg_cv_;          // Signalled when background work finishes
  MemTable* mem_;
  volatile MemTable* imm_;                // Memtable being compacted
  port::AtomicPointer has_imm_;  // So bg thread can detect non-NULL imm_
  WritableFile* logfile_;
  uint64_t logfile_number_;
  log::Writer* log_;

  // Queue of writers.
  std::deque<Writer*> writers_;
  WriteBatch* tmp_batch_;

  SnapshotList snapshots_;

  // Set of table files to protect from deletion because they are
  // part of ongoing compactions.
  std::set<uint64_t> pending_outputs_;

  // Information for a manual compaction
  struct ManualCompaction {
    int level;
    bool done;
    const InternalKey* begin;   // NULL means beginning of key range
    const InternalKey* end;     // NULL means end of key range
    InternalKey tmp_storage;    // Used to keep track of compaction progress
  };
  volatile ManualCompaction* manual_compaction_;

  VersionSet* versions_;

  // Have we encountered a background error in paranoid mode?
  Status bg_error_;

  // Per level compaction stats.  stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct CompactionStats {
    int64_t micros;
    int64_t bytes_read;
    int64_t bytes_written;

    CompactionStats() : micros(0), bytes_read(0), bytes_written(0) { }

    void Add(const CompactionStats& c) {
      this->micros += c.micros;
      this->bytes_read += c.bytes_read;
      this->bytes_written += c.bytes_written;
    }
  };
  CompactionStats stats_[config::kNumLevels];

  volatile uint64_t throttle_end;
  volatile uint32_t running_compactions_;
  volatile size_t current_block_size_;    // last dynamic block size computed
  volatile uint64_t block_size_changed_;  // NowMicros() when block size computed
  volatile uint64_t last_low_mem_;        // NowMicros() when low memory last seen

  // accessor to new, dynamic block_cache
  Cache * block_cache() {return(double_cache.GetBlockCache());};
  Cache * file_cache() {return(double_cache.GetFileCache());};

  volatile bool hotbackup_pending_;

  // No copying allowed
  DBImpl(const DBImpl&);
  void operator=(const DBImpl&);

  const Comparator* user_comparator() const {
    return internal_comparator_.user_comparator();
  }
};

// Sanitize db options.  The caller should delete result.info_log if
// it is not equal to src.info_log.
extern Options SanitizeOptions(const std::string& db,
                               const InternalKeyComparator* icmp,
                               const InternalFilterPolicy* ipolicy,
                               const Options& src,
                               Cache * block_cache);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DB_IMPL_H_
