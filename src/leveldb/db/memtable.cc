// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/memtable.h"
#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/expiry.h"
#include "leveldb/iterator.h"
#include "util/coding.h"

namespace leveldb {

static Slice GetLengthPrefixedSlice(const char* data) {
  uint32_t len;
  const char* p = data;
  p = GetVarint32Ptr(p, p + 5, &len);  // +5: we assume "p" is not corrupted
  return Slice(p, len);
}

MemTable::MemTable(const InternalKeyComparator& cmp)
    : comparator_(cmp),
      refs_(0),
      table_(comparator_, &arena_) {
}

MemTable::~MemTable() {
  assert(refs_ == 0);
}

size_t MemTable::ApproximateMemoryUsage() { return arena_.MemoryUsage(); }

int MemTable::KeyComparator::operator()(const char* aptr, const char* bptr)
    const {
  // Internal keys are encoded as length-prefixed strings.
  Slice a = GetLengthPrefixedSlice(aptr);
  Slice b = GetLengthPrefixedSlice(bptr);
  return comparator.Compare(a, b);
}

// Encode a suitable internal key target for "target" and return it.
// Uses *scratch as scratch space, and the returned pointer will point
// into this scratch space.
static const char* EncodeKey(std::string* scratch, const Slice& target) {
  scratch->clear();
  PutVarint32(scratch, target.size());
  scratch->append(target.data(), target.size());
  return scratch->data();
}

class MemTableIterator: public Iterator {
 public:
  explicit MemTableIterator(MemTable::Table* table) : iter_(table) { }

  virtual bool Valid() const { return iter_.Valid(); }
  virtual void Seek(const Slice& k) { iter_.Seek(EncodeKey(&tmp_, k)); }
  virtual void SeekToFirst() { iter_.SeekToFirst(); }
  virtual void SeekToLast() { iter_.SeekToLast(); }
  virtual void Next() { iter_.Next(); }
  virtual void Prev() { iter_.Prev(); }
  virtual Slice key() const { return GetLengthPrefixedSlice(iter_.key()); }
  virtual Slice value() const {
    Slice key_slice = GetLengthPrefixedSlice(iter_.key());
    return GetLengthPrefixedSlice(key_slice.data() + key_slice.size());
  }
  virtual KeyMetaData & keymetadata() const
   {MemTable::DecodeKeyMetaData(iter_.key(), keymetadata_); return(keymetadata_);};

  virtual Status status() const { return Status::OK(); }

 private:
  MemTable::Table::Iterator iter_;
  std::string tmp_;       // For passing to EncodeKey

  // No copying allowed
  MemTableIterator(const MemTableIterator&);
  void operator=(const MemTableIterator&);
};

Iterator* MemTable::NewIterator() {
  return new MemTableIterator(&table_);
}

void MemTable::Add(SequenceNumber s, ValueType type,
                   const Slice& key,
                   const Slice& value,
                   const ExpiryTimeMicros & expiry) {
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  size_t val_size = value.size();
  size_t internal_key_size = key_size + KeySuffixSize(type);
  const size_t encoded_len =
      VarintLength(internal_key_size) + internal_key_size +
      VarintLength(val_size) + val_size;
  char* buf = arena_.Allocate(encoded_len);
  char* p = EncodeVarint32(buf, internal_key_size);
  memcpy(p, key.data(), key_size);
  p += key_size;
  if (IsExpiryKey(type))
  {
      EncodeFixed64(p, expiry);
      p+=8;
  }
  EncodeFixed64(p, (s << 8) | type);
  p += 8;
  p = EncodeVarint32(p, val_size);
  memcpy(p, value.data(), val_size);
  assert((size_t)((p + val_size) - buf) == encoded_len);
  table_.Insert(buf);
}

bool MemTable::Get(const LookupKey& key, Value* value, Status* s,
    const Options * options) {
  bool ret_flag(false);
  Slice memkey = key.memtable_key();
  Table::Iterator iter(&table_);
  iter.Seek(memkey.data());
  if (iter.Valid()) {
    // entry format is:
    //    klength  varint32
    //    userkey  char[klength]
    //    optional uint64
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter.key();
    uint32_t key_length;
    const char* key_ptr = GetVarint32Ptr(entry, entry+5, &key_length);
    Slice internal_key(key_ptr, key_length);
    if (comparator_.comparator.user_comparator()->Compare(
            ExtractUserKey(internal_key),
            key.user_key()) == 0) {
      // Correct user key
      KeyMetaData meta;
      DecodeKeyMetaData(entry, meta);

      switch (meta.m_Type) {
        case kTypeValueWriteTime:
        case kTypeValueExplicitExpiry:
        {
            bool expired=false;
            if (NULL!=options && options->ExpiryActivated())
                expired=options->expiry_module->MemTableCallback(internal_key);
            if (expired)
            {
                // like kTypeDeletion
                *s = Status::NotFound(Slice());
                ret_flag=true;
                break;
            }   // if
            //otherwise fall into kTypeValue code
        }   // case

        case kTypeValue:
        {
          Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
          value->assign(v.data(), v.size());
          ret_flag=true;
          break;
        }
        case kTypeDeletion:
          *s = Status::NotFound(Slice());
          ret_flag=true;
          break;
      } // switch

      // only unpack metadata if requested
      if (key.WantsKeyMetaData())
          key.SetKeyMetaData(meta);
    }
  }
  return ret_flag;
}

// this is a static function
void MemTable::DecodeKeyMetaData(
    const char * key,
    KeyMetaData & meta)
{
    Slice key_slice = GetLengthPrefixedSlice(key);

    meta.m_Type=ExtractValueType(key_slice);
    meta.m_Sequence=ExtractSequenceNumber(key_slice);
    if (IsExpiryKey(meta.m_Type))
        meta.m_Expiry=ExtractExpiry(key_slice);
    else
        meta.m_Expiry=0;

} // DecodeKeyMetaData

}  // namespace leveldb
