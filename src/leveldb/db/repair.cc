// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// We recover the contents of the descriptor from the other files we find.
// (1) Any log files are first converted to tables
// (2) We scan every table to compute
//     (a) smallest/largest for the table
//     (b) largest sequence number in the table
// (3) We generate descriptor contents:
//      - log number is set to zero
//      - next-file-number is set to 1 + largest file number we found
//      - last-sequence-number is set to largest sequence# found across
//        all tables (see 2c)
//      - compaction pointers are cleared
//      - every table file is added at level 0
//
// Possible optimization 1:
//   (a) Compute total size and use to pick appropriate max-level M
//   (b) Sort tables by largest sequence# in the table
//   (c) For each table: if it overlaps earlier table, place in level-0,
//       else place in level-M.
// Possible optimization 2:
//   Store per-table metadata (smallest, largest, largest-seq#, ...)
//   in the table's meta section to speed up ScanTable.

#include "db/builder.h"
#include "db/db_impl.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "db/write_batch_internal.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"

namespace leveldb {

namespace {

class Repairer {
 public:
  Repairer(const std::string& dbname, const Options& options)
      : double_cache_(options),
        options_(SanitizeOptions(dbname, &icmp_, &ipolicy_, options, double_cache_.GetBlockCache())),
        org_options_(options),
        dbname_(options_.tiered_fast_prefix),
        org_dbname_(dbname),
        env_(options.env),
        icmp_(options.comparator),
        ipolicy_(options.filter_policy),
        owns_info_log_(options_.info_log != options.info_log),
        db_lock_(NULL),
        next_file_number_(1)
  {
    // TableCache can be small since we expect each table to be opened once.
    table_cache_ = new TableCache(dbname_, &options_, double_cache_.GetFileCache(), double_cache_);

  }

  ~Repairer() {
    if (owns_info_log_) {
      delete options_.info_log;
    }
//    if (owns_cache_) {
//      delete options_.block_cache;
//    }

    // must remove second ref counter that keeps overlapped files locked
    //  table cache
    bool is_overlap;
    for (int level = 0; level < config::kNumLevels; level++) {
        {
            is_overlap=(level < leveldb::config::kNumOverlapLevels);
            for (size_t i = 0; i < table_numbers_[level].size(); i++) {
                table_cache_->Evict(table_numbers_[level][i], is_overlap);
            }   // for
        }   // if
    } // for

    delete table_cache_;
  }

  Status Run() {
    Status status;

    status = env_->LockFile(LockFileName(dbname_), &db_lock_);

    if (status.ok())
        status = MakeLevelDirectories(env_, options_);

    if (status.ok()) {
      status = FindFiles();
      if (status.ok()) {
          ConvertLogFilesToTables();
          ExtractMetaData();
          status = WriteDescriptor();
      }
      if (status.ok()) {
        unsigned long long bytes = 0;
        unsigned long long files = 0;

        // calculate size for log information
        for (int level=0; level<config::kNumLevels;++level)
        {
          std::vector<TableInfo> * table_ptr;
          std::vector<TableInfo>::const_iterator i;

          table_ptr=&tables_[level];
          files+=table_ptr->size();

          for ( i = table_ptr->begin(); table_ptr->end()!= i; i++) {
            bytes += i->meta.file_size;
          }
        } // for

        Log(options_.info_log,
            "**** Repaired leveldb %s; "
            "recovered %d files; %llu bytes. "
            "Some data may have been lost. "
            "****",
            dbname_.c_str(),
            static_cast<int>(files),
            bytes);
      }
      if (db_lock_ != NULL) {
        env_->UnlockFile(db_lock_);
      }
    }

    // perform Riak specific scan for overlapping .sst files
    //  within a level
    if (status.ok())
    {
        leveldb::DB * db_ptr;
        Options options;

        db_ptr=NULL;
        options=org_options_;
//        options.block_cache=NULL;  // not reusing for fear of edge cases
        options.is_repair=true;
        options.error_if_exists=false;
        status=leveldb::DB::Open(options, org_dbname_, &db_ptr);

        if (status.ok())
            status=db_ptr->VerifyLevels();

        delete db_ptr;

    }   // if
    return status;
  }

 private:
  struct TableInfo {
    FileMetaData meta;
    SequenceNumber max_sequence;
  };

  DoubleCache double_cache_;
  Options const options_, org_options_;
  std::string const dbname_, org_dbname_;
  Env* const env_;
  InternalKeyComparator const icmp_;
  InternalFilterPolicy const ipolicy_;
  bool owns_info_log_;
  FileLock* db_lock_;
  TableCache* table_cache_;
  VersionEdit edit_;

  std::vector<std::string> manifests_;
  std::vector<uint64_t> table_numbers_[config::kNumLevels];
  std::vector<uint64_t> logs_;
  std::vector<TableInfo> tables_[config::kNumLevels];
  uint64_t next_file_number_;

  Status FindFiles()
  {
    std::vector<std::string> filenames;
    uint64_t number;
    FileType type;
    int level;

    // base directory
    Status status = env_->GetChildren(dbname_, &filenames);
    if (!status.ok()) {
      return status;
    }

    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type)) {
        if (type == kDescriptorFile) {
          manifests_.push_back(filenames[i]);
        } else {
          if (number + 1 > next_file_number_) {
            next_file_number_ = number + 1;
          }
          if (type == kLogFile) {
            logs_.push_back(number);
          } else if (type == kTableFile) {
            table_numbers_[0].push_back(number);
          } else {
            // Ignore other files
          } // else
        } // else
      } // if
    } // for

    for (level=0; level < config::kNumLevels; ++level)
    {
      std::string dirname;

      filenames.clear();
      dirname=MakeDirName2(options_, level, "sst");
      Status status = env_->GetChildren(dirname, &filenames);
      if (!status.ok()) {
          return status;
      }

      for (size_t i = 0; i < filenames.size(); i++) {
        if (ParseFileName(filenames[i], &number, &type)) {
          if (number + 1 > next_file_number_) {
            next_file_number_ = number + 1;
          }

          if (type == kTableFile) {
            table_numbers_[level].push_back(number);
          }
        } // if
      } // for
    } // for

    return status;
  }

  void ConvertLogFilesToTables() {
    for (size_t i = 0; i < logs_.size(); i++) {
      std::string logname = LogFileName(dbname_, logs_[i]);
      Status status = ConvertLogToTable(logs_[i]);
      if (!status.ok()) {
        Log(options_.info_log, "Log #%llu: ignoring conversion error: %s",
            (unsigned long long) logs_[i],
            status.ToString().c_str());
      }
      ArchiveFile(logname);
    }
  }

  Status ConvertLogToTable(uint64_t log) {
    struct LogReporter : public log::Reader::Reporter {
      Env* env;
      Logger* info_log;
      uint64_t lognum;
      virtual void Corruption(size_t bytes, const Status& s) {
        // We print error messages for corruption, but continue repairing.
        Log(info_log, "Log #%llu: dropping %d bytes; %s",
            (unsigned long long) lognum,
            static_cast<int>(bytes),
            s.ToString().c_str());
      }
    };

    // Open the log file
    std::string logname = LogFileName(dbname_, log);
    SequentialFile* lfile;
    Status status = env_->NewSequentialFile(logname, &lfile);
    if (!status.ok()) {
      return status;
    }

    // Create the log reader.
    LogReporter reporter;
    reporter.env = env_;
    reporter.info_log = options_.info_log;
    reporter.lognum = log;
    // We intentially make log::Reader do checksumming so that
    // corruptions cause entire commits to be skipped instead of
    // propagating bad information (like overly large sequence
    // numbers).
    log::Reader reader(lfile, &reporter, false/*do not checksum*/,
                       0/*initial_offset*/);

    // Read all the records and add to a memtable
    std::string scratch;
    Slice record;
    WriteBatch batch;
    MemTable* mem = new MemTable(icmp_);
    mem->Ref();
    int counter = 0;
    while (reader.ReadRecord(&record, &scratch)) {
      if (record.size() < 12) {
        reporter.Corruption(
            record.size(), Status::Corruption("log record too small"));
        continue;
      }
      WriteBatchInternal::SetContents(&batch, record);
      status = WriteBatchInternal::InsertInto(&batch, mem, &options_);
      if (status.ok()) {
        counter += WriteBatchInternal::Count(&batch);
      } else {
        Log(options_.info_log, "Log #%llu: ignoring %s",
            (unsigned long long) log,
            status.ToString().c_str());
        status = Status::OK();  // Keep going with rest of file
      }
    }
    delete lfile;

    // Do not record a version edit for this conversion to a Table
    // since ExtractMetaData() will also generate edits.
    FileMetaData meta;
    meta.number = next_file_number_++;
    meta.level = 0;
    Iterator* iter = mem->NewIterator();
    status = BuildTable(dbname_, env_, options_, icmp_.user_comparator(), table_cache_, iter, &meta, 0);
    delete iter;
    mem->Unref();
    mem = NULL;
    if (status.ok()) {
      if (meta.file_size > 0) {
        table_numbers_[0].push_back(meta.number);
      }
    }
    Log(options_.info_log, "Log #%llu: %d ops saved to Table #%llu %s",
        (unsigned long long) log,
        counter,
        (unsigned long long) meta.number,
        status.ToString().c_str());
    return status;
  }

  void ExtractMetaData() {
    for (int level=0; level < config::kNumLevels; ++level)
    {
      std::vector<uint64_t> * number_ptr;
      std::vector<uint64_t>::const_iterator i;

      number_ptr=&table_numbers_[level];
      for (i = number_ptr->begin(); number_ptr->end()!= i; ++i) {
        TableInfo t;
        t.meta.number = *i;
        t.meta.level = level;
        Status status = ScanTable(&t);
        if (!status.ok())
        {
          std::string fname = TableFileName(options_, t.meta.number, t.meta.level);
          Log(options_.info_log, "Table #%llu: ignoring %s",
              (unsigned long long) t.meta.number,
              status.ToString().c_str());
          ArchiveFile(fname, true);
        } else {
          tables_[level].push_back(t);
        }
      }
    }
  }

  Status ScanTable(TableInfo* t) {
    Table * table_ptr;
    SstCounters counters;
    std::string fname = TableFileName(options_, t->meta.number, t->meta.level);
    int counter = 0;
    Status status = env_->GetFileSize(fname, &t->meta.file_size);
    if (status.ok()) {
      Iterator* iter = table_cache_->NewIterator(
          ReadOptions(), t->meta.number, t->meta.file_size, t->meta.level, &table_ptr);
      bool empty = true;
      ParsedInternalKey parsed;
      t->max_sequence = 0;
      for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        Slice key = iter->key();
        if (!ParseInternalKey(key, &parsed)) {
          Log(options_.info_log, "Table #%llu: unparsable key %s",
              (unsigned long long) t->meta.number,
              EscapeString(key).c_str());
          continue;
        }

        counter++;
        if (empty) {
          empty = false;
          t->meta.smallest.DecodeFrom(key);
        }
        t->meta.largest.DecodeFrom(key);
        if (parsed.sequence > t->max_sequence) {
          t->max_sequence = parsed.sequence;
        }
      }
      if (!iter->status().ok()) {
        status = iter->status();
      }
      else {
        counters=table_ptr->GetSstCounters();
        t->meta.exp_write_low=counters.Value(eSstCountExpiry1);
        t->meta.exp_write_high=counters.Value(eSstCountExpiry2);
        t->meta.exp_explicit_high=counters.Value(eSstCountExpiry3);
      }
      delete iter;
    }
    Log(options_.info_log, "Table #%llu: %d entries %s",
        (unsigned long long) t->meta.number,
        counter,
        status.ToString().c_str());
    return status;
  }

  Status WriteDescriptor() {
    std::string tmp = TempFileName(dbname_, 1);
    WritableFile* file;
    Status status = env_->NewWritableFile(tmp, &file, 4096);
    if (!status.ok()) {
      return status;
    }

    SequenceNumber max_sequence = 0;
    for (int level=0; level<config::kNumLevels;++level)
    {
      std::vector<TableInfo> * table_ptr;
      std::vector<TableInfo>::const_iterator i;

      table_ptr=&tables_[level];

      for ( i = table_ptr->begin(); table_ptr->end()!= i; i++) {
        if (max_sequence < i->max_sequence) {
          max_sequence = i->max_sequence;
        }
      } // for
    } // for

    edit_.SetComparatorName(icmp_.user_comparator()->Name());
    edit_.SetLogNumber(0);
    edit_.SetNextFile(next_file_number_);
    edit_.SetLastSequence(max_sequence);

    for (int level=0; level<config::kNumLevels;++level)
    {
      std::vector<TableInfo> * table_ptr;
      std::vector<TableInfo>::const_iterator i;

      table_ptr=&tables_[level];

      for ( i = table_ptr->begin(); table_ptr->end()!= i; i++) {
          edit_.AddFile2(level, i->meta.number, i->meta.file_size,
                         i->meta.smallest, i->meta.largest,
                         i->meta.exp_write_low, i->meta.exp_write_high, i->meta.exp_explicit_high);

      } // for
    } // for

    //fprintf(stderr, "NewDescriptor:\n%s\n", edit_.DebugString().c_str());
    {
      log::Writer log(file);
      std::string record;
      edit_.EncodeTo(&record);  // manifest format is default for release, options_ often incomplete
      status = log.AddRecord(record);
    }
    if (status.ok()) {
      status = file->Close();
    }
    delete file;
    file = NULL;

    if (!status.ok()) {
      env_->DeleteFile(tmp);
    } else {
      // Discard older manifests
      for (size_t i = 0; i < manifests_.size(); i++) {
        ArchiveFile(dbname_ + "/" + manifests_[i]);
      }

      // Install new manifest
      status = env_->RenameFile(tmp, DescriptorFileName(dbname_, 1));
      if (status.ok()) {
        status = SetCurrentFile(env_, dbname_, 1);
      } else {
        env_->DeleteFile(tmp);
      }
    }
    return status;
  }

  void ArchiveFile(const std::string& fname, bool two_levels=false) {
    // Move into another directory.  E.g., for
    //    dir/foo
    // rename to
    //    dir/lost/foo
    std::string::size_type slash, slash2;

    slash=fname.rfind('/');
    if (two_levels && std::string::npos!=slash && 0<slash)
    {
        slash2=fname.rfind('/',slash-1);
        if (std::string::npos==slash2)
            slash2=slash;
    }   // if
    else
        slash2=slash;

    std::string new_dir;

    if (std::string::npos != slash2 && 0<slash2)
      new_dir.append(fname,0,slash2);

    new_dir.append("/lost");
    env_->CreateDir(new_dir);  // Ignore error
    std::string new_file = new_dir;
    new_file.append("/");
    new_file.append((std::string::npos!=slash) ? fname.substr(slash+1) : fname);
    Status s = env_->RenameFile(fname, new_file);
    Log(options_.info_log, "Archiving %s: %s\n",
        fname.c_str(), s.ToString().c_str());
  }
};
}  // namespace

Status RepairDB(const std::string& dbname, const Options& options) {
  Repairer repairer(dbname, options);
  return repairer.Run();
}

}  // namespace leveldb
