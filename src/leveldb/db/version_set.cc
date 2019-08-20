// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/version_set.h"

#include <algorithm>
#include <stdio.h>
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "leveldb/env.h"
#include "leveldb/expiry.h"
#include "leveldb/table_builder.h"
#include "table/block.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/db_list.h"
#include "util/hot_threads.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/thread_tasks.h"
#include "leveldb/perf_count.h"

namespace leveldb {

// branch mv-level-work1, March 2013
//
// Notes:
//
static struct
{
    uint64_t m_TargetFileSize;                   //!< mostly useless
    uint64_t m_MaxGrandParentOverlapBytes;       //!< needs tuning, but not essential
                                                 //!<   since moves eliminated
    int64_t  m_ExpandedCompactionByteSizeLimit;  //!< needs tuning

    // next two ignored if m_OverlappedFiles is true
    uint64_t m_MaxBytesForLevel;                 //!< start write throttle above this
    uint64_t m_DesiredBytesForLevel;             //!< compact into next level until this

    uint64_t m_MaxFileSizeForLevel;              //!< google really applies this
                                                 //!<   to file size of NEXT level
    bool m_OverlappedFiles;                      //!< false means sst files are sorted
                                                 //!<   and do not overlap
} gLevelTraits[config::kNumLevels]=

// level-0 and level-1 create .sst table files that have overlapping key spaces.
//   The compaction selection logic within VersionSet::Finalize() selects based
//   upon file count, not accumulated file size.  Write throttle is harsh if too
//   many files accumulate.  Timed grooming (if activated) adjusts the file
//   count threshold by time since last compaction.
// level-2 is the "landing zone" / first sorted level.  Try to keep it clear,
//   hence the low m_DesiredBytes for level.
// level-2+:  VersionSet::Finalize() selects compaction files when the
//   total bytes for level exceeds m_DesiredBytesForLevel.  Write throttle
//   starts when total bytes exceeds m_MaxFileSizeForLevel.

// WARNING: m_OverlappedFiles flags need to match config::kNumOverlapFiles ... until unified
{
    {10485760,  262144000,  57671680,      209715200,                 0,     420000000, true},
    {10485760,   82914560,  57671680,      419430400,                 0,     209715200, true},
    {10485760,  314572800,  57671680,     3082813440,         200000000,     314572800, false},
    {10485760,  419430400,  57671680,     6442450944ULL,     4294967296ULL,  419430400, false},
    {10485760,  524288000,  57671680,   128849018880ULL,    85899345920ULL,  524288000, false},
    {10485760,  629145600,  57671680,  2576980377600ULL,  1717986918400ULL,  629145600, false},
    {10485760,  734003200,  57671680, 51539607552000ULL, 34359738368000ULL,  734003200, false}
};

/// ULL above needed to compile on OSX 10.7.3

static int64_t TotalFileSize(const std::vector<FileMetaData*>& files) {
  int64_t sum = 0;
  for (size_t i = 0; i < files.size(); i++) {
    sum += files[i]->file_size;
  }
  return sum;
}

Version::~Version() {
  assert(refs_ == 0);

  // Remove from linked list
  prev_->next_ = next_;
  next_->prev_ = prev_;

  // Drop references to files
  for (int level = 0; level < config::kNumLevels; level++) {
    for (size_t i = 0; i < files_[level].size(); i++) {
      FileMetaData* f = files_[level][i];
      assert(f->refs > 0);
      f->refs--;

      if (f->refs <= 0) {
        // clear Riak's double reference of overlapped files
        if (vset_->IsLevelOverlapped(level))
          vset_->GetTableCache()->Evict(f->number, true);

        delete f;
      }
    }
  }
}

int FindFile(const InternalKeyComparator& icmp,
             const std::vector<FileMetaData*>& files,
             const Slice& key) {
  uint32_t left = 0;
  uint32_t right = files.size();
  while (left < right) {
    uint32_t mid = (left + right) / 2;
    const FileMetaData* f = files[mid];
    if (icmp.InternalKeyComparator::Compare(f->largest.Encode(), key) < 0) {
      // Key at "mid.largest" is < "target".  Therefore all
      // files at or before "mid" are uninteresting.
      left = mid + 1;
    } else {
      // Key at "mid.largest" is >= "target".  Therefore all files
      // after "mid" are uninteresting.
      right = mid;
    }
  }
  return right;
}

static bool AfterFile(const Comparator* ucmp,
                      const Slice* user_key, const FileMetaData* f) {
  // NULL user_key occurs before all keys and is therefore never after *f
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->largest.user_key()) > 0);
}

static bool BeforeFile(const Comparator* ucmp,
                       const Slice* user_key, const FileMetaData* f) {
  // NULL user_key occurs after all keys and is therefore never before *f
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->smallest.user_key()) < 0);
}

bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key) {
  const Comparator* ucmp = icmp.user_comparator();
  if (!disjoint_sorted_files) {
    // Need to check against all files
    for (size_t i = 0; i < files.size(); i++) {
      const FileMetaData* f = files[i];
      if (AfterFile(ucmp, smallest_user_key, f) ||
          BeforeFile(ucmp, largest_user_key, f)) {
        // No overlap
      } else {
        return true;  // Overlap
      }
    }
    return false;
  }

  // Binary search over file list
  uint32_t index = 0;
  if (smallest_user_key != NULL) {
    // Find the earliest possible internal key for smallest_user_key
    InternalKey small(*smallest_user_key, 0, kMaxSequenceNumber, kValueTypeForSeek);
    index = FindFile(icmp, files, small.Encode());
  }

  if (index >= files.size()) {
    // beginning of range is after all files, so no overlap.
    return false;
  }

  return !BeforeFile(ucmp, largest_user_key, files[index]);
}

// An internal iterator.  For a given version/level pair, yields
// information about the files in the level.  For a given entry, key()
// is the largest key that occurs in the file, and value() is an
// 16-byte value containing the file number and file size, both
// encoded using EncodeFixed64.
class Version::LevelFileNumIterator : public Iterator {
 public:
  LevelFileNumIterator(const InternalKeyComparator& icmp,
                       const std::vector<FileMetaData*>* flist)
      : icmp_(icmp),
        flist_(flist),
        index_(flist->size()) {        // Marks as invalid
  }
  virtual bool Valid() const {
    return index_ < flist_->size();
  }
  virtual void Seek(const Slice& target) {
    index_ = FindFile(icmp_, *flist_, target);
  }
  virtual void SeekToFirst() { index_ = 0; }
  virtual void SeekToLast() {
    index_ = flist_->empty() ? 0 : flist_->size() - 1;
  }
  virtual void Next() {
    assert(Valid());
    index_++;
  }
  virtual void Prev() {
    assert(Valid());
    if (index_ == 0) {
      index_ = flist_->size();  // Marks as invalid
    } else {
      index_--;
    }
  }
  Slice key() const {
    assert(Valid());
    return (*flist_)[index_]->largest.Encode();
  }
  Slice value() const {
    assert(Valid());
    EncodeFixed64(value_buf_, (*flist_)[index_]->number);
    EncodeFixed64(value_buf_+8, (*flist_)[index_]->file_size);
    EncodeFixed32(value_buf_+16, (*flist_)[index_]->level);
    return Slice(value_buf_, sizeof(value_buf_));
  }
  virtual Status status() const { return Status::OK(); }
 private:
  const InternalKeyComparator icmp_;
  const std::vector<FileMetaData*>* const flist_;
  uint32_t index_;

  // Backing store for value().  Holds the file number and size (and level).
  mutable char value_buf_[20];
};

static Iterator* GetFileIterator(void* arg,
                                 const ReadOptions& options,
                                 const Slice& file_value) {
  TableCache* cache = reinterpret_cast<TableCache*>(arg);
  if (file_value.size() != 20) {
    return NewErrorIterator(
        Status::Corruption("FileReader invoked with unexpected value"));
  } else {
    return cache->NewIterator(options,
                              DecodeFixed64(file_value.data()),
                              DecodeFixed64(file_value.data() + 8),
                              DecodeFixed32(file_value.data() + 16));
  }
}

Iterator* Version::NewConcatenatingIterator(const ReadOptions& options,
                                            int level) const {
  return NewTwoLevelIterator(
      new LevelFileNumIterator(vset_->icmp_, &files_[level]),
      &GetFileIterator, vset_->table_cache_, options);
}

void Version::AddIterators(const ReadOptions& options,
                           std::vector<Iterator*>* iters) {

    int level;

    for (level=0; level < config::kNumLevels; ++level)
    {
        if (gLevelTraits[level].m_OverlappedFiles)
        {
            // Merge all level files together since they may overlap
            for (size_t i = 0; i < files_[level].size(); i++)
            {
                iters->push_back(
                    vset_->table_cache_->NewIterator(
                        options, files_[level][i]->number, files_[level][i]->file_size, level));
            }   // for
        }   // if

        else
        {
            // For sorted levels, we can use a concatenating iterator that sequentially
            // walks through the non-overlapping files in the level, opening them
            // lazily.
            if (!files_[level].empty())
            {
                iters->push_back(NewConcatenatingIterator(options, level));
            }   // if
        }   // else
    }   // for
}   // Version::NewConcatenatingIterator


// Callback from TableCache::Get()
namespace {
enum SaverState {
  kNotFound,
  kFound,
  kDeleted,
  kCorrupt,
};
struct Saver {
  SaverState state;
  const Comparator* ucmp;
  const Options* options;
  Slice user_key;
  Value* value;
  const LookupKey * lookup;
};
}
static bool SaveValue(void* arg, const Slice& ikey, const Slice& v) {
  bool match=false;
  bool expired=false;
  Saver* s = reinterpret_cast<Saver*>(arg);
  ParsedInternalKey parsed_key;
  if (!ParseInternalKey(ikey, &parsed_key)) {
    s->state = kCorrupt;
  } else {
    if (s->ucmp->Compare(parsed_key.user_key, s->user_key) == 0) {
      match=true;
      if (NULL!=s->options && s->options->ExpiryActivated())
        expired=s->options->expiry_module->KeyRetirementCallback(parsed_key);
      s->state = (parsed_key.type != kTypeDeletion && !expired) ? kFound : kDeleted;
      if (s->state == kFound) {
        s->value->assign(v.data(), v.size());
      }
      if (NULL!=s->lookup)
        s->lookup->SetKeyMetaData(parsed_key);
    }
  }
  return(match);
}

static bool NewestFirst(FileMetaData* a, FileMetaData* b) {
  return a->number > b->number;
}

Status Version::Get(const ReadOptions& options,
                    const LookupKey& k,
                    Value* value,
                    GetStats* stats) {
  Slice ikey = k.internal_key();
  Slice user_key = k.user_key();
  const Comparator* ucmp = vset_->icmp_.user_comparator();
  Status s;

  stats->seek_file = NULL;
  stats->seek_file_level = -1;
  FileMetaData* last_file_read = NULL;
  int last_file_read_level = -1;

  // We can search level-by-level since entries never hop across
  // levels.  Therefore we are guaranteed that if we find data
  // in an smaller level, later levels are irrelevant.
  std::vector<FileMetaData*> tmp;
  FileMetaData* tmp2;
  for (int level = 0; level < config::kNumLevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

    // Get the list of files to search in this level
    FileMetaData* const* files = &files_[level][0];
    if (gLevelTraits[level].m_OverlappedFiles) {
      // Level files may overlap each other.  Find all files that
      // overlap user_key and process them in order from newest to oldest.
      tmp.reserve(num_files);
      for (uint32_t i = 0; i < num_files; i++) {
        FileMetaData* f = files[i];
        if (ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
            ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
          tmp.push_back(f);
        }
      }
      if (tmp.empty()) continue;

      std::sort(tmp.begin(), tmp.end(), NewestFirst);
      files = &tmp[0];
      num_files = tmp.size();
    } else {
      // Binary search to find earliest index whose largest key >= ikey.
      uint32_t index = FindFile(vset_->icmp_, files_[level], ikey);
      if (index >= num_files) {
        files = NULL;
        num_files = 0;
      } else {
        tmp2 = files[index];
        if (ucmp->Compare(user_key, tmp2->smallest.user_key()) < 0) {
          // All of "tmp2" is past any data for user_key
          files = NULL;
          num_files = 0;
        } else {
          files = &tmp2;
          num_files = 1;
        }
      }
    }

    if (0!=num_files)
        gPerfCounters->Add(ePerfSearchLevel0 + level, num_files);

    for (uint32_t i = 0; i < num_files; ++i) {
      if (last_file_read != NULL && stats->seek_file == NULL) {
        // We have had more than one seek for this read.  Charge the 1st file.
        stats->seek_file = last_file_read;
        stats->seek_file_level = last_file_read_level;
      }

      FileMetaData* f = files[i];
      last_file_read = f;
      last_file_read_level = level;

      Saver saver;
      saver.state = kNotFound;
      saver.ucmp = ucmp;
      saver.options = vset_->options_;
      saver.user_key = user_key;
      saver.value = value;
      saver.lookup = &k;
      s = vset_->table_cache_->Get(options, f->number, f->file_size, level,
                                   ikey, &saver, SaveValue);
      if (!s.ok()) {
        return s;
      }
      switch (saver.state) {
        case kNotFound:
          break;      // Keep searching in other files
        case kFound:
          return s;
        case kDeleted:
          s = Status::NotFound(Slice());  // Use empty error message for speed
          return s;
        case kCorrupt:
          s = Status::Corruption("corrupted key for ", user_key);
          return s;
      }
    }
  }

  return Status::NotFound(Slice());  // Use an empty error message for speed
}

bool Version::UpdateStats(const GetStats& stats) {
#if 0
  FileMetaData* f = stats.seek_file;
  if (f != NULL) {
    f->allowed_seeks--;
    if (f->allowed_seeks <= 0 && file_to_compact_ == NULL) {
      file_to_compact_ = f;
      file_to_compact_level_ = stats.seek_file_level;
      return true;
    }
  }
#endif
  return false;
}

void Version::Ref() {
  ++refs_;
}

void Version::Unref() {
  assert(this != &vset_->dummy_versions_);
  assert(refs_ >= 1);
  --refs_;
  if (refs_ == 0) {
    delete this;
  }
}

bool Version::OverlapInLevel(int level,
                             const Slice* smallest_user_key,
                             const Slice* largest_user_key) const {
  return SomeFileOverlapsRange(vset_->icmp_,
                               !gLevelTraits[level].m_OverlappedFiles,
                               files_[level],
                               smallest_user_key, largest_user_key);
}

int Version::PickLevelForMemTableOutput(
    const Slice& smallest_user_key,
    const Slice& largest_user_key,
    const int level_limit) {
  int level = 0;

// test if level 1 m_OverlappedFiles is false, proceded only then
  if (!OverlapInLevel(0, &smallest_user_key, &largest_user_key)) {
    // Push to next level if there is no overlap in next level,
    // and the #bytes overlapping in the level after that are limited.
    InternalKey start(smallest_user_key, 0, kMaxSequenceNumber, kValueTypeForSeek);
    InternalKey limit(largest_user_key, 0, 0, static_cast<ValueType>(0));
    std::vector<FileMetaData*> overlaps;
    while (level < level_limit) {
      if (OverlapInLevel(level + 1, &smallest_user_key, &largest_user_key)) {
        break;
      }
      GetOverlappingInputs(level + 2, &start, &limit, &overlaps);
      const uint64_t sum = TotalFileSize(overlaps);
      if (sum > gLevelTraits[level].m_MaxGrandParentOverlapBytes) {
        break;
      }
      level++;
    }
    // do not waste a move into an overlapped level, breaks
    //  different performance improvement
    if (gLevelTraits[level].m_OverlappedFiles)
        level=0;
  }

  return level;
}

// Store in "*inputs" all files in "level" that overlap [begin,end]
void Version::GetOverlappingInputs(
    int level,
    const InternalKey* begin,
    const InternalKey* end,
    std::vector<FileMetaData*>* inputs) {
  inputs->clear();
  Slice user_begin, user_end;

  // overlap takes everything
  bool test_inputs(!gLevelTraits[level].m_OverlappedFiles);

  if (begin != NULL) {
      user_begin = begin->user_key();
  }
  if (end != NULL) {
      user_end = end->user_key();
  }

  const Comparator* user_cmp = vset_->icmp_.user_comparator();
  for (size_t i = 0; i < files_[level].size(); ) {
    FileMetaData* f = files_[level][i++];
    const Slice file_start = f->smallest.user_key();
    const Slice file_limit = f->largest.user_key();
    if (test_inputs && begin != NULL && user_cmp->Compare(file_limit, user_begin) < 0) {
      // "f" is completely before specified range; skip it
    } else if (test_inputs && end != NULL && user_cmp->Compare(file_start, user_end) > 0) {
      // "f" is completely after specified range; skip it
    } else {
      inputs->push_back(f);
    }
  }
}


bool
Version::VerifyLevels(
    int & level,           // input / output for current level to inspect
    InternalKey & begin,   // output of lowest key in first overlapped file
    InternalKey & end)     // output of highest key in first overlapped file
{
    bool overlap_found;
    const Comparator* user_cmp;

    overlap_found=false;
    user_cmp = vset_->icmp_.user_comparator();

    do
    {
        // test only levels that do not expect overlapped .sst files
        if (!gLevelTraits[level].m_OverlappedFiles && 1<files_[level].size())
        {
            const std::vector<FileMetaData*>& files = files_[level];
            size_t inner, outer;

            for (outer=0; outer<files.size()-1 && !overlap_found; ++outer)
            {
                FileMetaData* outer_meta = files_[level][outer];
                const Slice outer_limit = outer_meta->largest.user_key();

                for (inner=outer+1; inner<files.size() && !overlap_found; ++inner)
                {
                    FileMetaData* inner_meta = files_[level][inner];
                    const Slice inner_start = inner_meta->smallest.user_key();

                    // do files overlap? assumes vector sorted by "start"
                    if (user_cmp->Compare(inner_start, outer_limit) <= 0)
                    {
                        overlap_found=true;
                        begin=outer_meta->smallest;
                        end=outer_meta->largest;
                    }   // if
                }   // for
            }   // for
        }   // if

        // current level is clean, move to next
        if (!overlap_found)
            ++level;

        // stopping before the last level.  that needs much
        //  more support code ... later project
    } while(!overlap_found && (level+1)<config::kNumLevels);

    return(overlap_found);

}   // VersionSet::VerifyLevels


std::string Version::DebugString() const {
  std::string r;
  for (int level = 0; level < config::kNumLevels; level++) {
    // E.g.,
    //   --- level 1 ---
    //   17:123['a' .. 'd']
    //   20:43['e' .. 'g']
    r.append("--- level ");
    AppendNumberTo(&r, level);
    r.append(" ---\n");
    const std::vector<FileMetaData*>& files = files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      r.push_back(' ');
      AppendNumberTo(&r, files[i]->number);
      r.push_back(':');
      AppendNumberTo(&r, files[i]->file_size);
      r.append("[");
      r.append(files[i]->smallest.DebugString());
      r.append(" .. ");
      r.append(files[i]->largest.DebugString());
      r.append("]\n");
    }
  }
  return r;
}

// A helper class so we can efficiently apply a whole sequence
// of edits to a particular state without creating intermediate
// Versions that contain full copies of the intermediate state.
class VersionSet::Builder {
 private:
  // Helper to sort by v->files_[file_number].smallest
  struct BySmallestKey {
    const InternalKeyComparator* internal_comparator;

    bool operator()(FileMetaData* f1, FileMetaData* f2) const {
      int r = internal_comparator->Compare(f1->smallest, f2->smallest);
      if (r != 0) {
        return (r < 0);
      } else {
        // Break ties by file number
        return (f1->number < f2->number);
      }
    }
  };

  typedef std::set<FileMetaData*, BySmallestKey> FileSet;
  struct LevelState {
    std::set<uint64_t> deleted_files;
    FileSet* added_files;
  };

  VersionSet* vset_;
  Version* base_;
  LevelState levels_[config::kNumLevels];

 public:
  // Initialize a builder with the files from *base and other info from *vset
  Builder(VersionSet* vset, Version* base)
      : vset_(vset),
        base_(base) {
    base_->Ref();
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      levels_[level].added_files = new FileSet(cmp);
    }
  }

  ~Builder() {
    for (int level = 0; level < config::kNumLevels; level++) {
      const FileSet* added = levels_[level].added_files;
      std::vector<FileMetaData*> to_unref;
      to_unref.reserve(added->size());
      for (FileSet::const_iterator it = added->begin();
          it != added->end(); ++it) {
        to_unref.push_back(*it);
      }
      delete added;
      for (uint32_t i = 0; i < to_unref.size(); i++) {
        FileMetaData* f = to_unref[i];
        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
    }
    base_->Unref();
  }

  // Apply all of the edits in *edit to the current state.
  void Apply(VersionEdit* edit) {
    // Update compaction pointers
    for (size_t i = 0; i < edit->compact_pointers_.size(); i++) {
      const int level = edit->compact_pointers_[i].first;
      vset_->compact_pointer_[level] =
          edit->compact_pointers_[i].second.Encode().ToString();
    }

    // Delete files
    const VersionEdit::DeletedFileSet& del = edit->deleted_files_;
    for (VersionEdit::DeletedFileSet::const_iterator iter = del.begin();
         iter != del.end();
         ++iter) {
      const int level = iter->first;
      const uint64_t number = iter->second;
      levels_[level].deleted_files.insert(number);
    }

    // Add new files
    for (size_t i = 0; i < edit->new_files_.size(); i++) {
      const int level = edit->new_files_[i].first;
      FileMetaData* f = new FileMetaData(edit->new_files_[i].second);
      f->refs = 1;

#if 0
      // We arrange to automatically compact this file after
      // a certain number of seeks.  Let's assume:
      //   (1) One seek costs 10ms
      //   (2) Writing or reading 1MB costs 10ms (100MB/s)
      //   (3) A compaction of 1MB does 25MB of IO:
      //         1MB read from this level
      //         10-12MB read from next level (boundaries may be misaligned)
      //         10-12MB written to next level
      // This implies that 25 seeks cost the same as the compaction
      // of 1MB of data.  I.e., one seek costs approximately the
      // same as the compaction of 40KB of data.  We are a little
      // conservative and allow approximately one seek for every 16KB
      // of data before triggering a compaction.
      f->allowed_seeks = (f->file_size / 16384);
      if (f->allowed_seeks < 100) f->allowed_seeks = 100;
#endif

      levels_[level].deleted_files.erase(f->number);
      levels_[level].added_files->insert(f);
    }
  }

  // Save the current state in *v.
  void SaveTo(Version* v) {
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      // Merge the set of added files with the set of pre-existing files.
      // Drop any deleted files.  Store the result in *v.
      const std::vector<FileMetaData*>& base_files = base_->files_[level];
      std::vector<FileMetaData*>::const_iterator base_iter = base_files.begin();
      std::vector<FileMetaData*>::const_iterator base_end = base_files.end();
      const FileSet* added = levels_[level].added_files;
      v->files_[level].reserve(base_files.size() + added->size());
      for (FileSet::const_iterator added_iter = added->begin();
           added_iter != added->end();
           ++added_iter) {
        // Add all smaller files listed in base_
        for (std::vector<FileMetaData*>::const_iterator bpos
                 = std::upper_bound(base_iter, base_end, *added_iter, cmp);
             base_iter != bpos;
             ++base_iter) {
          MaybeAddFile(v, level, *base_iter);
        }

        MaybeAddFile(v, level, *added_iter);
      }

      // Add remaining base files
      for (; base_iter != base_end; ++base_iter) {
        MaybeAddFile(v, level, *base_iter);
      }

#ifndef NDEBUG
      // Make sure there is no overlap in levels > 0
      if (!gLevelTraits[level].m_OverlappedFiles) {
        for (uint32_t i = 1; i < v->files_[level].size(); i++) {
          const InternalKey& prev_end = v->files_[level][i-1]->largest;
          const InternalKey& this_begin = v->files_[level][i]->smallest;
          if (vset_->icmp_.Compare(prev_end, this_begin) >= 0
              && !vset_->options_->is_repair) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    prev_end.DebugString().c_str(),
                    this_begin.DebugString().c_str());
            abort();
          }
        }
      }
#endif
    }
  }

  void MaybeAddFile(Version* v, int level, FileMetaData* f) {
    if (levels_[level].deleted_files.count(f->number) > 0) {
      // File is deleted: do nothing
    } else {
      std::vector<FileMetaData*>* files = &v->files_[level];
      if (!gLevelTraits[level].m_OverlappedFiles && !files->empty()
          && !vset_->options_->is_repair) {
        // Must not overlap
        assert(vset_->icmp_.Compare((*files)[files->size()-1]->largest,
                                    f->smallest) < 0);
      }
      f->refs++;
      files->push_back(f);
    }
  }
};

VersionSet::VersionSet(const std::string& dbname,
                       const Options* options,
                       TableCache* table_cache,
                       const InternalKeyComparator* cmp)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      next_file_number_(2),
      manifest_file_number_(0),  // Filled by Recover()
      last_sequence_(0),
      log_number_(0),
      prev_log_number_(0),
      descriptor_file_(NULL),
      descriptor_log_(NULL),
      dummy_versions_(this),
      current_(NULL),
      last_penalty_minutes_(0),
      prev_write_penalty_(0)
{
  AppendVersion(new Version(this));
}

VersionSet::~VersionSet() {
  // must remove second ref counter that keeps overlapped files locked
  //  table cache

  current_->Unref();
  assert(dummy_versions_.next_ == &dummy_versions_);  // List must be empty
  delete descriptor_log_;
  delete descriptor_file_;
}

void VersionSet::AppendVersion(Version* v) {
  // Make "v" current
  assert(v->refs_ == 0);
  assert(v != current_);
  if (current_ != NULL) {
    current_->Unref();
  }
  current_ = v;
  v->Ref();

  // Append to linked list
  v->prev_ = dummy_versions_.prev_;
  v->next_ = &dummy_versions_;
  v->prev_->next_ = v;
  v->next_->prev_ = v;
}

Status VersionSet::LogAndApply(VersionEdit* edit, port::Mutex* mu) {
  if (edit->has_log_number_) {
    assert(edit->log_number_ >= log_number_);
    assert(edit->log_number_ < next_file_number_);
  } else {
    edit->SetLogNumber(log_number_);
  }

  if (!edit->has_prev_log_number_) {
    edit->SetPrevLogNumber(prev_log_number_);
  }

  edit->SetNextFile(next_file_number_);
  edit->SetLastSequence(last_sequence_);

  Version* v = new Version(this);
  {
    Builder builder(this, current_);
    builder.Apply(edit);
    builder.SaveTo(v);
  }

  // Initialize new descriptor log file if necessary by creating
  // a temporary file that contains a snapshot of the current version.
  std::string new_manifest_file;
  Status s;
  if (descriptor_log_ == NULL) {
    // No reason to unlock *mu here since we only hit this path in the
    // first call to LogAndApply (when opening the database).
    assert(descriptor_file_ == NULL);
    new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
    edit->SetNextFile(next_file_number_);
    s = env_->NewWritableFile(new_manifest_file, &descriptor_file_, 4*1024L);
    if (s.ok()) {
      descriptor_log_ = new log::Writer(descriptor_file_);
      s = WriteSnapshot(descriptor_log_);
    }
  }

  // Install the new version
  //  matthewv Oct 2013 - this used to be after the MANIFEST write
  //  but overlapping compactions allow for a file to get lost
  //  if first does not post to version completely.
  if (s.ok()) {
    AppendVersion(v);
    log_number_ = edit->log_number_;
    prev_log_number_ = edit->prev_log_number_;

    // Unlock during expensive MANIFEST log write
    {
        mu->Unlock();

        // but only one writer at a time
        {
            MutexLock lock(&manifest_mutex_);
            // Write new record to MANIFEST log
            if (s.ok()) {
                std::string record;
                edit->EncodeTo(&record, options_->ExpiryActivated());
                s = descriptor_log_->AddRecord(record);
                if (s.ok()) {
                    s = descriptor_file_->Sync();
                }
            }

            // If we just created a new descriptor file, install it by writing a
            // new CURRENT file that points to it.
            if (s.ok() && !new_manifest_file.empty()) {
                s = SetCurrentFile(env_, dbname_, manifest_file_number_);
            }
        }   // manifest_lock_

        mu->Lock();
    }
  }

  // this used to be "else" clause to if(s.ok)
  //  moved on Oct 2013
  else
  {
    delete v;
    if (!new_manifest_file.empty()) {
      delete descriptor_log_;
      delete descriptor_file_;
      descriptor_log_ = NULL;
      descriptor_file_ = NULL;
      env_->DeleteFile(new_manifest_file);
    }
  }

  return s;
}

Status VersionSet::Recover() {
  struct LogReporter : public log::Reader::Reporter {
    Status* status;
    virtual void Corruption(size_t bytes, const Status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  // Read "CURRENT" file, which contains a pointer to the current manifest file
  std::string current;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_), &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return Status::Corruption("CURRENT file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname_ + "/" + current;
  SequentialFile* file;
  s = env_->NewSequentialFile(dscname, &file);
  if (!s.ok()) {
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  Builder builder(this, current_);

  {
    LogReporter reporter;
    reporter.status = &s;
    log::Reader reader(file, &reporter, true/*checksum*/, 0/*initial_offset*/);
    Slice record;
    std::string scratch;
    while (reader.ReadRecord(&record, &scratch) && s.ok()) {
      VersionEdit edit;
      s = edit.DecodeFrom(record);
      if (s.ok()) {
        if (edit.has_comparator_ &&
            edit.comparator_ != icmp_.user_comparator()->Name()) {
          s = Status::InvalidArgument(
              edit.comparator_ + "does not match existing comparator ",
              icmp_.user_comparator()->Name());
        }
      }

      if (s.ok()) {
        builder.Apply(&edit);
      }

      if (edit.has_log_number_) {
        log_number = edit.log_number_;
        have_log_number = true;
      }

      if (edit.has_prev_log_number_) {
        prev_log_number = edit.prev_log_number_;
        have_prev_log_number = true;
      }

      if (edit.has_next_file_number_) {
        next_file = edit.next_file_number_;
        have_next_file = true;
      }

      if (edit.has_last_sequence_) {
        last_sequence = edit.last_sequence_;
        have_last_sequence = true;
      }
    }
  }
  delete file;
  file = NULL;

  if (s.ok()) {
    if (!have_next_file) {
      s = Status::Corruption("no meta-nextfile entry in descriptor");
    } else if (!have_log_number) {
      s = Status::Corruption("no meta-lognumber entry in descriptor");
    } else if (!have_last_sequence) {
      s = Status::Corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }

    MarkFileNumberUsed(prev_log_number);
    MarkFileNumberUsed(log_number);
  }

  if (s.ok()) {
    Version* v = new Version(this);
    builder.SaveTo(v);
    // Install recovered version
    AppendVersion(v);
    manifest_file_number_ = next_file;
    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    log_number_ = log_number;
    prev_log_number_ = prev_log_number;
  }

  return s;
}

void VersionSet::MarkFileNumberUsed(uint64_t number) {
  if (next_file_number_ <= number) {
    next_file_number_ = number + 1;
  }
}


bool
VersionSet::NeighborCompactionsQuiet(int level)
{
    uint64_t parent_level_bytes(0);

    if (level < config::kNumLevels-1)
        parent_level_bytes = TotalFileSize(current_->files_[level+1]);

    // not an overlapped level and must not have compactions
    //   scheduled on either level below or level above
    return((0==level || !m_CompactionStatus[level-1].m_Submitted)
           && !gLevelTraits[level].m_OverlappedFiles
           && (level==config::kNumLevels-1
               || (!m_CompactionStatus[level+1].m_Submitted
                   && parent_level_bytes<=((gLevelTraits[level+1].m_MaxBytesForLevel
                                            +gLevelTraits[level+1].m_DesiredBytesForLevel)/2))));
}   // VersionSet::NeighborCompactionsQuiet


bool
VersionSet::Finalize(Version* v)
{
    // Riak:  looking for first compaction needed in level order
    int best_level = -1;
    double best_score = -1;
    bool compaction_found;
    bool is_grooming, no_move, expire_file;
    uint64_t micros_now;

    compaction_found=false;
    is_grooming=false;
    no_move=false;
    expire_file=false;
    micros_now=env_->NowMicros();

    // Note: level kNumLevels-1 only examined for whole file expiry
    for (int level = v->compaction_level_+1; level < config::kNumLevels && !compaction_found; ++level)
    {
        bool compact_ok;
        double score(0);
        uint64_t parent_level_bytes(0);

        is_grooming=false;
        // is this level eligible for compaction consideration?
        compact_ok=!m_CompactionStatus[level].m_Submitted;

        // not already scheduled for compaction
        if (compact_ok)
        {
            if (level < (config::kNumLevels-1))
                parent_level_bytes = TotalFileSize(v->files_[level+1]);

            // is overlapped and so is next level
            if (gLevelTraits[level].m_OverlappedFiles && gLevelTraits[level+1].m_OverlappedFiles)
            {
                // good ... stop consideration
            }   // if

            // overlapped and next level is not compacting
            else if (gLevelTraits[level].m_OverlappedFiles && !m_CompactionStatus[level+1].m_Submitted
                     && (parent_level_bytes<=gLevelTraits[level+1].m_DesiredBytesForLevel
                         || config::kL0_CompactionTrigger <= v->files_[level].size()))
            {
                // good ... stop consideration
            }   // else if

            else
            {
                // must not have compactions scheduled on neither level below nor level above
                compact_ok=NeighborCompactionsQuiet(level);
            }   // else
        }   // if

        // consider this level
        if (compact_ok)
        {
            size_t grooming_trigger;
            uint64_t elapsed_micros;

            // some platforms use gettimeofday() which can move backward
            if ( m_CompactionStatus[level].m_LastCompaction < micros_now
                 && 0 != m_CompactionStatus[level].m_LastCompaction)
                elapsed_micros=micros_now - m_CompactionStatus[level].m_LastCompaction;
            else
                elapsed_micros=0;

            // reevaluating timed grooming ... seems to crush caching
            //  this disables the code but leaves it in place for future
            //  reuse after block cache flushing impact addressed
            elapsed_micros=0;

            // which grooming trigger point?  based upon how long
            //  since last compaction on this level
            //   - less than 10 minutes?
            if (elapsed_micros < config::kL0_Grooming10minMicros)
                grooming_trigger=config::kL0_GroomingTrigger;

            //   - less than 20 minutes?
            else if (elapsed_micros < config::kL0_Grooming20minMicros)
                grooming_trigger=config::kL0_GroomingTrigger10min;

            //   - more than 20 minutes
            else
                grooming_trigger=config::kL0_GroomingTrigger20min;

            if (gLevelTraits[level].m_OverlappedFiles) {
                // We treat level-0 specially by bounding the number of files
                // instead of number of bytes for two reasons:
                //
                // (1) With larger write-buffer sizes, it is nice not to do too
                // many level-0 compactions.
                //
                // (2) The files in level-0 are merged on every read and
                // therefore we wish to avoid too many files when the individual
                // file size is small (perhaps because of a small write-buffer
                // setting, or very high compression ratios, or lots of
                // overwrites/deletions).
                score=0;

                // score of 1 at compaction trigger, incrementing for each thereafter
                if ( config::kL0_CompactionTrigger <= v->files_[level].size())
                    score += v->files_[level].size() - config::kL0_CompactionTrigger +1;

                is_grooming=false;

                // early overlapped compaction
                //  only occurs if no other compactions running on groomer thread
                //  (no grooming if landing level is still overloaded)
                if (0==score && grooming_trigger<=v->files_[level].size()
                    && 2<DBList()->GetDBCount(false)   // for non-Riak use cases, helps throughput
                    && (uint64_t)TotalFileSize(v->files_[config::kNumOverlapLevels])
		    < gLevelTraits[config::kNumOverlapLevels].m_DesiredBytesForLevel)
                {
                    // secondary test, don't push too much to next Overlap too soon
                    if (!gLevelTraits[level+1].m_OverlappedFiles
                         || v->files_[level+1].size()<=config::kL0_CompactionTrigger)
                    {
                        score=1;
                        is_grooming=true;
                    }   // if
                }   // if
            }   // if

            // highest level, kNumLevels-1, only considered for expiry not compaction
            else if (level < config::kNumLevels-1) {
                // Compute the ratio of current size to size limit.
                const uint64_t level_bytes = TotalFileSize(v->files_[level]);
                score = static_cast<double>(level_bytes) / gLevelTraits[level].m_DesiredBytesForLevel;
                is_grooming=(level_bytes < gLevelTraits[level].m_MaxFileSizeForLevel);

                // force landing level to not be grooming ... ever
                if (gLevelTraits[level-1].m_OverlappedFiles)
                    is_grooming=false;

                // within size constraints, are there any deletes worthy of consideration
                //  (must not do this on overlapped levels.  causes huge throughput problems
                //   on heavy loads)
                if (score < 1 && 0!=options_->delete_threshold)
                {
                    Version::FileMetaDataVector_t::iterator it;

                    for (it=v->files_[level].begin();
                         v->files_[level].end()!=it && !compaction_found;
                         ++it)
                    {
                        // if number of tombstones in stats exceeds threshold,
                        //  we have a compaction candidate
                        if (options_->delete_threshold <= GetTableCache()->GetStatisticValue((*it)->number, eSstCountDeleteKey))
                        {
                            compaction_found=true;
                            best_level=level;
                            best_score=0;
                            v->file_to_compact_=*it;
                            v->file_to_compact_level_=level;
                            is_grooming=true;
                            no_move=true;
                        }
                    }   // for
                }   // if
            }   // else

            // this code block is old, should be rewritten
            if (1<=score)
            {
                best_level = level;
                best_score = score;
                compaction_found=true;
            }   // if

            // finally test for expiry if no compaction candidates
            if (!compaction_found && options_->ExpiryActivated())
            {
                compaction_found=options_->expiry_module->CompactionFinalizeCallback(false,
                                                                                     *v,
                                                                                     level,
                                                                                     NULL);

                if (compaction_found)
                {
                    best_level=level;
                    best_score=0;
                    is_grooming=false;
                    no_move=true;
                    expire_file=true;
                    v->file_to_compact_level_=level;
                }   // if
            }   // if
        }   // if
    }   // for

    // set (almost) all at once to ensure
    //  no hold over from prior Finalize() call on this version.
    //  (could rewrite cleaner by doing reset of these at top of function)
    v->compaction_level_ = best_level;
    v->compaction_score_ = best_score;
    v->compaction_grooming_ = is_grooming;
    v->compaction_no_move_ = no_move;
    v->compaction_expirefile_ = expire_file;

    return(compaction_found);

} // VersionSet::Finalize


/**
 * UpdatePenalty was previous part of Finalize().  It is now
 *  an independent routine dedicated to setting the penalty
 *  value used within the WriteThrottle calculations.
 *
 * Penalty is an estimate of how many compactions/keys of work
 *  are overdue.
 */
void
VersionSet::UpdatePenalty(
    Version* v)
{
    int penalty=0;

    for (int level = 0; level < config::kNumLevels-1; ++level)
    {
        int loop, count, value;

        value=0;
        count=0;

        if (gLevelTraits[level].m_OverlappedFiles)
        {

            // compute penalty for write throttle if too many Level-0 files accumulating
            if (config::kL0_SlowdownWritesTrigger < v->NumFiles(level))
            {
                // assume each overlapped file represents another pass at same key
                //   and we are "close" on compaction backlog
                if ( v->NumFiles(level) < config::kL0_SlowdownWritesTrigger)
                {
                    // this code block will not execute due both "if"s using same values now
                    value = 1;
                    count = 0;
                }   // if

                // no longer estimating work, now trying to throw on the breaks
                //  to keep leveldb from stalling
                else
                {
                    count=(v->NumFiles(level) - config::kL0_SlowdownWritesTrigger);

                    // level 0 has own thread pool and will stall writes,
                    //  heavy penalty
                    if (0==level)
                    {   // non-linear penalty
                        value=2;
                    }   // if
                    else
                    {   // slightly less penalty
                        value=1;
                    }   // else
                }   // else
            }   // if
        }   // if
        else
        {
            const uint64_t level_bytes = TotalFileSize(v->GetFileList(level));

	    // how dire is the situation
            count=(int)(static_cast<double>(level_bytes) / gLevelTraits[level].m_MaxBytesForLevel);

            if (0<count)
            {
	        // how many compaction behind
                value=(level_bytes-gLevelTraits[level].m_MaxBytesForLevel) / options_->write_buffer_size;
                value+=1;
            }   // if

            // this penalty is about reducing write amplification, its
            //  side effect is to also improve compaction performance across
            //  the level 1 to 2 to 3 boundry.
            else if (config::kNumOverlapLevels==level
                && gLevelTraits[level].m_DesiredBytesForLevel < level_bytes)
            {
                // this approximates the number of compactions needed, no other penalty
                value=(int)(static_cast<double>(level_bytes-gLevelTraits[level].m_DesiredBytesForLevel) / options_->write_buffer_size);

		// how urgent is the need to clear this level before next flood
		//  (negative value is ignored)
                count= v->NumFiles(level-1) - (config::kL0_CompactionTrigger/2);

                // only throttle if backlog on the horizon
                if (count < 0)
                    value=0;
            }   // else if

        }   // else

        penalty+=value;

    }   // for

    // put a ceiling on the value
    if (1000<penalty || penalty<0)
        penalty=1000;

    uint64_t temp_min;
    temp_min=port::TimeMicros();

    if (last_penalty_minutes_<temp_min)
    {
        last_penalty_minutes_=temp_min+15*1000000;


        if (prev_write_penalty_<penalty)
            prev_write_penalty_+=(penalty - prev_write_penalty_)/7 +1;
        else
            prev_write_penalty_-=(prev_write_penalty_ - penalty)/5 +1;

        if (prev_write_penalty_ < 0)
            prev_write_penalty_ = 0;
    }   // if

    v->write_penalty_=prev_write_penalty_;

    return;

}   // VersionSet::UpdatePenalty


Status VersionSet::WriteSnapshot(log::Writer* log) {
  // TODO: Break up into multiple records to reduce memory usage on recovery?

  // Save metadata
  VersionEdit edit;
  edit.SetComparatorName(icmp_.user_comparator()->Name());

  // Save compaction pointers
  for (int level = 0; level < config::kNumLevels; level++) {
    if (!compact_pointer_[level].empty()) {
      InternalKey key;
      key.DecodeFrom(compact_pointer_[level]);
      edit.SetCompactPointer(level, key);
    }
  }

  // Save files
  for (int level = 0; level < config::kNumLevels; level++) {
    const std::vector<FileMetaData*>& files = current_->files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      const FileMetaData* f = files[i];
      edit.AddFile2(level, f->number, f->file_size, f->smallest, f->largest,
                    f->exp_write_low, f->exp_write_high, f->exp_explicit_high);
    }
  }

  std::string record;
  edit.EncodeTo(&record, options_->ExpiryActivated());
  return log->AddRecord(record);
}

size_t VersionSet::NumLevelFiles(int level) const {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return current_->files_[level].size();
}

bool VersionSet::IsLevelOverlapped(int level) {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return(gLevelTraits[level].m_OverlappedFiles);
}

uint64_t VersionSet::DesiredBytesForLevel(int level) {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return(gLevelTraits[level].m_DesiredBytesForLevel);
}

uint64_t VersionSet::MaxBytesForLevel(int level) {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return(gLevelTraits[level].m_MaxBytesForLevel);
}

uint64_t VersionSet::MaxFileSizeForLevel(int level) {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return(gLevelTraits[level].m_MaxFileSizeForLevel);
}

const char* VersionSet::LevelSummary(LevelSummaryStorage* scratch) const {
  // Update code if kNumLevels changes
  assert(config::kNumLevels == 7);
  snprintf(scratch->buffer, sizeof(scratch->buffer),
           "files[ %d %d %d %d %d %d %d ]",
           int(current_->files_[0].size()),
           int(current_->files_[1].size()),
           int(current_->files_[2].size()),
           int(current_->files_[3].size()),
           int(current_->files_[4].size()),
           int(current_->files_[5].size()),
           int(current_->files_[6].size()));
  return scratch->buffer;
}

const char* VersionSet::CompactionSummary(LevelSummaryStorage* scratch) const {
  // Update code if kNumLevels changes
  assert(config::kNumLevels == 7);
  snprintf(scratch->buffer, sizeof(scratch->buffer),
           "files[ %d,%d %d,%d %d,%d %d,%d %d,%d %d,%d %d,%d ]",
           m_CompactionStatus[0].m_Submitted, m_CompactionStatus[0].m_Running,
           m_CompactionStatus[1].m_Submitted, m_CompactionStatus[1].m_Running,
           m_CompactionStatus[2].m_Submitted, m_CompactionStatus[2].m_Running,
           m_CompactionStatus[3].m_Submitted, m_CompactionStatus[3].m_Running,
           m_CompactionStatus[4].m_Submitted, m_CompactionStatus[4].m_Running,
           m_CompactionStatus[5].m_Submitted, m_CompactionStatus[5].m_Running,
           m_CompactionStatus[6].m_Submitted, m_CompactionStatus[6].m_Running);

  return scratch->buffer;
}

uint64_t VersionSet::ApproximateOffsetOf(Version* v, const InternalKey& ikey) {
  uint64_t result = 0;
  for (int level = 0; level < config::kNumLevels; level++) {
    const std::vector<FileMetaData*>& files = v->files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      if (icmp_.Compare(files[i]->largest, ikey) <= 0) {
        // Entire file is before "ikey", so just add the file size
        result += files[i]->file_size;
      } else if (icmp_.Compare(files[i]->smallest, ikey) > 0) {
        // Entire file is after "ikey", so ignore
        if (!gLevelTraits[level].m_OverlappedFiles) {
          // Non-overlapped files are sorted by meta->smallest, so
          // no further files in this level will contain data for
          // "ikey".
          break;
        }
      } else {
        // "ikey" falls in the range for this table.  Add the
        // approximate offset of "ikey" within the table.
        Table* tableptr;
        Iterator* iter = table_cache_->NewIterator(
            ReadOptions(), files[i]->number, files[i]->file_size, level, &tableptr);
        if (tableptr != NULL) {
          result += tableptr->ApproximateOffsetOf(ikey.Encode());
        }
        delete iter;
      }
    }
  }
  return result;
}

void VersionSet::AddLiveFiles(std::set<uint64_t>* live) {
  for (Version* v = dummy_versions_.next_;
       v != &dummy_versions_;
       v = v->next_) {
    for (int level = 0; level < config::kNumLevels; level++) {
      const std::vector<FileMetaData*>& files = v->files_[level];
      for (size_t i = 0; i < files.size(); i++) {
        live->insert(files[i]->number);
      }
    }
  }
}

int64_t VersionSet::NumLevelBytes(int level) const {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return TotalFileSize(current_->files_[level]);
}

int64_t VersionSet::MaxNextLevelOverlappingBytes() {
  int64_t result = 0;
  std::vector<FileMetaData*> overlaps;
  for (int level = 1; level < config::kNumLevels - 1; level++) {
    for (size_t i = 0; i < current_->files_[level].size(); i++) {
      const FileMetaData* f = current_->files_[level][i];
      current_->GetOverlappingInputs(level+1, &f->smallest, &f->largest,
                                     &overlaps);
      const int64_t sum = TotalFileSize(overlaps);
      if (sum > result) {
        result = sum;
      }
    }
  }
  return result;
}

// Stores the minimal range that covers all entries in inputs in
// *smallest, *largest.
// REQUIRES: inputs is not empty
void VersionSet::GetRange(const std::vector<FileMetaData*>& inputs,
                          InternalKey* smallest,
                          InternalKey* largest) {
  assert(!inputs.empty());
  smallest->Clear();
  largest->Clear();
  for (size_t i = 0; i < inputs.size(); i++) {
    FileMetaData* f = inputs[i];
    if (i == 0) {
      *smallest = f->smallest;
      *largest = f->largest;
    } else {
      if (icmp_.Compare(f->smallest, *smallest) < 0) {
        *smallest = f->smallest;
      }
      if (icmp_.Compare(f->largest, *largest) > 0) {
        *largest = f->largest;
      }
    }
  }
}

// Stores the minimal range that covers all entries in inputs1 and inputs2
// in *smallest, *largest.
// REQUIRES: inputs is not empty
void VersionSet::GetRange2(const std::vector<FileMetaData*>& inputs1,
                           const std::vector<FileMetaData*>& inputs2,
                           InternalKey* smallest,
                           InternalKey* largest) {
  std::vector<FileMetaData*> all = inputs1;
  all.insert(all.end(), inputs2.begin(), inputs2.end());
  GetRange(all, smallest, largest);
}

Iterator* VersionSet::MakeInputIterator(Compaction* c) {
  ReadOptions options;
  options.verify_checksums = options_->verify_compactions;
  options.fill_cache = false;
  options.is_compaction = true;
  options.info_log = options_->info_log;
  options.dbname = dbname_;
  options.env = env_;

  int which_limit, space;

  // Level-0 files have to be merged together.  For other levels,
  // we will make a concatenating iterator per level.
  // TODO(opt): use concatenating iterator for level-0 if there is no overlap
  // (during a repair, all levels use merge iterator as a precaution)
  if (!options_->is_repair)
      space = (gLevelTraits[c->level()].m_OverlappedFiles ? c->inputs_[0].size() + 1 : 2);
  else
      space =  c->inputs_[0].size() + c->inputs_[1].size();

  Iterator** list = new Iterator*[space];
  int num = 0;

  which_limit=gLevelTraits[c->level()+1].m_OverlappedFiles ? 1 : 2;
  for (int which = 0; which < which_limit; which++) {
    if (!c->inputs_[which].empty()) {
      if (gLevelTraits[c->level() + which].m_OverlappedFiles || options_->is_repair) {
        const std::vector<FileMetaData*>& files = c->inputs_[which];
        for (size_t i = 0; i < files.size(); i++) {
          list[num++] = table_cache_->NewIterator(
              options, files[i]->number, files[i]->file_size, c->level() + which);
        }
      } else {
        // Create concatenating iterator for the files from this level
        list[num++] = NewTwoLevelIterator(
            new Version::LevelFileNumIterator(icmp_, &c->inputs_[which]),
            &GetFileIterator, table_cache_, options);
      }
    }
  }
  assert(num <= space);
  Iterator* result = NewMergingIterator(&icmp_, list, num);
  delete[] list;
  return result;
}


/**
 * PickCompactions() directly feeds hot_thread pools as of October 2013
 */
void
VersionSet::PickCompaction(
    class DBImpl * db_impl)
{
  Compaction* c;
  int level;

  // perform this once per call ... since Finalize now loops
  UpdatePenalty(current_);

  // submit a work object for every valid compaction needed
  current_->compaction_level_=-1;
  while(Finalize(current_))
  {
      bool submit_flag;

      Log(options_->info_log,"Finalize level: %d, grooming %d",
	  current_->compaction_level_, current_->compaction_grooming_);

      c=NULL;

      // We prefer compactions triggered by too much data in a level over
      // the compactions triggered by seeks.  (Riak redefines "seeks" to
      // "files containing delete tombstones")
      const bool size_compaction = (current_->compaction_score_ >= 1);
      const bool seek_compaction = (current_->file_to_compact_ != NULL);
      if (size_compaction)
      {
          level = current_->compaction_level_;
          assert(level >= 0);
          assert(level+1 < config::kNumLevels);

          c = new Compaction(level);

          // Pick the first file that comes after compact_pointer_[level]
          for (size_t i = 0; i < current_->files_[level].size(); i++) {
              FileMetaData* f = current_->files_[level][i];
              if (compact_pointer_[level].empty() ||
                  icmp_.Compare(f->largest.Encode(), compact_pointer_[level]) > 0) {
                  c->inputs_[0].push_back(f);
                  break;
              }
          }
          if (c->inputs_[0].empty()) {
              // Wrap-around to the beginning of the key space
              c->inputs_[0].push_back(current_->files_[level][0]);
          }
      } else if (seek_compaction) {
          level = current_->file_to_compact_level_;
          c = new Compaction(level);
          c->inputs_[0].push_back(current_->file_to_compact_);
      } else if (current_->compaction_expirefile_) {
          level = current_->file_to_compact_level_;
          c = new Compaction(level);
          c->compaction_type_=kExpiryFileCompaction;
      } else {
          return;
      }

      c->input_version_ = current_;
      c->input_version_->Ref();
      c->no_move_ = current_->compaction_no_move_;

      // set submitted as race defense
      m_CompactionStatus[level].m_Submitted=true;

      if (!current_->compaction_expirefile_)
      {
          // m_OverlappedFiles==true levels have files that
          //   may overlap each other, so pick up all overlapping ones
          if (gLevelTraits[level].m_OverlappedFiles) {
              InternalKey smallest, largest;
              GetRange(c->inputs_[0], &smallest, &largest);
              // Note that the next call will discard the file we placed in
              // c->inputs_[0] earlier and replace it with an overlapping set
              // which will include the picked file.
              current_->GetOverlappingInputs(level, &smallest, &largest, &c->inputs_[0]);
              assert(!c->inputs_[0].empty());

              // this can get into tens of thousands after a repair
              //  keep it sane
              size_t max_open_files=100;  // previously an options_ member variable
              if (max_open_files < c->inputs_[0].size())
              {
                  std::nth_element(c->inputs_[0].begin(),
                                   c->inputs_[0].begin()+max_open_files-1,
                                   c->inputs_[0].end(),FileMetaDataPtrCompare(options_->comparator));
                  c->inputs_[0].erase(c->inputs_[0].begin()+max_open_files,
                                      c->inputs_[0].end());
              }   // if
          }   // if

          SetupOtherInputs(c);

          ThreadTask * task=new CompactionTask(db_impl, c);

          if (0==level)
              submit_flag=gLevel0Threads->Submit(task, !current_->compaction_grooming_);
          else
              submit_flag=gCompactionThreads->Submit(task, !current_->compaction_grooming_);
      }   // if

      // expiry compaction
      else
      {
          ThreadTask * task=new CompactionTask(db_impl, c);
          submit_flag=gCompactionThreads->Submit(task, true);
      }   // else

      // set/reset submitted based upon truth of queuing
      //  (ref counting will auto delete task rejected)
      m_CompactionStatus[level].m_Submitted=submit_flag;

  }   // while

  return;
}

void VersionSet::SetupOtherInputs(Compaction* c) {
  const int level = c->level();
  InternalKey smallest, largest;
  GetRange(c->inputs_[0], &smallest, &largest);

  if (!gLevelTraits[level+1].m_OverlappedFiles)
  {
      current_->GetOverlappingInputs(level+1, &smallest, &largest, &c->inputs_[1]);

      // Get entire range covered by compaction
      InternalKey all_start, all_limit;
      GetRange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);

      // See if we can grow the number of inputs in "level" without
      // changing the number of "level+1" files we pick up.
      if (!c->inputs_[1].empty()) {
          std::vector<FileMetaData*> expanded0;
          current_->GetOverlappingInputs(level, &all_start, &all_limit, &expanded0);
          //const int64_t inputs0_size = TotalFileSize(c->inputs_[0]);
          const int64_t inputs1_size = TotalFileSize(c->inputs_[1]);
          const int64_t expanded0_size = TotalFileSize(expanded0);
          if (expanded0.size() > c->inputs_[0].size() &&
              inputs1_size + expanded0_size < gLevelTraits[level].m_ExpandedCompactionByteSizeLimit) {
              InternalKey new_start, new_limit;
              GetRange(expanded0, &new_start, &new_limit);
              std::vector<FileMetaData*> expanded1;
              current_->GetOverlappingInputs(level+1, &new_start, &new_limit,
                                             &expanded1);
              if (expanded1.size() == c->inputs_[1].size()) {
#if 0  // mutex_ held
                  Log(options_->info_log,
                      "Expanding@%d %d+%d (%ld+%ld bytes) to %d+%d (%ld+%ld bytes)\n",
                      level,
                      int(c->inputs_[0].size()),
                      int(c->inputs_[1].size()),
                      long(inputs0_size), long(inputs1_size),
                      int(expanded0.size()),
                      int(expanded1.size()),
                      long(expanded0_size), long(inputs1_size));
#endif
                  smallest = new_start;
                  largest = new_limit;
                  c->inputs_[0] = expanded0;
                  c->inputs_[1] = expanded1;
                  GetRange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);
              }
          }
      }

      // Compute the set of grandparent files that overlap this compaction
      // (parent == level+1; grandparent == level+2)
      if (level + 2 < config::kNumLevels) {
          current_->GetOverlappingInputs(level + 2, &all_start, &all_limit,
                                         &c->grandparents_);
      }
  }   // if
#if 1
  // compacting into an overlapped layer
  else
  {
      // if this is NOT a repair (or panic) situation, take all files
      //  to reduce write amplification
      if (c->inputs_[0].size()<=config::kL0_StopWritesTrigger
          && c->inputs_[0].size()!=current_->files_[level].size())
      {
          c->inputs_[0].clear();
          c->inputs_[0].reserve(current_->files_[level].size());

          for (size_t i = 0; i < current_->files_[level].size(); ++i )
          {
              FileMetaData* f = current_->files_[level][i];
              c->inputs_[0].push_back(f);
          }   // for

          GetRange(c->inputs_[0], &smallest, &largest);
      }   // if
  }   // else
#endif

  if (false) {
    Log(options_->info_log, "Compacting %d '%s' .. '%s'",
        level,
        smallest.DebugString().c_str(),
        largest.DebugString().c_str());
  }

  // Update the place where we will do the next compaction for this level.
  // We update this immediately instead of waiting for the VersionEdit
  // to be applied so that if the compaction fails, we will try a different
  // key range next time.
  compact_pointer_[level] = largest.Encode().ToString();
  c->edit_.SetCompactPointer(level, largest);
}

Compaction* VersionSet::CompactRange(
    int level,
    const InternalKey* begin,
    const InternalKey* end) {
  std::vector<FileMetaData*> inputs;
  current_->GetOverlappingInputs(level, begin, end, &inputs);
  if (inputs.empty()) {
    return NULL;
  }

  // Avoid compacting too much in one shot in case the range is large.
  const uint64_t limit = gLevelTraits[level].m_MaxFileSizeForLevel;
  uint64_t total = 0;
  for (size_t i = 0; i < inputs.size(); i++) {
    uint64_t s = inputs[i]->file_size;
    total += s;
    if (total >= limit) {
      inputs.resize(i + 1);
      break;
    }
  }

  Compaction* c = new Compaction(level);
  c->input_version_ = current_;
  c->input_version_->Ref();
  c->inputs_[0] = inputs;
  SetupOtherInputs(c);
  return c;
}


Compaction::Compaction(int level)
    : level_(level),
      max_output_file_size_(gLevelTraits[level].m_MaxFileSizeForLevel),
      input_version_(NULL),
      compaction_type_(kNormalCompaction),
      grandparent_index_(0),
      seen_key_(false),
      overlapped_bytes_(0),
      tot_user_data_(0), tot_index_keys_(0),
      avg_value_size_(0), avg_key_size_(0), avg_block_size_(0),
      compressible_(true),
      stats_done_(false),
      no_move_(false)
  {
  for (int i = 0; i < config::kNumLevels; i++) {
    level_ptrs_[i] = 0;
  }
}

Compaction::~Compaction() {
  if (input_version_ != NULL) {
    input_version_->Unref();
  }
}

bool Compaction::IsTrivialMove() const {
  // Avoid a move if there is lots of overlapping grandparent data.
  // Otherwise, the move could create a parent file that will require
  // a very expensive merge later on.
#if 1
  return (!gLevelTraits[level_].m_OverlappedFiles &&
          IsMoveOk() &&
          num_input_files(0) == 1 &&
          num_input_files(1) == 0 &&
          (uint64_t)TotalFileSize(grandparents_) <= gLevelTraits[level_].m_MaxGrandParentOverlapBytes);
#else
  // removed this functionality when creating gLevelTraits[].m_OverlappedFiles
  //  flag.  "Move" was intented by Google to delay compaction by moving small
  //  files in-between non-overlapping sorted files.  New concept is to delay
  //  all compactions by creating larger log files before starting to thrash
  //  disk by maintaining smaller sorted files.  Less thrash -> higher throughput
  return(false);
#endif

}

void Compaction::AddInputDeletions(VersionEdit* edit) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < inputs_[which].size(); i++) {
      edit->DeleteFile(level_ + which, inputs_[which][i]->number);
    }
  }
}

bool Compaction::IsBaseLevelForKey(const Slice& user_key) {
    bool ret_flag;

    ret_flag=true;

    if (gLevelTraits[level_].m_OverlappedFiles
        || gLevelTraits[level_+1].m_OverlappedFiles)
    {
        ret_flag=false;
    }   // if
    else
    {
        // Maybe use binary search to find right entry instead of linear search?
        const Comparator* user_cmp = input_version_->vset_->icmp_.user_comparator();
        for (int lvl = level_ + 2; lvl < config::kNumLevels; lvl++) {
            const std::vector<FileMetaData*>& files = input_version_->files_[lvl];
            for (; level_ptrs_[lvl] < files.size(); ) {
                FileMetaData* f = files[level_ptrs_[lvl]];
                if (user_cmp->Compare(user_key, f->largest.user_key()) <= 0) {
                    // We've advanced far enough
                    if (user_cmp->Compare(user_key, f->smallest.user_key()) >= 0) {
                        // Key falls in this file's range, so definitely not base level
                        return false;
                    }
                    break;
                }
                level_ptrs_[lvl]++;
            }
        }
    }   // else

    return ret_flag;
}

bool Compaction::ShouldStopBefore(const Slice& internal_key, size_t key_count) {

  bool ret_flag(false);

  // This is a look ahead to see how costly this key will make the subsequent compaction
  //  of this new file to the next higher level.  Start a new file if the cost is high.
  if (!gLevelTraits[level()+1].m_OverlappedFiles)
  {
    // Scan to find earliest grandparent file that contains key.
    const InternalKeyComparator* icmp = &input_version_->vset_->icmp_;
    while (grandparent_index_ < grandparents_.size() &&
           icmp->Compare(internal_key,
                         grandparents_[grandparent_index_]->largest.Encode()) > 0) {
      if (seen_key_) {
        overlapped_bytes_ += grandparents_[grandparent_index_]->file_size;
      }
        grandparent_index_++;
    }
    seen_key_ = true;

    if (overlapped_bytes_ > gLevelTraits[level_].m_MaxGrandParentOverlapBytes) {
      // Too much overlap for current output; start new output
      ret_flag=true;
    } // if

    // Second consideration:  sorted files need to keep the bloom filter size controlled
    //  to meet file open speed goals
    else
    {
      ret_flag=(300000<key_count);
    } // else
  }  // if

  if (ret_flag)
    overlapped_bytes_ = 0;

  return(ret_flag);
}

void Compaction::ReleaseInputs() {
  if (input_version_ != NULL) {
    input_version_->Unref();
    input_version_ = NULL;
  }
}

/**
 * Riak specific:  populate statistics data about this compaction
 */
void
Compaction::CalcInputStats(
    TableCache & tables)
{
    uint64_t temp, temp_cnt;
    size_t value_count, key_count, block_count;

    if (!stats_done_)
    {
        tot_user_data_=0;
        tot_index_keys_=0;
        avg_value_size_=0; value_count=0;
        avg_key_size_=0;   key_count=0;
        avg_block_size_=0; block_count=0;
        compressible_=(0==level_);

        // walk both levels of input files
        const size_t level0Count = inputs_[0].size();
        const size_t level1Count = inputs_[1].size();
        const size_t totalCount = level0Count + level1Count;

        for (size_t j = 0; j < totalCount; ++j)
        {
            FileMetaData * fmd;
            Status s;
            Cache::Handle * handle;
            size_t user_est, idx_est;

            fmd=(j < level0Count) ? inputs_[0][j] : inputs_[1][j-level0Count];

            // compression test
            // true if more data blocks than data blocks that did not compress
            //    or if no statistics available
            compressible_ = compressible_
                            || (tables.GetStatisticValue(fmd->number, eSstCountBlocks)
                               >tables.GetStatisticValue(fmd->number, eSstCountCompressAborted))
                              || 0==tables.GetStatisticValue(fmd->number, eSstCountBlocks);

            // block sizing algorithm
            temp=0;
            temp_cnt=0;
            user_est=0;
            idx_est=0;

            // get and hold handle to cache entry
            s=tables.TEST_FindTable(fmd->number, fmd->file_size, fmd->level, &handle);

            if (s.ok())
            {
                // 1. total size of all blocks before compaction
                temp=tables.GetStatisticValue(fmd->number, eSstCountBlockSize);

                // estimate size when counter does not exist
                if (0==temp)
                {
                    TableAndFile * tf;

                    tf=reinterpret_cast<TableAndFile*>(tables.TEST_GetInternalCache()->Value(handle));
                    if (tf->table->TableObjectSize() < fmd->file_size)
                        temp=fmd->file_size - tf->table->TableObjectSize();
                }   // if

                user_est=temp;
                tot_user_data_+=temp;

                // 2. total keys in the indexes
                temp=tables.GetStatisticValue(fmd->number, eSstCountIndexKeys);

                // estimate total when counter does not exist
                if (0==temp)
                {
                    TableAndFile * tf;
                    Block * index_block;

                    tf=reinterpret_cast<TableAndFile*>(tables.TEST_GetInternalCache()->Value(handle));
                    index_block=tf->table->TEST_GetIndexBlock();
                    temp=index_block->NumRestarts();
                }   // if

                idx_est=temp;
                tot_index_keys_+=temp;

                // 3. average size of values in input set
                //    (value is really size of value plus size of key)
                temp=tables.GetStatisticValue(fmd->number, eSstCountValueSize);
                temp+=tables.GetStatisticValue(fmd->number, eSstCountKeySize);
                temp_cnt=tables.GetStatisticValue(fmd->number, eSstCountKeys);

                // estimate total when counter does not exist
                if (0==temp || 0==temp_cnt)
                {
                    // no way to estimate total key count
                    //  (ok, could try from bloom filter size ... but likely no
                    //   bloom filter if no stats)
                    temp=0;
                    temp_cnt=0;
                }   // if

                avg_value_size_+=temp;
                value_count+=temp_cnt;

                // 4. average key size
                temp=tables.GetStatisticValue(fmd->number, eSstCountKeySize);
                temp_cnt=tables.GetStatisticValue(fmd->number, eSstCountKeys);

                // estimate total when counter does not exist
                if (0==temp || 0==temp_cnt)
                {
                    // no way to estimate total key count
                    //  (ok, could try from bloom filter size ... but likely no
                    //   bloom filter if no stats)
                    temp=0;
                    temp_cnt=0;
                }   // if

                avg_key_size_+=temp;
                key_count+=temp_cnt;

                // 5. block key size
                temp=tables.GetStatisticValue(fmd->number, eSstCountBlockSizeUsed);
                temp_cnt=tables.GetStatisticValue(fmd->number, eSstCountBlocks);
                temp*=temp_cnt;

                // estimate total when counter does not exist
                if (0==temp || 0==temp_cnt)
                {
                    temp=user_est;
                    temp_cnt=idx_est;
                }   // if

                avg_block_size_+=temp;
                block_count+=temp_cnt;

                // cleanup
                tables.Release(handle);
            }   // if
        }   // for

        // compute averages
        if (0!=value_count)
            avg_value_size_/=value_count;
        else
            avg_value_size_=0;

        if (0!=key_count)
            avg_key_size_/=key_count;
        else
            avg_key_size_=0;

        if (0!=block_count)
            avg_block_size_/=block_count;
        else
            avg_block_size_=0;

        // only want to do this once per compaction
        stats_done_=true;
    }   // if

    return;

}   // Compaction::CalcInputStats


}   // namespace leveldb
