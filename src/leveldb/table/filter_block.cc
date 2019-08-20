// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"

namespace leveldb {

// See doc/table_format.txt for an explanation of the filter block format.

// list of available filters within code base
const FilterPolicy * FilterInventory::ListHead(NULL);

FilterBlockBuilder::FilterBlockBuilder(const FilterPolicy* policy)
    : policy_(policy), filter_base_lg_(0), filter_base_(0), last_offset_(0)
{
}

void FilterBlockBuilder::StartBlock(uint64_t block_offset) {
    if (0==filter_base_lg_ && (1500<start_.size() || 268435456<block_offset))
        PickFilterBase(block_offset);

    if (0!=filter_base_lg_)
    {
        uint64_t filter_index = (block_offset / filter_base_);
        assert(filter_index >= filter_offsets_.size());
        while (filter_index > filter_offsets_.size())
        {
            GenerateFilter();
        }   // if
    }   // if

    last_offset_=block_offset;
}

void FilterBlockBuilder::AddKey(const Slice& key) {
  Slice k = key;
  start_.push_back(keys_.size());
  keys_.append(k.data(), k.size());
}

Slice FilterBlockBuilder::Finish() {
    if (0==filter_base_lg_)
        PickFilterBase(last_offset_);

  if (!start_.empty()) {
    GenerateFilter();
  }

  // Append array of per-filter offsets
  const uint32_t array_offset = result_.size();
  for (size_t i = 0; i < filter_offsets_.size(); i++) {
    PutFixed32(&result_, filter_offsets_[i]);
  }

  PutFixed32(&result_, array_offset);
  result_.push_back(filter_base_lg_);  // Save encoding parameter in result
  return Slice(result_);
}

void FilterBlockBuilder::GenerateFilter() {
  const size_t num_keys = start_.size();
  if (num_keys == 0) {
    // Fast path if there are no keys for this filter
    filter_offsets_.push_back(result_.size());
    return;
  }

  // Make list of keys from flattened key structure
  start_.push_back(keys_.size());  // Simplify length computation
  tmp_keys_.resize(num_keys);
  for (size_t i = 0; i < num_keys; i++) {
    const char* base = keys_.data() + start_[i];
    size_t length = start_[i+1] - start_[i];
    tmp_keys_[i] = Slice(base, length);
  }

  // Generate filter for current set of keys and append to result_.
  filter_offsets_.push_back(result_.size());
  policy_->CreateFilter(&tmp_keys_[0], num_keys, &result_);

  tmp_keys_.clear();
  keys_.clear();
  start_.clear();
}

FilterBlockReader::FilterBlockReader(const FilterPolicy* policy,
                                     const Slice& contents)
    : policy_(policy),
      data_(NULL),
      offset_(NULL),
      num_(0),
      base_lg_(0) {
  size_t n = contents.size();
  if (n < 5) return;  // 1 byte for base_lg_ and 4 for start of offset array
  base_lg_ = contents[n-1];
  uint32_t last_word = DecodeFixed32(contents.data() + n - 5);
  if (last_word > n - 5) return;
  data_ = contents.data();
  offset_ = data_ + last_word;
  num_ = (n - 5 - last_word) / 4;
}

bool FilterBlockReader::KeyMayMatch(uint64_t block_offset, const Slice& key) {
  uint64_t index = block_offset >> base_lg_;
  if (index < num_) {
    uint32_t start = DecodeFixed32(offset_ + index*4);
    uint32_t limit = DecodeFixed32(offset_ + index*4 + 4);
    if (start <= limit && limit <= (offset_ - data_)) {
      Slice filter = Slice(data_ + start, limit - start);
      return policy_->KeyMayMatch(key, filter);
    } else if (start == limit) {
      // Empty filters do not match any keys
      return false;
    }
  }
  return true;  // Errors are treated as potential matches
}


// wikipedia.com quotes following as source
//  Warren Jr., Henry S. (2002). Hacker's Delight. Addison Wesley. pp. 48. ISBN 978-0-201-91465-8
// Numerical Recipes, Third Edition credits
//   Anderson, S.E. 2001, "BitTwiddling Hacks", http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// latter states public domain.
static uint32_t
PowerOfTwoGreater(uint32_t num)
{
    uint32_t n;

    n=num;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    ++n;

    return n;
}   // CalcFilterBaseLg


void
FilterBlockBuilder::PickFilterBase(
    size_t BlockOffset)
{
    // create limits just for safety sake
    if (0==BlockOffset || 268435456<BlockOffset)
    {
        filter_base_lg_=28;
        filter_base_=268435456;
    }   // if
    else
    {
        uint32_t temp;
        filter_base_=PowerOfTwoGreater((uint32_t)BlockOffset);
        for (filter_base_lg_=0, temp=filter_base_>>1; 0!=temp; ++filter_base_lg_, temp=temp >> 1);
    }   // else

}   // FilterBlockBuilder::PickFilterBase


}
