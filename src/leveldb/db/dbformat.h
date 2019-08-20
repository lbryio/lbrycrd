// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_FORMAT_H_
#define STORAGE_LEVELDB_DB_FORMAT_H_

#include <stdio.h>
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

class Compaction;

// Grouping of constants.  We may want to make some of these
// parameters set via options.
namespace config {
static const int kNumLevels = 7;
static const int kNumOverlapLevels = 2;

// Level-0 compaction is started when we hit this many files.
// Google:  static const size_t kL0_CompactionTrigger = 4;
static const size_t kL0_CompactionTrigger = 6;

// Level-0 (any overlapped level) number of files where a grooming
//     compaction could start
static const size_t kL0_GroomingTrigger = 4;
static const size_t kL0_GroomingTrigger10min = 2;
static const size_t kL0_GroomingTrigger20min = 1;

// ... time limits in microseconds
static const size_t kL0_Grooming10minMicros = 10 * 60 * 1000000;
static const size_t kL0_Grooming20minMicros = 20 * 60 * 1000000;

// Soft limit on number of level-0 files.  We slow down writes at this point.
static const size_t kL0_SlowdownWritesTrigger = 8;

// Maximum number of level-0 files.  We stop writes at this point.
static const size_t kL0_StopWritesTrigger = 12;

// Maximum level to which a new compacted memtable is pushed if it
// does not create overlap.  We try to push to level 2 to avoid the
// relatively expensive level 0=>1 compactions and to avoid some
// expensive manifest file operations.  We do not push all the way to
// the largest level since that can generate a lot of wasted disk
// space if the same key space is being repeatedly overwritten.
// Basho: push to kNumOverlapLevels +1 ... beyond "landing level"
static const unsigned kMaxMemCompactLevel = kNumOverlapLevels+1;

}  // namespace config

class InternalKey;

// kValueTypeForSeek defines the ValueType that should be passed when
// constructing a ParsedInternalKey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// ValueType, not the lowest).
//  Riak note: kValueTypeForSeek is placed within temporary keys
//             for comparisons.  Using kTypeValueExplicitExpiry would
//             force more code changes to increase internal key size.
//             But ValueTypeForSeek is redundant to sequence number for
//             disambiguaty. Therefore going for easiest path and NOT changing.
static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;
typedef uint64_t ExpiryTimeMicros;

// We leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
static const SequenceNumber kMaxSequenceNumber =
    ((0x1ull << 56) - 1);

struct ParsedInternalKey {
  Slice user_key;
  ExpiryTimeMicros expiry;
  SequenceNumber sequence;
  ValueType type;

  ParsedInternalKey() { }  // Intentionally left uninitialized (for speed)
  ParsedInternalKey(const Slice& u, const ExpiryTimeMicros & exp, const SequenceNumber& seq, ValueType t)
      : user_key(u), expiry(exp), sequence(seq), type(t) { }
  std::string DebugString() const;
  std::string DebugStringHex() const;
};

// Append the serialization of "key" to *result.
extern void AppendInternalKey(std::string* result,
                              const ParsedInternalKey& key);

// Attempt to parse an internal key from "internal_key".  On success,
// stores the parsed data in "*result", and returns true.
//
// On error, returns false, leaves "*result" in an undefined state.
extern bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result);

inline ValueType ExtractValueType(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  const size_t n = internal_key.size();
  unsigned char c = DecodeLeastFixed64(internal_key.data() + n - sizeof(SequenceNumber));
  return static_cast<ValueType>(c);
}

inline size_t KeySuffixSize(ValueType val_type) {
  size_t ret_val;
  switch(val_type)
  {
      case kTypeDeletion:
      case kTypeValue:
          ret_val=sizeof(SequenceNumber);
          break;

      case kTypeValueWriteTime:
      case kTypeValueExplicitExpiry:
          ret_val=sizeof(SequenceNumber) + sizeof(ExpiryTimeMicros);
          break;

      default:
          // assert(0);  cannot use because bloom filter block's name is passed as internal key
          ret_val=sizeof(SequenceNumber);
          break;
  }   // switch
  return(ret_val);
}

const char * KeyTypeString(ValueType val_type);

inline size_t KeySuffixSize(const Slice & internal_key) {
    return(KeySuffixSize(ExtractValueType(internal_key)));
}

// Returns the user key portion of an internal key.
inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - KeySuffixSize(internal_key));
}

// Returns the sequence number with ValueType removed
inline SequenceNumber ExtractSequenceNumber(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return(DecodeFixed64(internal_key.data() + internal_key.size() - 8)>>8);
}

// Return the length of the encoding of "key".
inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key) {
  return key.user_key.size() + KeySuffixSize(key.type);
}

// Riak: is this an expiry key and therefore contain extra ExpiryTime field
inline bool IsExpiryKey(ValueType val_type) {
  return(kTypeValueWriteTime==val_type || kTypeValueExplicitExpiry==val_type);
}

// Riak: is this an expiry key and therefore contain extra ExpiryTime field
inline bool IsExpiryKey(const Slice & internal_key) {
    return(internal_key.size()>=KeySuffixSize(kTypeValueWriteTime)
           && IsExpiryKey(ExtractValueType(internal_key)));
}

// Riak: extracts expiry value
inline ExpiryTimeMicros ExtractExpiry(const Slice& internal_key) {
  assert(internal_key.size() >= KeySuffixSize(kTypeValueWriteTime));
  assert(IsExpiryKey(internal_key));
  return(DecodeFixed64(internal_key.data() + internal_key.size() - KeySuffixSize(kTypeValueWriteTime)));
}

// A comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;
 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) { }
  virtual const char* Name() const;
  virtual int Compare(const Slice& a, const Slice& b) const;
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const;
  virtual void FindShortSuccessor(std::string* key) const;

  const Comparator* user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey& a, const InternalKey& b) const;
};

// Filter policy wrapper that converts from internal keys to user keys
class InternalFilterPolicy : public FilterPolicy {
 protected:
  const FilterPolicy* const user_policy_;
 public:
  explicit InternalFilterPolicy(const FilterPolicy* p) : user_policy_(p) { }
  virtual const char* Name() const;
  virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const;
  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const;
};

class InternalFilterPolicy2 : public InternalFilterPolicy {
 public:
  explicit InternalFilterPolicy2(const FilterPolicy* p) : InternalFilterPolicy(p) { }
  virtual ~InternalFilterPolicy2() {delete user_policy_;};
};

// Modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an InternalKeyComparator.
class InternalKey {
 private:
  std::string rep_;
 public:
  InternalKey() { }   // Leave rep_ as empty to indicate it is invalid
  InternalKey(const Slice& user_key, ExpiryTimeMicros exp, SequenceNumber s, ValueType t) {
    AppendInternalKey(&rep_, ParsedInternalKey(user_key, exp, s, t));
  }

  void DecodeFrom(const Slice& s) { rep_.assign(s.data(), s.size()); }
  Slice Encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  Slice user_key() const { return ExtractUserKey(rep_); }
  Slice internal_key() const { return Slice(rep_); }

  void SetFrom(const ParsedInternalKey& p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(
    const InternalKey& a, const InternalKey& b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  if (IsExpiryKey((ValueType)c))
    result->expiry=DecodeFixed64(internal_key.data() + n - KeySuffixSize((ValueType)c));
  else
    result->expiry=0;
  result->user_key = Slice(internal_key.data(), n - KeySuffixSize((ValueType)c));
  return (c <= static_cast<unsigned char>(kTypeValueExplicitExpiry));
}

// A helper class useful for DBImpl::Get()
class LookupKey {
 public:
  // Initialize *this for looking up user_key at a snapshot with
  // the specified sequence number.
  LookupKey(const Slice& user_key, SequenceNumber sequence, KeyMetaData * meta=NULL);

  ~LookupKey();

  // Return a key suitable for lookup in a MemTable.
  Slice memtable_key() const { return Slice(start_, end_ - start_); }

  // Return an internal key (suitable for passing to an internal iterator)
  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

  // Return the user key
  Slice user_key() const
  { return Slice(kstart_, end_ - kstart_ - KeySuffixSize(internal_key())); }

  // did requestor have metadata object?
  bool WantsKeyMetaData() const {return(NULL!=meta_);};

  void SetKeyMetaData(ValueType type, SequenceNumber seq, ExpiryTimeMicros expiry) const
  {if (NULL!=meta_)
    {
      meta_->m_Type=type;
      meta_->m_Sequence=seq;
      meta_->m_Expiry=expiry;
    } // if
  };

  void SetKeyMetaData(const ParsedInternalKey & pi_key) const
  {if (NULL!=meta_)
    {
      meta_->m_Type=pi_key.type;
      meta_->m_Sequence=pi_key.sequence;
      meta_->m_Expiry=pi_key.expiry;
    } // if
  };

  void SetKeyMetaData(const KeyMetaData & meta) const
  {if (NULL!=meta_) *meta_=meta;};

 private:
  // We construct a char array of the form:
  //    klength  varint32               <-- start_
  //    userkey  char[klength]          <-- kstart_
  //    optional uint64
  //    tag      uint64
  //                                    <-- end_
  // The array is a suitable MemTable key.
  // The suffix starting with "userkey" can be used as an InternalKey.
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];      // Avoid allocation for short keys

  // allow code that finds the key to place metadata here, even if 'const'
  mutable KeyMetaData * meta_;

  // No copying allowed
  LookupKey(const LookupKey&);
  void operator=(const LookupKey&);
};

inline LookupKey::~LookupKey() {
  if (start_ != space_) delete[] start_;
};


// this class was constructed from code with DBImpl::DoCompactionWork (db_impl.cc)
//   so it could be shared within BuildTable (and thus reduce Level 0 bloating)
class KeyRetirement
{
protected:
    // "state" from previous key reviewed
    std::string current_user_key;
    bool has_current_user_key;
    SequenceNumber last_sequence_for_key;

    // database values needed for processing
    const Comparator * user_comparator;
    SequenceNumber smallest_snapshot;
    const Options * options;
    Compaction * const compaction;

    bool valid;
    size_t dropped;   // tombstone or old version dropped
    size_t expired;   // expired dropped

public:
    KeyRetirement(const Comparator * UserComparator, SequenceNumber SmallestSnapshot,
                  const Options * Opts, Compaction * const Compaction=NULL);

    virtual ~KeyRetirement();

    bool operator()(Slice & key);

    size_t GetDroppedCount() const {return(dropped);};
    size_t GetExpiredCount() const {return(expired);};

private:
    KeyRetirement();
    KeyRetirement(const KeyRetirement &);
    const KeyRetirement & operator=(const KeyRetirement &);

};  // class KeyRetirement

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_FORMAT_H_
