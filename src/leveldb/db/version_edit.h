// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_VERSION_EDIT_H_
#define STORAGE_LEVELDB_DB_VERSION_EDIT_H_

#include <set>
#include <utility>
#include <vector>
#include "db/dbformat.h"

namespace leveldb {

class VersionSet;

struct FileMetaData {
  int refs;
//  int allowed_seeks;          // Seeks allowed until compaction
  uint64_t number;
  uint64_t file_size;         // File size in bytes
  uint64_t num_entries;       // count of values in .sst file, only valid during table build
  InternalKey smallest;       // Smallest internal key served by table
  InternalKey largest;        // Largest internal key served by table
  int level;
  ExpiryTimeMicros exp_write_low;     // oldest write time in file:
                                //  0 - non-expiry keys exist too
                                //  ULLONG_MAX - no write time expiry & no plain keys
  ExpiryTimeMicros exp_write_high;    // most recent write time in file
  ExpiryTimeMicros exp_explicit_high; // most recent/furthest into future explicit expiry

  FileMetaData()
  : refs(0), /*allowed_seeks(1 << 30),*/ file_size(0),
      num_entries(0), level(-1), exp_write_low(0), exp_write_high(0), exp_explicit_high(0)
  { }
};


class FileMetaDataPtrCompare
{
protected:
    const Comparator * comparator_;

public:
    explicit FileMetaDataPtrCompare(const Comparator * Comparer)
        : comparator_(Comparer) {};

    bool operator() (const FileMetaData * file1, const FileMetaData * file2) const
    {
        return(comparator_->Compare(file1->smallest.user_key(), file2->smallest.user_key()) < 0);
    }
};  // class FileMetaDataPtrCompare

class VersionEdit {
 public:
  VersionEdit() { Clear(); }
  ~VersionEdit() { }

  void Clear();

  void SetComparatorName(const Slice& name) {
    has_comparator_ = true;
    comparator_ = name.ToString();
  }
  void SetLogNumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  void SetPrevLogNumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void SetNextFile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void SetLastSequence(SequenceNumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }
  void SetCompactPointer(int level, const InternalKey& key) {
    compact_pointers_.push_back(std::make_pair(level, key));
  }

  // Add the specified file at the specified number.
  // REQUIRES: This version has not been saved (see VersionSet::SaveTo)
  // REQUIRES: "smallest" and "largest" are smallest and largest keys in file
#if 0
  void AddFile(int level, uint64_t file,
               uint64_t file_size,
               const InternalKey& smallest,
               const InternalKey& largest) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
    f.level = level;
    new_files_.push_back(std::make_pair(level, f));
  }
#endif

  void AddFile2(int level, uint64_t file,
                uint64_t file_size,
                const InternalKey& smallest,
                const InternalKey& largest,
                uint64_t exp_write_low,
                uint64_t exp_write_high,
                uint64_t exp_explicit_high) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
    f.level = level;
    f.exp_write_low = exp_write_low;
    f.exp_write_high = exp_write_high;
    f.exp_explicit_high = exp_explicit_high;
    new_files_.push_back(std::make_pair(level, f));
  }

  // Delete the specified "file" from the specified "level".
  void DeleteFile(int level, uint64_t file) {
    deleted_files_.insert(std::make_pair(level, file));
  }
  size_t DeletedFileCount() const {return(deleted_files_.size());};

  void EncodeTo(std::string* dst, bool format2=true) const;
  Status DecodeFrom(const Slice& src);

  // unit test access to validate file entries' format types
  bool HasF1Files() const {return(has_f1_files_);};
  bool HasF2Files() const {return(has_f2_files_);};

  std::string DebugString() const;

// Tag numbers for serialized VersionEdit.  These numbers are written to
// disk and should not be changed.
enum Tag {
  kComparator           = 1,
  kLogNumber            = 2,
  kNextFileNumber       = 3,
  kLastSequence         = 4,
  kCompactPointer       = 5,
  kDeletedFile          = 6,
  kNewFile              = 7,
  // 8 was used for large value refs
  kPrevLogNumber        = 9,
  kFileCacheObject      = 10,
  kNewFile2             = 11  // expiry capable file
};

 private:
  friend class VersionSet;

  USED_BY_NESTED_FRIEND2(typedef std::set< std::pair<int, uint64_t> > DeletedFileSet)

  std::string comparator_;
  uint64_t log_number_;
  uint64_t prev_log_number_;
  uint64_t next_file_number_;
  SequenceNumber last_sequence_;
  bool has_comparator_;
  bool has_log_number_;
  bool has_prev_log_number_;
  bool has_next_file_number_;
  bool has_last_sequence_;
  // following should be mutually exclusive, but tested independently to be sure
  bool has_f1_files_;         // manifest uses format 1 (for unit tests)
  bool has_f2_files_;         // manifest uses format 2 (for unit tests)

  USED_BY_NESTED_FRIEND2(std::vector< std::pair<int, InternalKey> > compact_pointers_)
  USED_BY_NESTED_FRIEND(DeletedFileSet deleted_files_)
  USED_BY_NESTED_FRIEND2(std::vector< std::pair<int, FileMetaData> > new_files_)
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_EDIT_H_
