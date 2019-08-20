// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdio.h>
//#include "leveldb/expiry.h"
#include "db/dbformat.h"
#include "db/version_set.h"
#include "port/port.h"
#include "util/coding.h"

namespace leveldb {

static uint64_t PackSequenceAndType(uint64_t seq, ValueType t) {
  assert(seq <= kMaxSequenceNumber);
  // assert(t <= kValueTypeForSeek);  requires revisit once expiry live
  assert(t <= kTypeValueExplicitExpiry);  // temp replacement for above
  return (seq << 8) | t;
}

void AppendInternalKey(std::string* result, const ParsedInternalKey& key) {
  result->append(key.user_key.data(), key.user_key.size());
  if (IsExpiryKey(key.type))
    PutFixed64(result, key.expiry);
  PutFixed64(result, PackSequenceAndType(key.sequence, key.type));
}

std::string ParsedInternalKey::DebugString() const {
  char buf[50];
  if (IsExpiryKey(type))
    snprintf(buf, sizeof(buf), "' @ %llu %llu : %d",
             (unsigned long long) expiry,
             (unsigned long long) sequence,
             int(type));
  else
    snprintf(buf, sizeof(buf), "' @ %llu : %d",
             (unsigned long long) sequence,
             int(type));
  std::string result = "'";
  result += HexString(user_key.ToString());
  result += buf;
  return result;
}

std::string ParsedInternalKey::DebugStringHex() const {
  char buf[50];
  if (IsExpiryKey(type))
    snprintf(buf, sizeof(buf), "' @ %llu %llu : %d",
             (unsigned long long) expiry,
             (unsigned long long) sequence,
             int(type));
  else
    snprintf(buf, sizeof(buf), "' @ %llu : %d",
             (unsigned long long) sequence,
             int(type));
  std::string result = "'";
  result += HexString(user_key);
  result += buf;
  return result;
}


const char * KeyTypeString(ValueType val_type) {
  const char * ret_ptr;
  switch(val_type)
  {
      case kTypeDeletion: ret_ptr="kTypeDelete"; break;
      case kTypeValue:    ret_ptr="kTypeValue"; break;
      case kTypeValueWriteTime: ret_ptr="kTypeValueWriteTime"; break;
      case kTypeValueExplicitExpiry: ret_ptr="kTypeValueExplicitExpiry"; break;
      default: ret_ptr="(unknown ValueType)"; break;
  }   // switch
  return(ret_ptr);
}

std::string InternalKey::DebugString() const {
  std::string result;
  ParsedInternalKey parsed;
  if (ParseInternalKey(rep_, &parsed)) {
    result = parsed.DebugString();
  } else {
    result = "(bad)";
    result.append(EscapeString(rep_));
  }
  return result;
}

const char* InternalKeyComparator::Name() const {
  return "leveldb.InternalKeyComparator";
}

int InternalKeyComparator::Compare(const Slice& akey, const Slice& bkey) const {
  // Order by:
  //    increasing user key (according to user-supplied comparator)
  //    decreasing sequence number
  //    decreasing type (though sequence# should be enough to disambiguate)
  int r = user_comparator_->Compare(ExtractUserKey(akey), ExtractUserKey(bkey));
  if (r == 0) {
    uint64_t anum = DecodeFixed64(akey.data() + akey.size() - 8);
    uint64_t bnum = DecodeFixed64(bkey.data() + bkey.size() - 8);
    if (IsExpiryKey((ValueType)*(unsigned char *)&anum)) *(unsigned char*)&anum=(unsigned char)kTypeValue;
    if (IsExpiryKey((ValueType)*(unsigned char *)&bnum)) *(unsigned char*)&bnum=(unsigned char)kTypeValue;
    if (anum > bnum) {
      r = -1;
    } else if (anum < bnum) {
      r = +1;
    }
  }
  return r;
}

void InternalKeyComparator::FindShortestSeparator(
      std::string* start,
      const Slice& limit) const {
  // Attempt to shorten the user portion of the key
  Slice user_start = ExtractUserKey(*start);
  Slice user_limit = ExtractUserKey(limit);
  std::string tmp(user_start.data(), user_start.size());
  user_comparator_->FindShortestSeparator(&tmp, user_limit);
  if (tmp.size() < user_start.size() &&
      user_comparator_->Compare(user_start, tmp) < 0) {
    // User key has become shorter physically, but larger logically.
    // Tack on the earliest possible number to the shortened user key.
    PutFixed64(&tmp, PackSequenceAndType(kMaxSequenceNumber,kValueTypeForSeek));
    assert(this->Compare(*start, tmp) < 0);
    assert(this->Compare(tmp, limit) < 0);
    start->swap(tmp);
  }
}

void InternalKeyComparator::FindShortSuccessor(std::string* key) const {
  Slice user_key = ExtractUserKey(*key);
  std::string tmp(user_key.data(), user_key.size());
  user_comparator_->FindShortSuccessor(&tmp);
  if (tmp.size() < user_key.size() &&
      user_comparator_->Compare(user_key, tmp) < 0) {
    // User key has become shorter physically, but larger logically.
    // Tack on the earliest possible number to the shortened user key.
    PutFixed64(&tmp, PackSequenceAndType(kMaxSequenceNumber,kValueTypeForSeek));
    assert(this->Compare(*key, tmp) < 0);
    key->swap(tmp);
  }
}

const char* InternalFilterPolicy::Name() const {
  return user_policy_->Name();
}

void InternalFilterPolicy::CreateFilter(const Slice* keys, int n,
                                        std::string* dst) const {
  // We rely on the fact that the code in table.cc does not mind us
  // adjusting keys[].
  Slice* mkey = const_cast<Slice*>(keys);
  for (int i = 0; i < n; i++) {
    mkey[i] = ExtractUserKey(keys[i]);
    // TODO(sanjay): Suppress dups?
  }
  user_policy_->CreateFilter(keys, n, dst);
}

bool InternalFilterPolicy::KeyMayMatch(const Slice& key, const Slice& f) const {
  return user_policy_->KeyMayMatch(ExtractUserKey(key), f);
}

  LookupKey::LookupKey(const Slice& user_key, SequenceNumber s, KeyMetaData * meta) {
  meta_=meta;
  size_t usize = user_key.size();
  size_t needed = usize + 13;  // A conservative estimate
  char* dst;
  if (needed <= sizeof(space_)) {
    dst = space_;
  } else {
    dst = new char[needed];
  }
  start_ = dst;
  dst = EncodeVarint32(dst, usize + 8);
  kstart_ = dst;
  memcpy(dst, user_key.data(), usize);
  dst += usize;
  EncodeFixed64(dst, PackSequenceAndType(s, kValueTypeForSeek));
  dst += 8;
  end_ = dst;
}


KeyRetirement::KeyRetirement(
    const Comparator * Comparator,
    SequenceNumber SmallestSnapshot,
    const Options * Opts,
    Compaction * const Compaction)
    : has_current_user_key(false), last_sequence_for_key(kMaxSequenceNumber),
      user_comparator(Comparator), smallest_snapshot(SmallestSnapshot),
      options(Opts), compaction(Compaction),
      valid(false), dropped(0), expired(0)
{
    // NULL is ok for compaction
    valid=(NULL!=user_comparator);

    return;
}   // KeyRetirement::KeyRetirement


KeyRetirement::~KeyRetirement()
{
    if (0!=expired)
        gPerfCounters->Add(ePerfExpiredKeys, expired);
}   // KeyRetirement::~KeyRetirement


bool
KeyRetirement::operator()(
    Slice & key)
{
    ParsedInternalKey ikey;
    bool drop = false, expire_flag;

    if (valid)
    {
        if (!ParseInternalKey(key, &ikey))
        {
            // Do not hide error keys
            current_user_key.clear();
            has_current_user_key = false;
            last_sequence_for_key = kMaxSequenceNumber;
        }   // else
        else
        {
            if (!has_current_user_key ||
                user_comparator->Compare(ikey.user_key,
                                         Slice(current_user_key)) != 0)
            {
                // First occurrence of this user key
                current_user_key.assign(ikey.user_key.data(), ikey.user_key.size());
                has_current_user_key = true;
                last_sequence_for_key = kMaxSequenceNumber;
            }   // if

            if (last_sequence_for_key <= smallest_snapshot)
            {
                // Hidden by an newer entry for same user key
                drop = true;    // (A)
            }   // if

            else
            {
                expire_flag=false;
                if (NULL!=options && options->ExpiryActivated())
                    expire_flag=options->expiry_module->KeyRetirementCallback(ikey);

                if ((ikey.type == kTypeDeletion || expire_flag)
                    && ikey.sequence <= smallest_snapshot
                    && NULL!=compaction  // mem to level0 ignores this test
                    && compaction->IsBaseLevelForKey(ikey.user_key))
                {
                    // For this user key:
                    // (1) there is no data in higher levels
                    // (2) data in lower levels will have larger sequence numbers
                    // (3) data in layers that are being compacted here and have
                    //     smaller sequence numbers will be dropped in the next
                    //     few iterations of this loop (by rule (A) above).
                    // Therefore this deletion marker is obsolete and can be dropped.
                    drop = true;

                    if (expire_flag)
                        ++expired;
                    else
                        ++dropped;
                }   // if
            }   // else

            last_sequence_for_key = ikey.sequence;
        }   // else
    }   // if

#if 0
    // needs clean up to be used again
    Log(options_.info_log,
        "  Compact: %s, seq %d, type: %d %d, drop: %d, is_base: %d, "
        "%d smallest_snapshot: %d",
        ikey.user_key.ToString().c_str(),
        (int)ikey.sequence, ikey.type, kTypeValue, drop,
        compact->compaction->IsBaseLevelForKey(ikey.user_key),
        (int)last_sequence_for_key, (int)compact->smallest_snapshot);
#endif
    return(drop);

}   // KeyRetirement::operator(Slice & )


}  // namespace leveldb
