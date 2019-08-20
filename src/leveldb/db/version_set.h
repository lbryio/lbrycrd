// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// The representation of a DBImpl consists of a set of Versions.  The
// newest version is called "current".  Older versions may be kept
// around to provide a consistent view to live iterators.
//
// Each Version keeps track of a set of Table files per level.  The
// entire set of versions is maintained in a VersionSet.
//
// Version,VersionSet are thread-compatible, but require external
// synchronization on all accesses.

#ifndef STORAGE_LEVELDB_DB_VERSION_SET_H_
#define STORAGE_LEVELDB_DB_VERSION_SET_H_

#include <map>
#include <set>
#include <vector>
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "port/port.h"
#include "leveldb/atomics.h"
#include "leveldb/env.h"
#include "util/throttle.h"

namespace leveldb {

namespace log { class Writer; }

class Compaction;
class Iterator;
class MemTable;
class TableBuilder;
class TableCache;
class Version;
class VersionSet;
class WritableFile;

// Return the smallest index i such that files[i]->largest >= key.
// Return files.size() if there is no such file.
// REQUIRES: "files" contains a sorted list of non-overlapping files.
extern int FindFile(const InternalKeyComparator& icmp,
                    const std::vector<FileMetaData*>& files,
                    const Slice& key);

// Returns true iff some file in "files" overlaps the user key range
// [*smallest,*largest].
// smallest==NULL represents a key smaller than all keys in the DB.
// largest==NULL represents a key largest than all keys in the DB.
// REQUIRES: If disjoint_sorted_files, files[] contains disjoint ranges
//           in sorted order.
extern bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key);

class Version {
 public:
  // Append to *iters a sequence of iterators that will
  // yield the contents of this Version when merged together.
  // REQUIRES: This version has been saved (see VersionSet::SaveTo)
  void AddIterators(const ReadOptions&, std::vector<Iterator*>* iters);

  // Lookup the value for key.  If found, store it in *val and
  // return OK.  Else return a non-OK status.  Fills *stats.
  // REQUIRES: lock is not held
  struct GetStats {
    FileMetaData* seek_file;
    int seek_file_level;
  };
  Status Get(const ReadOptions&, const LookupKey& key, Value* val,
             GetStats* stats);

  // Adds "stats" into the current state.  Returns true if a new
  // compaction may need to be triggered, false otherwise.
  // REQUIRES: lock is held
  bool UpdateStats(const GetStats& stats);

  // Reference count management (so Versions do not disappear out from
  // under live iterators)
  void Ref();
  void Unref();

  void GetOverlappingInputs(
      int level,
      const InternalKey* begin,         // NULL means before all keys
      const InternalKey* end,           // NULL means after all keys
      std::vector<FileMetaData*>* inputs);

  // Returns true iff some file in the specified level overlaps
  // some part of [*smallest_user_key,*largest_user_key].
  // smallest_user_key==NULL represents a key smaller than all keys in the DB.
  // largest_user_key==NULL represents a key largest than all keys in the DB.
  bool OverlapInLevel(int level,
                      const Slice* smallest_user_key,
                      const Slice* largest_user_key) const;

  // Return the level at which we should place a new memtable compaction
  // result that covers the range [smallest_user_key,largest_user_key].
  int PickLevelForMemTableOutput(const Slice& smallest_user_key,
                                 const Slice& largest_user_key,
                                 const int level_limit);

  virtual size_t NumFiles(int level) const { return files_[level].size(); }

  const VersionSet * GetVersionSet() const { return vset_; }

  typedef std::vector<FileMetaData*> FileMetaDataVector_t;

  virtual const std::vector<FileMetaData*> & GetFileList(int level) const {return files_[level];};

  volatile int WritePenalty() const {return write_penalty_; }

  // Riak specific repair routine
  bool VerifyLevels(int & level, InternalKey & begin, InternalKey & end);

  // Return a human readable string that describes this version's contents.
  std::string DebugString() const;

protected:
  friend class Compaction;
  friend class VersionSet;

  class LevelFileNumIterator;
  Iterator* NewConcatenatingIterator(const ReadOptions&, int level) const;

  VersionSet* vset_;            // VersionSet to which this Version belongs
  Version* next_;               // Next version in linked list
  Version* prev_;               // Previous version in linked list
  int refs_;                    // Number of live refs to this version

  // List of files per level
  USED_BY_NESTED_FRIEND(std::vector<FileMetaData*> files_[config::kNumLevels];)

 protected:
  // Next file to compact based on seek stats (or Riak delete test)
  FileMetaData* file_to_compact_;
  int file_to_compact_level_;

  // Level that should be compacted next and its compaction score.
  // Score < 1 means compaction is not strictly needed.  These fields
  // are initialized by Finalize().
  double compaction_score_;
  int compaction_level_;
  bool compaction_grooming_;
  bool compaction_no_move_;
  bool compaction_expirefile_;
  volatile int write_penalty_;

 protected:
  // make the ctor/dtor protected, so that a unit test can subclass
  explicit Version(VersionSet* vset)
      : vset_(vset), next_(this), prev_(this), refs_(0),
        file_to_compact_(NULL),
        file_to_compact_level_(-1),
        compaction_score_(-1),
        compaction_level_(-1),
        compaction_grooming_(false),
        compaction_no_move_(false),
        compaction_expirefile_(false),
        write_penalty_(0)
  {
  }

  virtual ~Version();

private:
  // No copying allowed
  Version(const Version&);
  void operator=(const Version&);
};

class VersionSet {
 public:
  VersionSet(const std::string& dbname,
             const Options* options,
             TableCache* table_cache,
             const InternalKeyComparator*);
  ~VersionSet();

  // Apply *edit to the current version to form a new descriptor that
  // is both saved to persistent state and installed as the new
  // current version.  Will release *mu while actually writing to the file.
  // REQUIRES: *mu is held on entry.
  // REQUIRES: no other thread concurrently calls LogAndApply()
  Status LogAndApply(VersionEdit* edit, port::Mutex* mu);

  // Recover the last saved descriptor from persistent storage.
  Status Recover();

  // Return the current version.
  Version* current() const { return current_; }

  // Return the current manifest file number
  uint64_t ManifestFileNumber() const { return manifest_file_number_; }

  // Allocate and return a new file number
  //  (-1 is to "duplicate" old post-increment logic while maintaining
  //   some threading integrity ... next_file_number_ used naked a bunch)
  uint64_t NewFileNumber() { return(inc_and_fetch(&next_file_number_) -1); }

  // Arrange to reuse "file_number" unless a newer file number has
  // already been allocated.
  // REQUIRES: "file_number" was returned by a call to NewFileNumber().
  //  (disabled due to threading concerns ... and desire NOT to use mutex, matthewv)
  void ReuseFileNumber(uint64_t file_number) {
//    if (next_file_number_ == file_number + 1) {
//      next_file_number_ = file_number;
//    }
  }

  // Return the number of Table files at the specified level.
  size_t NumLevelFiles(int level) const;

  // is the specified level overlapped (or if false->sorted)
  static bool IsLevelOverlapped(int level);

  static uint64_t DesiredBytesForLevel(int level);
  static uint64_t MaxBytesForLevel(int level);
  static uint64_t MaxFileSizeForLevel(int level);

  // Return the combined file size of all files at the specified level.
  int64_t NumLevelBytes(int level) const;

  // Return the last sequence number.
  uint64_t LastSequence() const { return last_sequence_; }

  // Set the last sequence number to s.
  void SetLastSequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_ = s;
  }

  // Mark the specified file number as used.
  void MarkFileNumberUsed(uint64_t number);

  // Return the current log file number.
  uint64_t LogNumber() const { return log_number_; }

  // Return the log file number for the log file that is currently
  // being compacted, or zero if there is no such log file.
  uint64_t PrevLogNumber() const { return prev_log_number_; }

  int WriteThrottleUsec(bool active_compaction)
  {
      uint64_t penalty, throttle;
      int ret_val;

      penalty=current_->write_penalty_;
      throttle=GetThrottleWriteRate();

      ret_val=0;
      if (0==penalty && 1!=throttle)
          ret_val=(int)throttle;
      else if (0!=penalty)
      {
          if (1==throttle)
              throttle=GetUnadjustedThrottleWriteRate();
          ret_val=(int)penalty * throttle;
      }   // else if

      return(ret_val);
  }


  // Pick level and inputs for a new compaction.
  // Returns NULL if there is no compaction to be done.
  // Otherwise returns a pointer to a heap-allocated object that
  // describes the compaction.  Caller should delete the result.
  //
  // Riak October 2013:  Pick Compaction now posts work directly
  //  to hot_thread pools
  void PickCompaction(class DBImpl * db_impl);

  // Return a compaction object for compacting the range [begin,end] in
  // the specified level.  Returns NULL if there is nothing in that
  // level that overlaps the specified range.  Caller should delete
  // the result.
  Compaction* CompactRange(
      int level,
      const InternalKey* begin,
      const InternalKey* end);

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t MaxNextLevelOverlappingBytes();

  // Create an iterator that reads over the compaction inputs for "*c".
  // The caller should delete the iterator when no longer needed.
  Iterator* MakeInputIterator(Compaction* c);

  // Returns true iff some level needs a compaction.
  bool NeedsCompaction() const {
    Version* v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != NULL);
  }

  // Add all files listed in any live version to *live.
  // May also mutate some internal state.
  void AddLiveFiles(std::set<uint64_t>* live);

  // Return the approximate offset in the database of the data for
  // "key" as of version "v".
  uint64_t ApproximateOffsetOf(Version* v, const InternalKey& key);

  // Return a human-readable short (single-line) summary of the number
  // of files per level.  Uses *scratch as backing store.
  struct LevelSummaryStorage {
    char buffer[100];
  };
  const char* LevelSummary(LevelSummaryStorage* scratch) const;
  const char* CompactionSummary(LevelSummaryStorage* scratch) const;

  TableCache* GetTableCache() {return(table_cache_);};

  const Options * GetOptions() const {return(options_);};

  bool IsCompactionSubmitted(int level)
  {return(m_CompactionStatus[level].m_Submitted);}

  void SetCompactionSubmitted(int level)
  {m_CompactionStatus[level].m_Submitted=true;}

  void SetCompactionRunning(int level)
  {m_CompactionStatus[level].m_Running=true;}

  void SetCompactionDone(int level, uint64_t Now)
  {   m_CompactionStatus[level].m_Running=false;
      m_CompactionStatus[level].m_Submitted=false;
      // must set both source and destination.  otherwise
      //  destination might immediately decide it needs a
      //  timed grooming too ... defeating idea to spreadout the groomings
      m_CompactionStatus[level].m_LastCompaction=Now;
      if ((level+1)<config::kNumLevels)
          m_CompactionStatus[level+1].m_LastCompaction=Now;
  }

  bool NeighborCompactionsQuiet(int level);

protected:
  class Builder;

  friend class Compaction;
  friend class Version;

  bool Finalize(Version* v);
  void UpdatePenalty(Version *v);

  void GetRange(const std::vector<FileMetaData*>& inputs,
                InternalKey* smallest,
                InternalKey* largest);

  void GetRange2(const std::vector<FileMetaData*>& inputs1,
                 const std::vector<FileMetaData*>& inputs2,
                 InternalKey* smallest,
                 InternalKey* largest);

  void SetupOtherInputs(Compaction* c);

  // Save current contents to *log
  Status WriteSnapshot(log::Writer* log);

  void AppendVersion(Version* v);

  Env* const env_;
  const std::string dbname_;
  const Options* const options_;
  TableCache* const table_cache_;
  const InternalKeyComparator icmp_;
  volatile uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted

  // Opened lazily
  WritableFile* descriptor_file_;
  log::Writer* descriptor_log_;
  Version dummy_versions_;  // Head of circular doubly-linked list of versions.
  Version* current_;        // == dummy_versions_.prev_

  // Per-level key at which the next compaction at that level should start.
  // Either an empty string, or a valid InternalKey.
  std::string compact_pointer_[config::kNumLevels];

  // Riak allows multiple compaction threads, this mutex allows
  //  only one to write to manifest at a time.  Only used in LogAndApply
  port::Mutex manifest_mutex_;

  volatile uint64_t last_penalty_minutes_;
  volatile int prev_write_penalty_;



  struct CompactionStatus_s
  {
      bool m_Submitted;     //!< level submitted to hot thread pool
      bool m_Running;       //!< thread actually running compaction
      uint64_t m_LastCompaction; //!<NowMicros() when last compaction completed

      CompactionStatus_s()
      : m_Submitted(false), m_Running(false), m_LastCompaction(0)
      {};
  } m_CompactionStatus[config::kNumLevels];

private:
  // No copying allowed
  VersionSet(const VersionSet&);
  void operator=(const VersionSet&);
};

//
// allows routing of compaction request to
//  diverse processing routines via common
//  BackgroundCall2 thread entry
//
enum CompactionType
{
    kNormalCompaction = 0x0,
    kExpiryFileCompaction = 0x1
};  // CompactionType


// A Compaction encapsulates information about a compaction.
class Compaction {
 public:
  ~Compaction();

  // Return the level that is being compacted.  Inputs from "level"
  // and "level+1" will be merged to produce a set of "level+1" files.
  int level() const { return level_; }

  // Return parent Version object
  const Version * version() const { return input_version_; }

  // Return the object that holds the edits to the descriptor done
  // by this compaction.
  VersionEdit* edit() { return &edit_; }

  // "which" must be either 0 or 1
  int num_input_files(int which) const { return inputs_[which].size(); }

  // Return the ith input file at "level()+which" ("which" must be 0 or 1).
  FileMetaData* input(int which, int i) const { return inputs_[which][i]; }

  // Maximum size of files to build during this compaction.
  uint64_t MaxOutputFileSize() const { return max_output_file_size_; }

  // Is this a trivial compaction that can be implemented by just
  // moving a single input file to the next level (no merging or splitting)
  bool IsTrivialMove() const;

  // Add all inputs to this compaction as delete operations to *edit.
  void AddInputDeletions(VersionEdit* edit);

  // Returns true if the information we have available guarantees that
  // the compaction is producing data in "level+1" for which no data exists
  // in levels greater than "level+1".
  bool IsBaseLevelForKey(const Slice& user_key);

  // Returns true iff we should stop building the current output
  // before processing "internal_key".
  bool ShouldStopBefore(const Slice& internal_key, size_t key_count);

  // Release the input version for the compaction, once the compaction
  // is successful.
  void ReleaseInputs();

  // Riak specific:  get summary statistics from compaction inputs
  void CalcInputStats(TableCache & tables);
  size_t TotalUserDataSize() const {return(tot_user_data_);};
  size_t TotalIndexKeys()    const {return(tot_index_keys_);};
  size_t AverageValueSize()  const {return(avg_value_size_);};
  size_t AverageKeySize()    const {return(avg_key_size_);};
  size_t AverageBlockSize()  const {return(avg_block_size_);};
  bool IsCompressible()      const {return(compressible_);};

  // Riak specific:  is move operation ok for compaction?
  bool IsMoveOk()            const {return(!no_move_);};

  enum CompactionType GetCompactionType() const {return(compaction_type_);};

 private:
  friend class Version;
  friend class VersionSet;

  explicit Compaction(int level);

  int level_;
  uint64_t max_output_file_size_;
  Version* input_version_;
  VersionEdit edit_;
  CompactionType compaction_type_;

  // Each compaction reads inputs from "level_" and "level_+1"
  std::vector<FileMetaData*> inputs_[2];      // The two sets of inputs

  // State used to check for number of of overlapping grandparent files
  // (parent == level_ + 1, grandparent == level_ + 2)
  std::vector<FileMetaData*> grandparents_;
  size_t grandparent_index_;  // Index in grandparent_starts_
  bool seen_key_;             // Some output key has been seen
  uint64_t overlapped_bytes_;  // Bytes of overlap between current output
                              // and grandparent files

  // State for implementing IsBaseLevelForKey

  // level_ptrs_ holds indices into input_version_->levels_: our state
  // is that we are positioned at one of the file ranges for each
  // higher level than the ones involved in this compaction (i.e. for
  // all L >= level_ + 2).
  size_t level_ptrs_[config::kNumLevels];

  // Riak specific:  output statistics from CalcInputStats
  size_t tot_user_data_;
  size_t tot_index_keys_;
  size_t avg_value_size_;
  size_t avg_key_size_;
  size_t avg_block_size_;
  bool compressible_;
  bool stats_done_;
  bool no_move_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_SET_H_
