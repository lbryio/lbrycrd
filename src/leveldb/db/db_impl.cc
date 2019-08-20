// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/db_impl.h"

#include <time.h>
#include <algorithm>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <set>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include "db/builder.h"
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/status.h"
#include "leveldb/table.h"
#include "leveldb/table_builder.h"
#include "port/port.h"
#include "table/block.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/db_list.h"
#include "util/coding.h"
#include "util/flexcache.h"
#include "util/hot_threads.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/thread_tasks.h"
#include "util/throttle.h"
#include "leveldb/perf_count.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace leveldb {

// Information kept for every waiting writer
struct DBImpl::Writer {
  Status status;
  WriteBatch* batch;
  bool sync;
  bool done;
  port::CondVar cv;

  explicit Writer(port::Mutex* mu) : cv(mu) { }
};

struct DBImpl::CompactionState {
  Compaction* const compaction;

  // Sequence numbers < smallest_snapshot are not significant since we
  // will never have to service a snapshot below smallest_snapshot.
  // Therefore if we have seen a sequence number S <= smallest_snapshot,
  // we can drop all entries for the same key with sequence numbers < S.
  SequenceNumber smallest_snapshot;

  // Files produced by compaction
  struct Output {
    uint64_t number;
    uint64_t file_size;
    InternalKey smallest, largest;
    uint64_t exp_write_low, exp_write_high, exp_explicit_high;

    Output() : number(0), file_size(0), exp_write_low(ULLONG_MAX), exp_write_high(0), exp_explicit_high(0) {}
  };
  std::vector<Output> outputs;

  // State kept for output being generated
  WritableFile* outfile;
  TableBuilder* builder;

  uint64_t total_bytes;
  uint64_t num_entries;

  Output* current_output() { return &outputs[outputs.size()-1]; }

  explicit CompactionState(Compaction* c)
      : compaction(c),
        outfile(NULL),
        builder(NULL),
        total_bytes(0),
        num_entries(0) {
  }
};

Value::~Value() {}

class StringValue : public Value {
 public:
  explicit StringValue(std::string& val) : value_(val) {}
  ~StringValue() {}

  StringValue& assign(const char* data, size_t size) {
    value_.assign(data, size);
    return *this;
  }

 private:
  std::string& value_;
};

// Fix user-supplied options to be reasonable
template <class T,class V>
static void ClipToRange(T* ptr, V minvalue, V maxvalue) {
  if (static_cast<V>(*ptr) > maxvalue) *ptr = maxvalue;
  if (static_cast<V>(*ptr) < minvalue) *ptr = minvalue;
}

Options SanitizeOptions(const std::string& dbname,
                        const InternalKeyComparator* icmp,
                        const InternalFilterPolicy* ipolicy,
                        const Options& src,
                        Cache * block_cache) {
  std::string tiered_dbname;
  Options result = src;
  result.comparator = icmp;
  result.filter_policy = (src.filter_policy != NULL) ? ipolicy : NULL;
  ClipToRange(&result.max_open_files,            20,     50000);
  ClipToRange(&result.write_buffer_size,         64<<10, 1<<30);
  ClipToRange(&result.block_size,                1<<10,  4<<20);

  // alternate means to change gMapSize ... more generic
  if (0!=src.mmap_size)
      gMapSize=src.mmap_size;

  // reduce buffer sizes if limited_developer_mem is true
  if (src.limited_developer_mem)
  {
      if (0==src.mmap_size)
          gMapSize=2*1024*1024L;
      if (gMapSize < result.write_buffer_size) // let unit tests be smaller
          result.write_buffer_size=gMapSize;
  }   // if

  // Validate tiered storage options
  tiered_dbname=MakeTieredDbname(dbname, result);

  if (result.info_log == NULL) {
    // Open a log file in the same directory as the db
    src.env->CreateDir(tiered_dbname);  // In case it does not exist
    src.env->RenameFile(InfoLogFileName(tiered_dbname), OldInfoLogFileName(tiered_dbname));
    Status s = src.env->NewLogger(InfoLogFileName(tiered_dbname), &result.info_log);
    if (!s.ok()) {
      // No place suitable for logging
      result.info_log = NULL;
    }
  }

  if (result.block_cache == NULL) {
      result.block_cache = block_cache;
  }

  // remove anything expiry if this is an internal database
  if (result.is_internal_db)
      result.expiry_module.reset();
  else if (NULL!=result.expiry_module.get())
      result.expiry_module.get()->NoteUserExpirySettings();

  return result;
}

DBImpl::DBImpl(const Options& options, const std::string& dbname)
    : double_cache(options),
      env_(options.env),
      internal_comparator_(options.comparator),
      internal_filter_policy_(options.filter_policy),
      options_(SanitizeOptions(
          dbname, &internal_comparator_, &internal_filter_policy_,
          options, block_cache())),
      owns_info_log_(options_.info_log != options.info_log),
      owns_cache_(options_.block_cache != options.block_cache),
      dbname_(options_.tiered_fast_prefix),
      db_lock_(NULL),
      shutting_down_(NULL),
      bg_cv_(&mutex_),
      mem_(new MemTable(internal_comparator_)),
      imm_(NULL),
      logfile_(NULL),
      logfile_number_(0),
      log_(NULL),
      tmp_batch_(new WriteBatch),
      manual_compaction_(NULL),
      throttle_end(0),
      running_compactions_(0),
      block_size_changed_(0), last_low_mem_(0),
      hotbackup_pending_(false)
{
  current_block_size_=options_.block_size;

  mem_->Ref();
  has_imm_.Release_Store(NULL);

  table_cache_ = new TableCache(dbname_, &options_, file_cache(), double_cache);

  versions_ = new VersionSet(dbname_, &options_, table_cache_,
                             &internal_comparator_);

  // switch global for everyone ... tacky implementation for now
  gFadviseWillNeed=options_.fadvise_willneed;

  // CAUTION: all object initialization must be completed
  //          before the AddDB and SetTotalMemory calls.
  DBList()->AddDB(this, options_.is_internal_db);
  gFlexCache.SetTotalMemory(options_.total_leveldb_mem);

  options_.Dump(options_.info_log);
  Log(options_.info_log,"               File cache size: %zd", double_cache.GetCapacity(true));
  Log(options_.info_log,"              Block cache size: %zd", double_cache.GetCapacity(false));
}

DBImpl::~DBImpl() {
  DBList()->ReleaseDB(this, options_.is_internal_db);

  // Wait for background work to finish
  mutex_.Lock();
  shutting_down_.Release_Store(this);  // Any non-NULL value is ok
  while (IsCompactionScheduled()) {
    bg_cv_.Wait();
  }
  mutex_.Unlock();

  // make sure flex cache knows this db is gone
  //  (must follow ReleaseDB() call ... see above)
  gFlexCache.RecalculateAllocations();

  delete versions_;
  if (mem_ != NULL) mem_->Unref();
  if (imm_ != NULL) imm_->Unref();
  delete tmp_batch_;
  delete log_;
  delete logfile_;

  if (options_.cache_object_warming)
      table_cache_->SaveOpenFileList();

  delete table_cache_;

  if (owns_info_log_) {
    delete options_.info_log;
  }
  if (db_lock_ != NULL) {
    env_->UnlockFile(db_lock_);
  }
}

Status DBImpl::NewDB() {
  VersionEdit new_db;
  new_db.SetComparatorName(user_comparator()->Name());
  new_db.SetLogNumber(0);
  new_db.SetNextFile(2);
  new_db.SetLastSequence(0);

  const std::string manifest = DescriptorFileName(dbname_, 1);
  WritableFile* file;
  Status s = env_->NewWritableFile(manifest, &file, 4*1024L);
  if (!s.ok()) {
    return s;
  }
  {
    log::Writer log(file);
    std::string record;
    new_db.EncodeTo(&record, options_.ExpiryActivated());
    s = log.AddRecord(record);
    if (s.ok()) {
      s = file->Close();
    }
  }
  delete file;
  if (s.ok()) {
    // Make "CURRENT" file that points to the new manifest file.
    s = SetCurrentFile(env_, dbname_, 1);
  } else {
    env_->DeleteFile(manifest);
  }

  return s;
}

void DBImpl::MaybeIgnoreError(Status* s) const {
  if (s->ok() || options_.paranoid_checks) {
    // No change needed
  } else {
    Log(options_.info_log, "Ignoring error %s", s->ToString().c_str());
    *s = Status::OK();
  }
}

void DBImpl::DeleteObsoleteFiles()
{
  // Only run this routine when down to one
  //  simultaneous compaction
  if (RunningCompactionCount()<2)
  {
      // each caller has mutex, we need to release it
      //  since this disk activity can take a while
      mutex_.AssertHeld();

      // Make a set of all of the live files
      std::set<uint64_t> live = pending_outputs_;
      versions_->AddLiveFiles(&live);

      // prune the database root directory
      std::vector<std::string> filenames;
      env_->GetChildren(dbname_, &filenames); // Ignoring errors on purpose
      for (size_t i = 0; i < filenames.size(); i++) {
          KeepOrDelete(filenames[i], -1, live);
      }   // for

      // prune the table file directories
      for (int level=0; level<config::kNumLevels; ++level)
      {
          std::string dirname;

          filenames.clear();
          dirname=MakeDirName2(options_, level, "sst");
          env_->GetChildren(dirname, &filenames); // Ignoring errors on purpose
          for (size_t i = 0; i < filenames.size(); i++) {
              KeepOrDelete(filenames[i], level, live);
          }   // for
      }   // for
  }   // if
}

void
DBImpl::KeepOrDelete(
    const std::string & Filename,
    int Level,
    const std::set<uint64_t> & Live)
{
  uint64_t number;
  FileType type;
  bool keep = true;

  if (ParseFileName(Filename, &number, &type))
  {
      switch (type)
      {
          case kLogFile:
              keep = ((number >= versions_->LogNumber()) ||
                      (number == versions_->PrevLogNumber()));
              break;

          case kDescriptorFile:
              // Keep my manifest file, and any newer incarnations'
              // (in case there is a race that allows other incarnations)
              keep = (number >= versions_->ManifestFileNumber());
              break;

          case kTableFile:
              keep = (Live.find(number) != Live.end());
              break;

          case kTempFile:
              // Any temp files that are currently being written to must
              // be recorded in pending_outputs_, which is inserted into "Live"
              keep = (Live.find(number) != Live.end());
          break;

          case kCurrentFile:
          case kDBLockFile:
          case kInfoLogFile:
          case kCacheWarming:
              keep = true;
              break;
      }   // switch

      if (!keep)
      {
          if (type == kTableFile) {
              // temporary hard coding of extra overlapped
              //  levels
              table_cache_->Evict(number, (Level<config::kNumOverlapLevels));
          }
          Log(options_.info_log, "Delete type=%d #%lld\n",
              int(type),
              static_cast<unsigned long long>(number));

          if (-1!=Level)
          {
              std::string file;

              file=TableFileName(options_, number, Level);
              env_->DeleteFile(file);
          }   // if
          else
          {
              env_->DeleteFile(dbname_ + "/" + Filename);
          }   // else
      }   // if
  }   // if
} // DBImpl::KeepOrDelete


Status DBImpl::Recover(VersionEdit* edit) {
  mutex_.AssertHeld();

  // Ignore error from CreateDir since the creation of the DB is
  // committed only when the descriptor is created, and this directory
  // may already exist from a previous failed creation attempt.
  env_->CreateDir(options_.tiered_fast_prefix);
  env_->CreateDir(options_.tiered_slow_prefix);
  assert(db_lock_ == NULL);
  Status s = env_->LockFile(LockFileName(dbname_), &db_lock_);
  if (!s.ok()) {
    return s;
  }

  if (!env_->FileExists(CurrentFileName(dbname_))) {
    if (options_.create_if_missing) {
      s = NewDB();
      if (!s.ok()) {
        return s;
      }
    } else {
      return Status::InvalidArgument(
          dbname_, "does not exist (create_if_missing is false)");
    }
  } else {
    if (options_.error_if_exists) {
      return Status::InvalidArgument(
          dbname_, "exists (error_if_exists is true)");
    }
  }

  // read manifest
  s = versions_->Recover();

  // Verify Riak 1.3 directory structure created and ready
  if (s.ok() && !TestForLevelDirectories(env_, options_, versions_->current()))
  {
      int level;
      std::string old_name, new_name;

      if (options_.create_if_missing)
      {
          // move files from old heirarchy to new
          s=MakeLevelDirectories(env_, options_);
          if (s.ok())
          {
              for (level=0; level<config::kNumLevels && s.ok(); ++level)
              {
                  const std::vector<FileMetaData*> & level_files(versions_->current()->GetFileList(level));
                  std::vector<FileMetaData*>::const_iterator it;

                  for (it=level_files.begin(); level_files.end()!=it && s.ok(); ++it)
                  {
                      new_name=TableFileName(options_, (*it)->number, level);

                      // test for partial completion
                      if (!env_->FileExists(new_name.c_str()))
                      {
                          old_name=TableFileName(options_, (*it)->number, -2);
                          s=env_->RenameFile(old_name, new_name);
                      }   // if
                  }   // for
              }   // for
          }   // if
          else
              return s;
      }   // if
      else
      {
          return Status::InvalidArgument(
              dbname_, "level directories do not exist (create_if_missing is false)");
      }   // else
  }   // if


  if (s.ok()) {
    SequenceNumber max_sequence(0);

    // Recover from all newer log files than the ones named in the
    // descriptor (new log files may have been added by the previous
    // incarnation without registering them in the descriptor).
    //
    // Note that PrevLogNumber() is no longer used, but we pay
    // attention to it in case we are recovering a database
    // produced by an older version of leveldb.
    const uint64_t min_log = versions_->LogNumber();
    const uint64_t prev_log = versions_->PrevLogNumber();
    std::vector<std::string> filenames;
    s = env_->GetChildren(dbname_, &filenames);
    if (!s.ok()) {
      return s;
    }
    uint64_t number;
    FileType type;
    std::vector<uint64_t> logs;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type)
          && type == kLogFile
          && ((number >= min_log) || (number == prev_log))) {
        logs.push_back(number);
      }
    }

    // Recover in the order in which the logs were generated
    std::sort(logs.begin(), logs.end());
    for (size_t i = 0; i < logs.size() && s.ok(); i++) {
      s = RecoverLogFile(logs[i], edit, &max_sequence);

      // The previous incarnation may not have written any MANIFEST
      // records after allocating this log number.  So we manually
      // update the file number allocation counter in VersionSet.
      versions_->MarkFileNumberUsed(logs[i]);
    }

    if (s.ok()) {
      if (versions_->LastSequence() < max_sequence) {
        versions_->SetLastSequence(max_sequence);
      }
    }
  }

  return s;
}


void DBImpl::CheckCompactionState()
{
    mutex_.AssertHeld();
    bool log_flag, need_compaction;

    // Verify Riak 1.4 level sizing, run compactions to fix as necessary
    //  (also recompacts hard repair of all files to level 0)

    log_flag=false;
    need_compaction=false;

    // loop on pending background compactions
    //  reminder: mutex_ is held
    do
    {
        int level;

        // wait out executing compaction (Wait gives mutex to compactions)
        if (IsCompactionScheduled())
            bg_cv_.Wait();

        for (level=0, need_compaction=false;
             level<config::kNumLevels && !need_compaction;
             ++level)
        {
            if (versions_->IsLevelOverlapped(level)
                && config::kL0_SlowdownWritesTrigger<=versions_->NumLevelFiles(level))
            {
                need_compaction=true;
                MaybeScheduleCompaction();
                if (!log_flag)
                {
                    log_flag=true;
                    Log(options_.info_log, "Cleanup compactions started ... DB::Open paused");
                }   // if
            }   //if
        }   // for

    } while(IsCompactionScheduled() && need_compaction);

    if (log_flag)
        Log(options_.info_log, "Cleanup compactions completed ... DB::Open continuing");

    // prior code only called this function instead of CheckCompactionState
    //  (duplicates original Google functionality)
    else
        MaybeScheduleCompaction();

    return;

}  // DBImpl::CheckCompactionState()


Status DBImpl::RecoverLogFile(uint64_t log_number,
                              VersionEdit* edit,
                              SequenceNumber* max_sequence) {
  struct LogReporter : public log::Reader::Reporter {
    Env* env;
    Logger* info_log;
    const char* fname;
    Status* status;  // NULL if options_.paranoid_checks==false
    virtual void Corruption(size_t bytes, const Status& s) {
      Log(info_log, "%s%s: dropping %d bytes; %s",
          (this->status == NULL ? "(ignoring error) " : ""),
          fname, static_cast<int>(bytes), s.ToString().c_str());
      if (this->status != NULL && this->status->ok()) *this->status = s;
    }
  };

  mutex_.AssertHeld();

  // Open the log file
  std::string fname = LogFileName(dbname_, log_number);
  SequentialFile* file;
  Status status = env_->NewSequentialFile(fname, &file);
  if (!status.ok()) {
    MaybeIgnoreError(&status);
    return status;
  }

  // Create the log reader.
  LogReporter reporter;
  reporter.env = env_;
  reporter.info_log = options_.info_log;
  reporter.fname = fname.c_str();
  reporter.status = (options_.paranoid_checks ? &status : NULL);
  // We intentially make log::Reader do checksumming even if
  // paranoid_checks==false so that corruptions cause entire commits
  // to be skipped instead of propagating bad information (like overly
  // large sequence numbers).
  log::Reader reader(file, &reporter, true/*checksum*/,
                     0/*initial_offset*/);
  Log(options_.info_log, "Recovering log #%llu",
      (unsigned long long) log_number);

  // Read all the records and add to a memtable
  std::string scratch;
  Slice record;
  WriteBatch batch;
  MemTable* mem = NULL;
  while (reader.ReadRecord(&record, &scratch) &&
         status.ok()) {
    if (record.size() < 12) {
      reporter.Corruption(
          record.size(), Status::Corruption("log record too small"));
      continue;
    }
    WriteBatchInternal::SetContents(&batch, record);

    if (mem == NULL) {
      mem = new MemTable(internal_comparator_);
      mem->Ref();
    }
    status = WriteBatchInternal::InsertInto(&batch, mem, &options_);
    MaybeIgnoreError(&status);
    if (!status.ok()) {
      break;
    }
    const SequenceNumber last_seq =
        WriteBatchInternal::Sequence(&batch) +
        WriteBatchInternal::Count(&batch) - 1;
    if (last_seq > *max_sequence) {
      *max_sequence = last_seq;
    }

    if (mem->ApproximateMemoryUsage() > options_.write_buffer_size) {
      status = WriteLevel0Table(mem, edit, NULL);
      if (!status.ok()) {
        // Reflect errors immediately so that conditions like full
        // file-systems cause the DB::Open() to fail.
        break;
      }
      mem->Unref();
      mem = NULL;
    }
  }

  if (status.ok() && mem != NULL) {
    status = WriteLevel0Table(mem, edit, NULL);
    // Reflect errors immediately so that conditions like full
    // file-systems cause the DB::Open() to fail.
  }

  if (mem != NULL) mem->Unref();
  delete file;
  return status;
}

Status DBImpl::WriteLevel0Table(volatile MemTable* mem, VersionEdit* edit,
                                Version* base) {
  mutex_.AssertHeld();
  const uint64_t start_micros = env_->NowMicros();
  FileMetaData meta;
  meta.number = versions_->NewFileNumber();
  meta.level = 0;
  pending_outputs_.insert(meta.number);
  Iterator* iter = ((MemTable *)mem)->NewIterator();
  SequenceNumber smallest_snapshot;

  if (snapshots_.empty()) {
    smallest_snapshot = versions_->LastSequence();
  } else {
    smallest_snapshot = snapshots_.oldest()->number_;
  }

  Status s;
  {
    Options local_options;

    mutex_.Unlock();
    Log(options_.info_log, "Level-0 table #%llu: started",
        (unsigned long long) meta.number);

    // want the data slammed to disk as fast as possible,
    //  no compression for level 0.
    local_options=options_;
    // matthewv Nov 2, 2016 local_options.compression=kNoCompression;
    local_options.block_size=current_block_size_;
    s = BuildTable(dbname_, env_, local_options, user_comparator(), table_cache_, iter, &meta, smallest_snapshot);

    Log(options_.info_log, "Level-0 table #%llu: %llu bytes, %llu keys %s",
        (unsigned long long) meta.number,
        (unsigned long long) meta.file_size,
        (unsigned long long) meta.num_entries,
      s.ToString().c_str());
    mutex_.Lock();
  }

  delete iter;
  pending_outputs_.erase(meta.number);


  // Note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  int level = 0;
  if (s.ok() && meta.file_size > 0) {
    const Slice min_user_key = meta.smallest.user_key();
    const Slice max_user_key = meta.largest.user_key();
    if (base != NULL) {
        int level_limit;
        if (0!=options_.tiered_slow_level && (options_.tiered_slow_level-1)<static_cast<unsigned>(config::kMaxMemCompactLevel))
            level_limit=options_.tiered_slow_level-1;
        else
            level_limit=config::kMaxMemCompactLevel;

        // remember, mutex is held so safe to push file into a non-compacting level
        level = base->PickLevelForMemTableOutput(min_user_key, max_user_key, level_limit);
        if (versions_->IsCompactionSubmitted(level) || !versions_->NeighborCompactionsQuiet(level))
            level=0;

        if (0!=level)
        {
            Status move_s;
            std::string old_name, new_name;

            old_name=TableFileName(options_, meta.number, 0);
            new_name=TableFileName(options_, meta.number, level);
            move_s=env_->RenameFile(old_name, new_name);

            if (move_s.ok())
            {
                // builder already added file to table_cache with 2 references and
                //  marked as level 0 (used by cache warming) ... going to remove from cache
                //  and add again correctly
                table_cache_->Evict(meta.number, true);
                meta.level=level;

                // sadly, we must hold the mutex during this file open
                //  since operating in non-overlapped level
                Iterator* it=table_cache_->NewIterator(ReadOptions(),
                                                       meta.number,
                                                       meta.file_size,
                                                       meta.level);
                delete it;

                // argh!  logging while holding mutex ... cannot release
                Log(options_.info_log, "Level-0 table #%llu:  moved to level %d",
                    (unsigned long long) meta.number,
                    level);
            }   // if
            else
            {
                level=0;
            }   // else
        }   // if
    }

    if (s.ok())
        edit->AddFile2(level, meta.number, meta.file_size,
                       meta.smallest, meta.largest,
                       meta.exp_write_low, meta.exp_write_high, meta.exp_explicit_high);
  }

  CompactionStats stats;
  stats.micros = env_->NowMicros() - start_micros;
  stats.bytes_written = meta.file_size;
  stats_[level].Add(stats);

  // Riak adds extra reference to file, must remove it
  //  in this race condition upon close
  if (s.ok() && shutting_down_.Acquire_Load()) {
      table_cache_->Evict(meta.number, versions_->IsLevelOverlapped(level));
  }

  return s;
}

Status DBImpl::CompactMemTable() {
  mutex_.AssertHeld();
  assert(imm_ != NULL);

  // Save the contents of the memtable as a new Table
  VersionEdit edit;
  Version* base = versions_->current();
  base->Ref();
  Status s = WriteLevel0Table(imm_, &edit, base);
  base->Unref();

  if (s.ok() && shutting_down_.Acquire_Load()) {
    s = Status::IOError("Deleting DB during memtable compaction");
  }

  // Replace immutable memtable with the generated Table
  if (s.ok()) {
    edit.SetPrevLogNumber(0);
    edit.SetLogNumber(logfile_number_);  // Earlier logs no longer needed
    s = versions_->LogAndApply(&edit, &mutex_);
  }

  if (s.ok()) {
    // Commit to the new state
    imm_->Unref();
    imm_ = NULL;
    has_imm_.Release_Store(NULL);
    DeleteObsoleteFiles();
  }

  return s;
}

void DBImpl::CompactRange(const Slice* begin, const Slice* end) {
  int max_level_with_files = 1;
  {
    MutexLock l(&mutex_);
    Version* base = versions_->current();
    for (int level = 1; level < config::kNumLevels; level++) {
      if (base->OverlapInLevel(level, begin, end)) {
        max_level_with_files = level;
      }
    }
  }
  CompactMemTableSynchronous(); // TODO(sanjay): Skip if memtable does not overlap
  for (int level = 0; level < max_level_with_files; level++) {
    TEST_CompactRange(level, begin, end);
  }
}

void DBImpl::TEST_CompactRange(int level, const Slice* begin,const Slice* end) {
  assert(level >= 0);
  assert(level + 1 < config::kNumLevels);

  InternalKey begin_storage, end_storage;

  ManualCompaction manual;
  manual.level = level;
  manual.done = false;
  if (begin == NULL) {
    manual.begin = NULL;
  } else {
    begin_storage = InternalKey(*begin, 0, kMaxSequenceNumber, kValueTypeForSeek);
    manual.begin = &begin_storage;
  }
  if (end == NULL) {
    manual.end = NULL;
  } else {
    end_storage = InternalKey(*end, 0, 0, static_cast<ValueType>(0));
    manual.end = &end_storage;
  }

  MutexLock l(&mutex_);
  while (!manual.done) {
    while (manual_compaction_ != NULL || IsCompactionScheduled()) {
      bg_cv_.Wait();
    }
    manual_compaction_ = &manual;
    MaybeScheduleCompaction();
    while (manual_compaction_ == &manual) {
      bg_cv_.Wait();
    }
  }
}

/**
 * This "test" routine was used in one production location,
 *  then two with addition of hot backup.  Inappropriate for
 *  TEST_ prefix if used in production.
 */
Status DBImpl::TEST_CompactMemTable() {
    return(CompactMemTableSynchronous());
}   // TEST_CompactMemTable


Status DBImpl::CompactMemTableSynchronous() {
  // NULL batch means just wait for earlier writes to be done
  Status s = Write(WriteOptions(), NULL);
  if (s.ok()) {
    // Wait until the compaction completes
    MutexLock l(&mutex_);
    while (imm_ != NULL && bg_error_.ok()) {
      bg_cv_.Wait();
    }
    if (imm_ != NULL) {
      s = bg_error_;
    }
  }
  return s;
}

void DBImpl::MaybeScheduleCompaction() {
  mutex_.AssertHeld();

  if (!shutting_down_.Acquire_Load())
  {
      if (NULL==manual_compaction_)
      {
          // ask versions_ to schedule work to hot threads
          versions_->PickCompaction(this);
      }   // if

      else if (!versions_->IsCompactionSubmitted(manual_compaction_->level))
      {
          // support manual compaction under hot threads
          versions_->SetCompactionSubmitted(manual_compaction_->level);
          ThreadTask * task=new CompactionTask(this, NULL);
          gCompactionThreads->Submit(task, true);
      }   // else if
  }   // if
}


void DBImpl::BackgroundCall2(
    Compaction * Compact) {
  MutexLock l(&mutex_);
  int level, type;
  assert(IsCompactionScheduled());

  type=kNormalCompaction;
  ++running_compactions_;
  if (NULL!=Compact)
  {
      level=Compact->level();
      type=Compact->GetCompactionType();
  }   // if
  else if (NULL!=manual_compaction_)
      level=manual_compaction_->level;
  else
      level=0;

  if (0==level)
      gPerfCounters->Inc(ePerfBGCompactLevel0);
  else
      gPerfCounters->Inc(ePerfBGNormal);

  versions_->SetCompactionRunning(level);

  if (!shutting_down_.Acquire_Load()) {
    Status s;

    switch(type)
    {
        case kNormalCompaction:
            s = BackgroundCompaction(Compact);
            break;

        case kExpiryFileCompaction:
            s = BackgroundExpiry(Compact);
            break;

        default:
            assert(0);
            break;
    }   // switch

    if (!s.ok() && !shutting_down_.Acquire_Load()) {
      // Wait a little bit before retrying background compaction in
      // case this is an environmental problem and we do not want to
      // chew up resources for failed compactions for the duration of
      // the problem.
      bg_cv_.SignalAll();  // In case a waiter can proceed despite the error
      mutex_.Unlock();
      Log(options_.info_log, "Waiting after background compaction error: %s",
          s.ToString().c_str());
      env_->SleepForMicroseconds(1000000);
      mutex_.Lock();
    }
  }
  else
  {
    delete Compact;
  }   // else

  --running_compactions_;
  versions_->SetCompactionDone(level, env_->NowMicros());

  // Previous compaction may have produced too many files in a level,
  // so reschedule another compaction if needed.
  if (!options_.is_repair)
      MaybeScheduleCompaction();
  bg_cv_.SignalAll();

}


void
DBImpl::BackgroundImmCompactCall() {
  MutexLock l(&mutex_);
  assert(NULL != imm_);
  Status s;

  ++running_compactions_;
  gPerfCounters->Inc(ePerfBGCompactImm);

  if (!shutting_down_.Acquire_Load()) {
    s = CompactMemTable();
    if (!s.ok() && !shutting_down_.Acquire_Load()) {
      // Wait a little bit before retrying background compaction in
      // case this is an environmental problem and we do not want to
      // chew up resources for failed compactions for the duration of
      // the problem.
      bg_cv_.SignalAll();  // In case a waiter can proceed despite the error
      mutex_.Unlock();
      Log(options_.info_log, "Waiting after background imm compaction error: %s",
          s.ToString().c_str());
      env_->SleepForMicroseconds(1000000);
      mutex_.Lock();
    }
  }

  --running_compactions_;

  // Previous compaction may have produced too many files in a level,
  // so reschedule another compaction if needed.
  if (!options_.is_repair)
      MaybeScheduleCompaction();

  // shutdown is waiting for this imm_ to clear
  if (shutting_down_.Acquire_Load()) {

    // must abandon data in memory ... hope recovery log works
    if (NULL!=imm_)
      imm_->Unref();
    imm_ = NULL;
    has_imm_.Release_Store(NULL);
  } // if

  // retry imm compaction if failed and not shutting down
  else if (!s.ok())
  {
      ThreadTask * task=new ImmWriteTask(this);
      gImmThreads->Submit(task, true);
  }   // else

  bg_cv_.SignalAll();
}


Status DBImpl::BackgroundCompaction(
    Compaction * Compact) {
  Status status;
  bool do_compact(true);

  mutex_.AssertHeld();

  Compaction* c(Compact);
  bool is_manual = (manual_compaction_ != NULL);
  InternalKey manual_end;
  if (NULL!=c) {
      // do nothing in this work block
  } else  if (is_manual) {
    ManualCompaction* m = (ManualCompaction *) manual_compaction_;
    c = versions_->CompactRange(m->level, m->begin, m->end);
    m->done = (c == NULL);
    if (c != NULL) {
      manual_end = c->input(0, c->num_input_files(0) - 1)->largest;
    }
    Log(options_.info_log,
        "Manual compaction at level-%d from %s .. %s; will stop at %s\n",
        m->level,
        (m->begin ? m->begin->DebugString().c_str() : "(begin)"),
        (m->end ? m->end->DebugString().c_str() : "(end)"),
        (m->done ? "(end)" : manual_end.DebugString().c_str()));
  } else {
      // c = versions_->PickCompaction();
  }


  if (c == NULL) {
    // Nothing to do
    do_compact=false;
  } else if (!is_manual && c->IsTrivialMove()
             && (c->level()+1)!=(int)options_.tiered_slow_level) {
    // Move file to next level
    assert(c->num_input_files(0) == 1);
    std::string old_name, new_name;
    FileMetaData* f = c->input(0, 0);

    old_name=TableFileName(options_, f->number, c->level());
    new_name=TableFileName(options_, f->number, c->level() +1);
    status=env_->RenameFile(old_name, new_name);

    if (status.ok())
    {
        gPerfCounters->Inc(ePerfBGMove);
        do_compact=false;
        c->edit()->DeleteFile(c->level(), f->number);
        c->edit()->AddFile2(c->level() + 1, f->number, f->file_size,
                            f->smallest, f->largest,
                            f->exp_write_low, f->exp_write_high, f->exp_explicit_high);
        status = versions_->LogAndApply(c->edit(), &mutex_);
        DeleteObsoleteFiles();

        // if LogAndApply fails, should file be renamed back to original spot?
        VersionSet::LevelSummaryStorage tmp;
        Log(options_.info_log, "Moved #%lld to level-%d %lld bytes %s: %s\n",
            static_cast<unsigned long long>(f->number),
            c->level() + 1,
            static_cast<unsigned long long>(f->file_size),
            status.ToString().c_str(),
            versions_->LevelSummary(&tmp));

        // no time, no keys ... just make the call so that one compaction
        //  gets posted against potential backlog ... extremely important
        //  to write throttle logic.
        SetThrottleWriteRate(0, 0, (0 == c->level()));
    }  // if
    else {
        // retry as compaction instead of move
        do_compact=true; // redundant but safe
        gPerfCounters->Inc(ePerfBGMoveFail);
    }   // else
  }
  if (do_compact) {
    CompactionState* compact = new CompactionState(c);
    status = DoCompactionWork(compact);
    CleanupCompaction(compact);
    c->ReleaseInputs();
    DeleteObsoleteFiles();
  }
  delete c;

  if (status.ok()) {
    // Done
  } else if (shutting_down_.Acquire_Load()) {
    // Ignore compaction errors found during shutting down
  } else {
    Log(options_.info_log,
        "Compaction error: %s", status.ToString().c_str());
    if (options_.paranoid_checks && bg_error_.ok()) {
      bg_error_ = status;
    }
  }

  if (is_manual) {
    ManualCompaction* m = (ManualCompaction *)manual_compaction_;
    if (!status.ok()) {
      m->done = true;
    }
    if (!m->done) {
      // We only compacted part of the requested range.  Update *m
      // to the range that is left to be compacted.
      m->tmp_storage = manual_end;
      m->begin = &m->tmp_storage;
    }
    manual_compaction_ = NULL;
  }

  return status;
}

void DBImpl::CleanupCompaction(CompactionState* compact) {
  mutex_.AssertHeld();
  if (compact->builder != NULL) {
    // May happen if we get a shutdown call in the middle of compaction
    compact->builder->Abandon();
    delete compact->builder;
  } else {
    assert(compact->outfile == NULL);
  }
  delete compact->outfile;
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const CompactionState::Output& out = compact->outputs[i];
    pending_outputs_.erase(out.number);
  }
  delete compact;
}

Status DBImpl::OpenCompactionOutputFile(
    CompactionState* compact,
    size_t sample_value_size) {
  assert(compact != NULL);
  assert(compact->builder == NULL);
  uint64_t file_number;
  bool pagecache_flag;

  {
    mutex_.Lock();
    file_number = versions_->NewFileNumber();
    pending_outputs_.insert(file_number);
    CompactionState::Output out;
    out.number = file_number;
    out.smallest.Clear();
    out.largest.Clear();
    compact->outputs.push_back(out);
    pagecache_flag=Send2PageCache(compact);
    mutex_.Unlock();
  }

  // Make the output file
  std::string fname = TableFileName(options_, file_number, compact->compaction->level()+1);
  Status s = env_->NewWritableFile(fname, &compact->outfile, gMapSize);
  if (s.ok()) {
      Options options;
      options=options_;
      options.block_size=current_block_size_;

      // consider larger block size if option enabled (block_size_steps!=0)
      //  and low on file cache space
      if (0!=options.block_size_steps)
      {
          uint64_t now;

          now=env_->NowMicros();

          if (!double_cache.GetPlentySpace())
          {
              // keep track of last time there was lack of space.
              //  use info in block below to revert block_size
              last_low_mem_=now;

              // do not make changes often, a multi file compaction
              //  could raise more than one step (5 min)
              if (block_size_changed_+(5*60*1000000L) < now)
              {
                  size_t old_size=current_block_size_;

                  options.block_size=MaybeRaiseBlockSize(*compact->compaction, sample_value_size);

                  // did size change?
                  if (options.block_size!=old_size)
                  {
                      block_size_changed_=now;
                  }   // if
              }   // if

          }   // if

          // has system's memory been ok for a while now
          else if (last_low_mem_+double_cache.GetFileTimeout()*1000000L < now)
          {
              // reset size to original, data could have been deleted and/or old
              //  files no longer need cache space
              current_block_size_=options_.block_size;
          }   // else if

      }   // if

      // force call to CalcInputState to set IsCompressible
      compact->compaction->CalcInputStats(*table_cache_);

      // do not attempt compression if data known to not compress
      if (kSnappyCompression==options.compression && !compact->compaction->IsCompressible())
      {
          options.compression=kNoCompressionAutomated;
          Log(options.info_log, "kNoCompressionAutomated");
      }   // if


      // tune fadvise to keep as much of the file data in RAM as
      //  reasonably possible
      if (pagecache_flag)
          compact->outfile->SetMetadataOffset(1);
      compact->builder = new TableBuilder(options, compact->outfile);
  }   // if

  return s;
}


bool
DBImpl::Send2PageCache(
    CompactionState* compact)
{
    bool ret_flag;

    mutex_.AssertHeld();

    // tune fadvise to keep all of the lower level file in page cache
    //  (compaction of unsorted files causes severe cache misses)
    if (versions_->IsLevelOverlapped(compact->compaction->level()))
//    if (0==compact->compaction->level())
    {
        ret_flag=true;
    }   // if

    // look at current RAM availability to decide whether or not to keep
    //  file data in page cache
    else
    {
        size_t avail_block;
        int64_t lower_levels;
        int level;

        // current block cache size without PageCache estimation
        avail_block=double_cache.GetCapacity(false, false);

        lower_levels=0;
        for (level=0; level<=compact->compaction->level(); ++level)
            lower_levels+=versions_->NumLevelBytes(level);

        // does the block cache's unadjusted size exceed higher
        //  volatility file sizes in lower levels?
        ret_flag=(lower_levels<=(int64_t)avail_block);
    }   // else

    return(ret_flag);

}   // DbImpl::Send2PageCache

size_t
DBImpl::MaybeRaiseBlockSize(
    Compaction & CompactionStuff,
    size_t SampleValueSize)
{
    size_t new_block_size, tot_user_data, tot_index_keys, avg_value_size,
        avg_key_size, avg_block_size;

    // start with most recent dynamic sizing
    new_block_size=current_block_size_;

    //
    // 1. Get estimates for key values.  Zero implies unable to estimate
    //    (as the formula is tuned, some of the values become unused ... apologies
    CompactionStuff.CalcInputStats(*table_cache_);
    tot_user_data=CompactionStuff.TotalUserDataSize();
    tot_index_keys=CompactionStuff.TotalIndexKeys();
    avg_value_size=CompactionStuff.AverageValueSize();
    avg_key_size=CompactionStuff.AverageKeySize();
    avg_block_size=CompactionStuff.AverageBlockSize();

    // CalcInputStats does not have second source for avg_value_size.
    //  Use size of next key.
    if (0==avg_value_size)
        avg_value_size=SampleValueSize;

    Log(options_.info_log,
        "Block stats used %zd user data, %zd index keys, %zd avg value, %zd avg key, %zd avg block",
        tot_user_data, tot_index_keys, avg_value_size, avg_key_size, avg_block_size);

    //
    // 2. Define boundaries of block size steps.  Calculate
    //    "next step"
    //
    if (0!=tot_user_data && 0!=tot_index_keys && 0!=avg_value_size
        && 0!=avg_key_size && 0!=avg_block_size)
    {
        size_t high_size, low_size, cur_size, increment, file_data_size, keys_per_file;

        // 2a. Highest block size:
        //      (sqrt()/sqrt() stuff is from first derivative to minimize
        //       total read size of one block plus file metadata)

        // limited by keys or filesize? (pretend metadata is zero, i love pretend games)
        file_data_size=versions_->MaxFileSizeForLevel(CompactionStuff.level());
        keys_per_file=file_data_size / avg_value_size;

        if (300000 < keys_per_file)
        {
            keys_per_file = 300000;
            file_data_size = avg_value_size * keys_per_file;
        }   // if

        // cast to double inside sqrt() is required for Solaris 13
        high_size=(size_t)((double)file_data_size / (sqrt((double)file_data_size)/sqrt((double)avg_key_size)));

        // 2b. Lowest block size: largest of given block size or average value size
        //      because large values are one block
        if (avg_value_size < options_.block_size)
            low_size=options_.block_size;
        else
            low_size=avg_value_size;

        // 2c. Current block size: compaction can skew numbers in files
        //     without counters, use current dynamic block_size in that case
        if (options_.block_size < avg_block_size)
            cur_size=avg_block_size;
        else
            cur_size=current_block_size_;

        // safety check values to eliminate negatives
        if (low_size <= high_size)
        {
            size_t cur_step;

            increment=(high_size - low_size)/options_.block_size_steps;

            // adjust old, too low stuff
            if (low_size < cur_size)
                cur_step=(cur_size - low_size)/increment;
            else
                cur_step=0;

            // move to next step, but not over the top step
            if (cur_step < (size_t)options_.block_size_steps)
                ++cur_step;
            else
                cur_step=options_.block_size_steps;

            //
            // 3. Set new block size to next higher step
            //
            new_block_size=low_size + increment * cur_step;

            Log(options_.info_log,
                "Block size selected %zd block size, %zd cur, %zd low, %zd high, %zd inc, %zd step",
                new_block_size, cur_size, low_size, high_size, increment, cur_step);

            // This is not thread safe, but not worthy of mutex either
            if (current_block_size_ < new_block_size)
                current_block_size_ = new_block_size;
        }   // if
    }   // if

    return(new_block_size);

}   // DBImpl::MaybeRaiseBlockSize


Status DBImpl::FinishCompactionOutputFile(CompactionState* compact,
                                          Iterator* input) {
  assert(compact != NULL);
  assert(compact->outfile != NULL);
  assert(compact->builder != NULL);

  const uint64_t output_number = compact->current_output()->number;
  assert(output_number != 0);

  // Check for iterator errors
  Status s = input->status();
  const uint64_t current_entries = compact->builder->NumEntries();
  if (s.ok()) {
    s = compact->builder->Finish();
  } else {
    compact->builder->Abandon();
  }
  const uint64_t current_bytes = compact->builder->FileSize();
  compact->current_output()->file_size = current_bytes;
  compact->total_bytes += current_bytes;
  compact->num_entries += compact->builder->NumEntries();
  compact->current_output()->exp_write_low = compact->builder->GetExpiryWriteLow();
  compact->current_output()->exp_write_high = compact->builder->GetExpiryWriteHigh();
  compact->current_output()->exp_explicit_high = compact->builder->GetExpiryExplicitHigh();
  delete compact->builder;
  compact->builder = NULL;

  // Finish and check for file errors
  if (s.ok()) {
    s = compact->outfile->Sync();
  }
  if (s.ok()) {
    s = compact->outfile->Close();
  }
  delete compact->outfile;
  compact->outfile = NULL;

  if (s.ok() && current_entries > 0) {
    // Verify that the table is usable
    Table * table_ptr;
    Iterator* iter = table_cache_->NewIterator(ReadOptions(),
                                               output_number,
                                               current_bytes,
                                               compact->compaction->level()+1,
                                               &table_ptr);
    s = iter->status();
    // Riak specific: bloom filter is no longer read by default,
    //  force read on highly used overlapped table files
    if (s.ok() && VersionSet::IsLevelOverlapped(compact->compaction->level()+1))
        table_ptr->ReadFilter();

    // table_ptr invalidated by this delete
    delete iter;

    if (s.ok()) {
      Log(options_.info_log,
          "Generated table #%llu: %lld keys, %lld bytes",
          (unsigned long long) output_number,
          (unsigned long long) current_entries,
          (unsigned long long) current_bytes);
    }
  }
  return s;
}


Status DBImpl::InstallCompactionResults(CompactionState* compact) {
  mutex_.AssertHeld();

  mutex_.Unlock();
  // release lock while writing Log entry, could stall
  Log(options_.info_log,  "Compacted %d@%d + %d@%d files => %lld bytes",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1,
      static_cast<long long>(compact->total_bytes));
  mutex_.Lock();

  // Add compaction outputs
  compact->compaction->AddInputDeletions(compact->compaction->edit());
  const int level = compact->compaction->level();
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const CompactionState::Output& out = compact->outputs[i];
    compact->compaction->edit()->AddFile2(
        level + 1,
        out.number, out.file_size, out.smallest, out.largest,
        out.exp_write_low, out.exp_write_high, out.exp_explicit_high);
  }
  return versions_->LogAndApply(compact->compaction->edit(), &mutex_);
}

Status DBImpl::DoCompactionWork(CompactionState* compact) {

  assert(versions_->NumLevelFiles(compact->compaction->level()) > 0);
  assert(compact->builder == NULL);
  assert(compact->outfile == NULL);
  if (snapshots_.empty()) {
    compact->smallest_snapshot = versions_->LastSequence();
  } else {
    compact->smallest_snapshot = snapshots_.oldest()->number_;
  }

  // Release mutex while we're actually doing the compaction work
  mutex_.Unlock();

  Log(options_.info_log,  "Compacting %d@%d + %d@%d files",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1);

  bool is_level0_compaction=(0 == compact->compaction->level());

  const uint64_t start_micros = env_->NowMicros();

  Iterator* input = versions_->MakeInputIterator(compact->compaction);
  input->SeekToFirst();
  Status status;

  KeyRetirement retire(user_comparator(), compact->smallest_snapshot, &options_, compact->compaction);

  for (; input->Valid() && !shutting_down_.Acquire_Load(); )
  {
    Slice key = input->key();
    if (compact->builder != NULL
        && compact->compaction->ShouldStopBefore(key, compact->builder->NumEntries())) {

      status = FinishCompactionOutputFile(compact, input);
      if (!status.ok()) {
        break;
      }
    }

    // Handle key/value, add to state, etc.
    bool drop = retire(key);

    if (!drop) {
      // Open output file if necessary
      if (compact->builder == NULL) {
        status = OpenCompactionOutputFile(compact, input->value().size() + key.size());
        if (!status.ok()) {
          break;
        }
      }
      if (compact->builder->NumEntries() == 0) {
        compact->current_output()->smallest.DecodeFrom(key);
      }
      compact->current_output()->largest.DecodeFrom(key);
      compact->builder->Add(key, input->value());

      // Close output file if it is big enough
      if (compact->builder->FileSize() >=
          compact->compaction->MaxOutputFileSize()) {
        status = FinishCompactionOutputFile(compact, input);
        if (!status.ok()) {
          break;
        }
      }
    }

    input->Next();
  }

  if (status.ok() && shutting_down_.Acquire_Load()) {
    status = Status::IOError("Deleting DB during compaction");
#if 0 // validating this block is redundant  (eleveldb issue #110)
    // cleanup Riak modification that adds extra reference
    //  to overlap levels files.
    if (compact->compaction->level() < config::kNumOverlapLevels)
    {
        for (size_t i = 0; i < compact->outputs.size(); i++) {
            const CompactionState::Output& out = compact->outputs[i];
            versions_->GetTableCache()->Evict(out.number, true);
        }   // for
    }   // if
#endif
  }
  if (status.ok() && compact->builder != NULL) {
    status = FinishCompactionOutputFile(compact, input);
  }
  if (status.ok()) {
    status = input->status();
  }
  delete input;
  input = NULL;

  CompactionStats stats;
  stats.micros = env_->NowMicros() - start_micros;
  for (int which = 0; which < 2; which++) {
    for (int i = 0; i < compact->compaction->num_input_files(which); i++) {
      stats.bytes_read += compact->compaction->input(which, i)->file_size;
    }
  }
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    stats.bytes_written += compact->outputs[i].file_size;
  }

  // write log before taking mutex_
  VersionSet::LevelSummaryStorage tmp;
  Log(options_.info_log,
      "compacted to: %s", versions_->LevelSummary(&tmp));

  mutex_.Lock();
  stats_[compact->compaction->level() + 1].Add(stats);

  if (status.ok()) {
    if (0!=compact->num_entries)
        SetThrottleWriteRate((env_->NowMicros() - start_micros),
                             compact->num_entries, is_level0_compaction);
    status = InstallCompactionResults(compact);
  }

  return status;
}


namespace {
struct IterState {
  port::Mutex* mu;
  Version* version;
  MemTable* mem;
  volatile MemTable* imm;
};

static void CleanupIteratorState(void* arg1, void* arg2) {
  IterState* state = reinterpret_cast<IterState*>(arg1);
  state->mu->Lock();
  state->mem->Unref();
  if (state->imm != NULL) state->imm->Unref();
  state->version->Unref();
  state->mu->Unlock();
  delete state;
}
}  // namespace

Iterator* DBImpl::NewInternalIterator(const ReadOptions& options,
                                      SequenceNumber* latest_snapshot) {
  IterState* cleanup = new IterState;
  mutex_.Lock();
  *latest_snapshot = versions_->LastSequence();

  // Collect together all needed child iterators
  std::vector<Iterator*> list;
  list.push_back(mem_->NewIterator());
  mem_->Ref();
  if (imm_ != NULL) {
     list.push_back(((MemTable *)imm_)->NewIterator());
    imm_->Ref();
  }
  versions_->current()->AddIterators(options, &list);
  Iterator* internal_iter =
      NewMergingIterator(&internal_comparator_, &list[0], list.size());
  versions_->current()->Ref();

  cleanup->mu = &mutex_;
  cleanup->mem = mem_;
  cleanup->imm = imm_;
  cleanup->version = versions_->current();
  internal_iter->RegisterCleanup(CleanupIteratorState, cleanup, NULL);

  mutex_.Unlock();
  return internal_iter;
}

Iterator* DBImpl::TEST_NewInternalIterator() {
  SequenceNumber ignored;
  return NewInternalIterator(ReadOptions(), &ignored);
}

int64_t DBImpl::TEST_MaxNextLevelOverlappingBytes() {
  MutexLock l(&mutex_);
  return versions_->MaxNextLevelOverlappingBytes();
}

Status DBImpl::Get(const ReadOptions& options,
                   const Slice& key,
                   std::string* value,
                   KeyMetaData * meta) {
  StringValue stringvalue(*value);
  return DBImpl::Get(options, key, &stringvalue, meta);
}

Status DBImpl::Get(const ReadOptions& options,
                   const Slice& key,
                   Value* value,
                   KeyMetaData * meta) {
  Status s;
  MutexLock l(&mutex_);
  SequenceNumber snapshot;
  if (options.snapshot != NULL) {
    snapshot = reinterpret_cast<const SnapshotImpl*>(options.snapshot)->number_;
  } else {
    snapshot = versions_->LastSequence();
  }

  MemTable* mem = mem_;
  volatile MemTable* imm = imm_;
  Version* current = versions_->current();
  mem->Ref();
  if (imm != NULL) imm->Ref();
  current->Ref();

  bool have_stat_update = false;
  Version::GetStats stats;

  // Unlock while reading from files and memtables
  {
    mutex_.Unlock();
    // First look in the memtable, then in the immutable memtable (if any).
    LookupKey lkey(key, snapshot, meta);
    if (mem->Get(lkey, value, &s, &options_)) {
      // Done
        gPerfCounters->Inc(ePerfGetMem);
    } else if (imm != NULL && ((MemTable *)imm)->Get(lkey, value, &s, &options_)) {
      // Done
        gPerfCounters->Inc(ePerfGetImm);
    } else {
      s = current->Get(options, lkey, value, &stats);
      have_stat_update = true;
      gPerfCounters->Inc(ePerfGetVersion);
    }
    mutex_.Lock();
  }

  if (have_stat_update && current->UpdateStats(stats)) {
      // no compactions initiated by reads, takes too long
      // MaybeScheduleCompaction();
  }
  mem->Unref();
  if (imm != NULL) imm->Unref();
  current->Unref();

  gPerfCounters->Inc(ePerfApiGet);

  return s;
}

Iterator* DBImpl::NewIterator(const ReadOptions& options) {
  SequenceNumber latest_snapshot;
  Iterator* internal_iter = NewInternalIterator(options, &latest_snapshot);
  gPerfCounters->Inc(ePerfIterNew);
  return NewDBIterator(
      &dbname_, env_, user_comparator(), internal_iter,
      (options.snapshot != NULL
       ? reinterpret_cast<const SnapshotImpl*>(options.snapshot)->number_
       : latest_snapshot),
      options_.expiry_module.get());
}

const Snapshot* DBImpl::GetSnapshot() {
  MutexLock l(&mutex_);
  return snapshots_.New(versions_->LastSequence());
}

void DBImpl::ReleaseSnapshot(const Snapshot* s) {
  MutexLock l(&mutex_);
  snapshots_.Delete(reinterpret_cast<const SnapshotImpl*>(s));
}

// Convenience methods
Status DBImpl::Put(const WriteOptions& o, const Slice& key, const Slice& val, const KeyMetaData * meta) {
  return DB::Put(o, key, val, meta);
}

Status DBImpl::Delete(const WriteOptions& options, const Slice& key) {
  return DB::Delete(options, key);
}

Status DBImpl::Write(const WriteOptions& options, WriteBatch* my_batch) {
  Status status;
  int throttle(0);

  Writer w(&mutex_);
  w.batch = my_batch;
  w.sync = options.sync;
  w.done = false;

  {  // place mutex_ within a block
     //  not changing tabs to ease compare to Google sources
  MutexLock l(&mutex_);
  writers_.push_back(&w);
  while (!w.done && &w != writers_.front()) {
    w.cv.Wait();
  }
  if (w.done) {
    return w.status;  // skips throttle ... maintenance unfriendly coding, bastards
  }

  // May temporarily unlock and wait.
  status = MakeRoomForWrite(my_batch == NULL);
  uint64_t last_sequence = versions_->LastSequence();
  Writer* last_writer = &w;
  if (status.ok() && my_batch != NULL) {  // NULL batch is for compactions
    WriteBatch* updates = BuildBatchGroup(&last_writer);
    WriteBatchInternal::SetSequence(updates, last_sequence + 1);
    last_sequence += WriteBatchInternal::Count(updates);

    // Add to log and apply to memtable.  We can release the lock
    // during this phase since &w is currently responsible for logging
    // and protects against concurrent loggers and concurrent writes
    // into mem_.
    {
      mutex_.Unlock();
      status = log_->AddRecord(WriteBatchInternal::Contents(updates));
      if (status.ok() && options.sync) {
        status = logfile_->Sync();
      }
      if (status.ok()) {
        status = WriteBatchInternal::InsertInto(updates, mem_, &options_);
      }
      mutex_.Lock();
    }
    if (updates == tmp_batch_) tmp_batch_->Clear();

    versions_->SetLastSequence(last_sequence);
  }

  while (true) {
    Writer* ready = writers_.front();
    writers_.pop_front();
    if (ready != &w) {
      ready->status = status;
      ready->done = true;
      ready->cv.Signal();
    }
    if (ready == last_writer) break;
  }

  // Notify new head of write queue
  if (!writers_.empty()) {
    writers_.front()->cv.Signal();
  }

  gPerfCounters->Inc(ePerfApiWrite);

  // protect use of versions_ ... still within scope of mutex_ lock
  throttle=versions_->WriteThrottleUsec(IsCompactionScheduled());
  }  // release  MutexLock l(&mutex_)


  // throttle on exit to reduce possible reordering
  if (0!=throttle)
  {
      uint64_t now, remaining_wait, new_end, batch_wait;
      int batch_count;

      /// slowing each call down sequentially
      MutexLock l(&throttle_mutex_);

      // server may have been busy since previous write,
      //  use only the remaining time as throttle
      now=env_->NowMicros();

      if (now < throttle_end)
      {

          remaining_wait=throttle_end - now;
          env_->SleepForMicroseconds(remaining_wait);
          new_end=now+remaining_wait+throttle;

          gPerfCounters->Add(ePerfThrottleWait, remaining_wait);
      }   // if
      else
      {
          remaining_wait=0;
          new_end=now + throttle;
      }   // else

      // throttle is per key write, how many in batch?
      //  (do not use batch count on internal db because of impact to AAE)
      batch_count=(!options_.is_internal_db && NULL!=my_batch ? WriteBatchInternal::Count(my_batch) : 1);
      if (0 < batch_count)  // unclear if Count() could return zero
          --batch_count;
      batch_wait=throttle * batch_count;

      // only wait on batch if extends beyond potential wait period
      if (now + remaining_wait < throttle_end + batch_wait)
      {
          remaining_wait=throttle_end + batch_wait - (now + remaining_wait);
          env_->SleepForMicroseconds(remaining_wait);
          new_end +=remaining_wait;

          gPerfCounters->Add(ePerfThrottleWait, remaining_wait);
      }   // if

      throttle_end=new_end;
  }   // if

  // throttle not needed, kill off old wait time
  else if (0!=throttle_end)
  {
      throttle_end=0;
  }   // else if

  return status;
}

// REQUIRES: Writer list must be non-empty
// REQUIRES: First writer must have a non-NULL batch
// REQUIRES: mutex_ is held
WriteBatch* DBImpl::BuildBatchGroup(Writer** last_writer) {
  mutex_.AssertHeld();
  assert(!writers_.empty());
  Writer* first = writers_.front();
  WriteBatch* result = first->batch;
  assert(result != NULL);

  size_t size = WriteBatchInternal::ByteSize(first->batch);

  // Allow the group to grow up to a maximum size, but if the
  // original write is small, limit the growth so we do not slow
  // down the small write too much.
  size_t max_size = 1 << 20;
  if (size <= (128<<10)) {
    max_size = size + (128<<10);
  }

  *last_writer = first;
  std::deque<Writer*>::iterator iter = writers_.begin();
  ++iter;  // Advance past "first"
  for (; iter != writers_.end(); ++iter) {
    Writer* w = *iter;
    if (w->sync && !first->sync) {
      // Do not include a sync write into a batch handled by a non-sync write.
      break;
    }

    if (w->batch != NULL) {
      size += WriteBatchInternal::ByteSize(w->batch);
      if (size > max_size) {
        // Do not make batch too big
        break;
      }

      // Append to *reuslt
      if (result == first->batch) {
        // Switch to temporary batch instead of disturbing caller's batch
        result = tmp_batch_;
        assert(WriteBatchInternal::Count(result) == 0);
        WriteBatchInternal::Append(result, first->batch);
      }
      WriteBatchInternal::Append(result, w->batch);
    }
    *last_writer = w;
  }
  return result;
}

// REQUIRES: mutex_ is held
// REQUIRES: this thread is currently at the front of the writer queue
Status DBImpl::MakeRoomForWrite(bool force) {
  mutex_.AssertHeld();
  assert(!writers_.empty());
  bool allow_delay = !force;
  Status s;

  while (true) {
    if (!bg_error_.ok()) {
      // Yield previous error
        gPerfCounters->Inc(ePerfWriteError);
      s = bg_error_;
      break;
    } else if (
        allow_delay &&
        versions_->NumLevelFiles(0) >= (int)config::kL0_SlowdownWritesTrigger) {
      // We are getting close to hitting a hard limit on the number of
      // L0 files.  Rather than delaying a single write by several
      // seconds when we hit the hard limit, start delaying each
      // individual write by 1ms to reduce latency variance.  Also,
      // this delay hands over some CPU to the compaction thread in
      // case it is sharing the same core as the writer.
      mutex_.Unlock();
#if 0   // see if this impacts smoothing or helps (but keep the counts)
      // (original Google code left for reference)
      env_->SleepForMicroseconds(1000);
#endif
      allow_delay = false;  // Do not delay a single write more than once
      gPerfCounters->Inc(ePerfWriteSleep);
      mutex_.Lock();
    } else if (!force &&
               (mem_->ApproximateMemoryUsage() <= options_.write_buffer_size)) {
      // There is room in current memtable
        gPerfCounters->Inc(ePerfWriteNoWait);
      break;
    } else if (imm_ != NULL) {
      // We have filled up the current memtable, but the previous
      // one is still being compacted, so we wait.
      Log(options_.info_log, "waiting 2...\n");
      gPerfCounters->Inc(ePerfWriteWaitImm);
      MaybeScheduleCompaction();
      if (!shutting_down_.Acquire_Load())
          bg_cv_.Wait();
      Log(options_.info_log, "running 2...\n");
    } else if (versions_->NumLevelFiles(0) >= config::kL0_StopWritesTrigger) {
      // There are too many level-0 files.
      Log(options_.info_log, "waiting...\n");
      gPerfCounters->Inc(ePerfWriteWaitLevel0);
      MaybeScheduleCompaction();
      if (!shutting_down_.Acquire_Load())
          bg_cv_.Wait();
      Log(options_.info_log, "running...\n");
    } else {
      // Attempt to switch to a new memtable and trigger compaction of old
      assert(versions_->PrevLogNumber() == 0);
      uint64_t new_log_number = versions_->NewFileNumber();

      gPerfCounters->Inc(ePerfWriteNewMem);
      s = NewRecoveryLog(new_log_number);

      if (!s.ok()) {
        // Avoid chewing through file number space in a tight loop.
        versions_->ReuseFileNumber(new_log_number);
        break;
      }

      imm_ = mem_;
      has_imm_.Release_Store((MemTable*)imm_);
      if (NULL!=imm_)
      {
         ThreadTask * task=new ImmWriteTask(this);
         gImmThreads->Submit(task, true);
      }
      mem_ = new MemTable(internal_comparator_);
      mem_->Ref();

      force = false;   // Do not force another compaction if have room
      MaybeScheduleCompaction();
    }
  }
  return s;
}


// the following steps existed in two places, DB::Open() and
//  DBImpl::MakeRoomForWrite().  This lead to a bug in Basho's
//  tiered storage feature.  Unifying the code.
Status DBImpl::NewRecoveryLog(
    uint64_t NewLogNumber)
{
    mutex_.AssertHeld();
    Status s;
    WritableFile * lfile(NULL);

    s = env_->NewWriteOnlyFile(LogFileName(dbname_, NewLogNumber), &lfile,
                               options_.env->RecoveryMmapSize(&options_));
    if (s.ok())
    {
        // close any existing
        delete log_;
        delete logfile_;

        logfile_ = lfile;
        logfile_number_ = NewLogNumber;
        log_ = new log::Writer(lfile);
    }   // if

    return(s);

}   // DBImpl::NewRecoveryLog


bool DBImpl::GetProperty(const Slice& property, std::string* value) {
  value->clear();

  MutexLock l(&mutex_);
  Slice in = property;
  Slice prefix("leveldb.");
  if (!in.starts_with(prefix)) return false;
  in.remove_prefix(prefix.size());

  if (in.starts_with("num-files-at-level")) {
    in.remove_prefix(strlen("num-files-at-level"));
    uint64_t level;
    bool ok = ConsumeDecimalNumber(&in, &level) && in.empty();
    if (!ok || level >= (uint64_t)config::kNumLevels) {
      return false;
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "%zd",
               versions_->NumLevelFiles(static_cast<int>(level)));
      *value = buf;
      return true;
    }
  } else if (in == "stats") {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "                               Compactions\n"
             "Level  Files Size(MB) Time(sec) Read(MB) Write(MB)\n"
             "--------------------------------------------------\n"
             );
    value->append(buf);
    for (int level = 0; level < config::kNumLevels; level++) {
      int files = versions_->NumLevelFiles(level);
      if (stats_[level].micros > 0 || files > 0) {
        snprintf(
            buf, sizeof(buf),
            "%3d %8d %8.0f %9.0f %8.0f %9.0f\n",
            level,
            files,
            versions_->NumLevelBytes(level) / 1048576.0,
            stats_[level].micros / 1e6,
            stats_[level].bytes_read / 1048576.0,
            stats_[level].bytes_written / 1048576.0);
        value->append(buf);
      }
    }
    return true;
  } else if (in == "sstables") {
    *value = versions_->current()->DebugString();
    return true;
  } else if (in == "total-bytes") {
    char buf[50];
    uint64_t total = 0;
    for (int level = 0; level < config::kNumLevels; level++) {
      total += versions_->NumLevelBytes(level);
    }
    snprintf(buf, sizeof(buf), "%" PRIu64, total);
    value->append(buf);
    return true;
  } else if (in == "file-cache") {
    char buf[50];
    snprintf(buf, sizeof(buf), "%zd", double_cache.GetCapacity(true));
    value->append(buf);
    return true;
  } else if (in == "block-cache") {
    char buf[50];
    snprintf(buf, sizeof(buf), "%zd", double_cache.GetCapacity(false));
    value->append(buf);
    return true;
  } else if (-1!=gPerfCounters->LookupCounter(in.ToString().c_str())) {

      char buf[66];
      int index;

      index=gPerfCounters->LookupCounter(in.ToString().c_str());
      snprintf(buf, sizeof(buf), "%" PRIu64 , gPerfCounters->Value(index));
      value->append(buf);
      return(true);
  }

  return false;
}

void DBImpl::GetApproximateSizes(
    const Range* range, int n,
    uint64_t* sizes) {
  // TODO(opt): better implementation
  Version* v;
  {
    MutexLock l(&mutex_);
    versions_->current()->Ref();
    v = versions_->current();
  }

  for (int i = 0; i < n; i++) {
    // Convert user_key into a corresponding internal key.
    InternalKey k1(range[i].start, 0, kMaxSequenceNumber, kValueTypeForSeek);
    InternalKey k2(range[i].limit, 0, kMaxSequenceNumber, kValueTypeForSeek);
    uint64_t start = versions_->ApproximateOffsetOf(v, k1);
    uint64_t limit = versions_->ApproximateOffsetOf(v, k2);
    sizes[i] = (limit >= start ? limit - start : 0);
  }

  {
    MutexLock l(&mutex_);
    v->Unref();
  }
}

// Default implementations of convenience methods that subclasses of DB
// can call if they wish
Status DB::Put(const WriteOptions& opt, const Slice& key, const Slice& value,
  const KeyMetaData * meta) {
  WriteBatch batch;
  batch.Put(key, value, meta);
  return Write(opt, &batch);
}

Status DB::Delete(const WriteOptions& opt, const Slice& key) {
  WriteBatch batch;
  batch.Delete(key);

  // Negate the count to "ApiWrite"
  gPerfCounters->Dec(ePerfApiWrite);
  gPerfCounters->Inc(ePerfApiDelete);

  return Write(opt, &batch);
}

DB::~DB() { }

Status DB::Open(const Options& options, const std::string& dbname,
                DB** dbptr) {
  *dbptr = NULL;

  DBImpl* impl = new DBImpl(options, dbname);
  impl->mutex_.Lock();
  VersionEdit edit;
  Status s;

  // WARNING:  only use impl and impl->options_ from this point.
  //           Things like tiered storage change the meanings

  // 4 level0 files at 2Mbytes and 2Mbytes of block cache
  //  (but first level1 file is likely to thrash)
  //  ... this value is AFTER write_buffer and 40M for recovery log and LOG
  //if (!options.limited_developer_mem && impl->GetCacheCapacity() < flex::kMinimumDBMemory)
  //    s=Status::InvalidArgument("Less than 10Mbytes per database/vnode");

  if (s.ok())
      s = impl->Recover(&edit); // Handles create_if_missing, error_if_exists

  if (s.ok()) {
    uint64_t new_log_number = impl->versions_->NewFileNumber();

    s = impl->NewRecoveryLog(new_log_number);

    if (s.ok()) {
      edit.SetLogNumber(new_log_number);
      s = impl->versions_->LogAndApply(&edit, &impl->mutex_);
    }
    if (s.ok()) {
      impl->DeleteObsoleteFiles();
      impl->CheckCompactionState();
    }
  }

  if (impl->options_.cache_object_warming)
      impl->table_cache_->PreloadTableCache();

  impl->mutex_.Unlock();
  if (s.ok()) {
    *dbptr = impl;
  } else {
    delete impl;
  }

  gPerfCounters->Inc(ePerfApiOpen);

  return s;
}

Snapshot::~Snapshot() {
}

Status DestroyDB(const std::string& dbname, const Options& options) {
  Env* env = options.env;
  std::vector<std::string> filenames;
  Options options_tiered;
  std::string dbname_tiered;

  options_tiered=options;
  dbname_tiered=MakeTieredDbname(dbname, options_tiered);

  // Ignore error in case directory does not exist
  env->GetChildren(dbname_tiered, &filenames);
  if (filenames.empty()) {
    return Status::OK();
  }

  FileLock* lock;
  const std::string lockname = LockFileName(dbname_tiered);
  Status result = env->LockFile(lockname, &lock);
  if (result.ok()) {
    uint64_t number;
    FileType type;

    // prune the table file directories
    for (int level=0; level<config::kNumLevels; ++level)
    {
        std::string dirname;

        filenames.clear();
        dirname=MakeDirName2(options_tiered, level, "sst");
        env->GetChildren(dirname, &filenames); // Ignoring errors on purpose
        for (size_t i = 0; i < filenames.size(); i++) {
            if (ParseFileName(filenames[i], &number, &type)) {
                Status del = env->DeleteFile(dirname + "/" + filenames[i]);
                if (result.ok() && !del.ok()) {
                    result = del;
                }   // if
            }   // if
        }   // for
        env->DeleteDir(dirname);
    }   // for

    filenames.clear();
    env->GetChildren(dbname_tiered, &filenames);
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type) &&
          type != kDBLockFile) {  // Lock file will be deleted at end
        Status del = env->DeleteFile(dbname_tiered + "/" + filenames[i]);
        if (result.ok() && !del.ok()) {
          result = del;
        }
      }
    }
    env->UnlockFile(lock);  // Ignore error since state is already gone
    env->DeleteFile(lockname);
    env->DeleteDir(options.tiered_fast_prefix);  // Ignore error in case dir contains other files
    env->DeleteDir(options.tiered_slow_prefix);  // Ignore error in case dir contains other files
  }
  return result;
}


Status DB::VerifyLevels() {return(Status::InvalidArgument("is_repair not set in Options before database opened"));};

// Riak specific repair
Status
DBImpl::VerifyLevels()
{
    Status result;

    // did they remember to open the db with flag set in options
    if (options_.is_repair)
    {
        InternalKey begin, end;
        bool overlap_found;
        int level;
        Version * ver;

        overlap_found=false;
        level=0;

        do
        {
            // get a copy of current version
            {
                MutexLock l(&mutex_);
                ver = versions_->current();
                ver->Ref();
            }

            // level is input and output (acts as cursor to progress)
            //  begin and end are outputs of function
            overlap_found=ver->VerifyLevels(level, begin, end);
            ver->Unref();

            if (overlap_found)
            {
                Slice s_begin, s_end;

                s_begin=begin.user_key();
                s_end=end.user_key();
                TEST_CompactRange(level, &s_begin, &s_end);
            }   // if

        } while(overlap_found);

    }   // if
    else
    {
        result=Status::InvalidArgument("is_repair not set in Options before database opened");
    }   // else

    return(result);

}   // VerifyLevels

void DB::CheckAvailableCompactions() {return;};

// Used internally for inter-database notification
//  of potential grooming timeslot availability.
void
DBImpl::CheckAvailableCompactions()
{
    MutexLock l(&mutex_);
    MaybeScheduleCompaction();

    return;
}   // CheckAvailableCompactions


bool
DBImpl::IsCompactionScheduled()
{
    mutex_.AssertHeld();
    bool flag(false);
    for (int level=0; level< config::kNumLevels && !flag; ++level)
        flag=versions_->IsCompactionSubmitted(level);
    return(flag || NULL!=imm_ || hotbackup_pending_);
}   // DBImpl::IsCompactionScheduled

}  // namespace leveldb
