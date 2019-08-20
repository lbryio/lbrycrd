// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table.h"

#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "leveldb/perf_count.h"
#include "table/block.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"

namespace leveldb {

struct Table::Rep {
  ~Rep() {
    delete filter;
    delete [] filter_data;
    delete index_block;
  }

  Options options;
  Status status;
  RandomAccessFile* file;
  uint64_t file_size;
  uint64_t cache_id;
  FilterBlockReader* filter;
  const char* filter_data;
  size_t filter_data_size;

  BlockHandle metaindex_handle;  // Handle to metaindex_block: saved from footer
  Block* index_block;
  SstCounters sst_counters;
  BlockHandle filter_handle;
  const FilterPolicy * filter_policy;
  volatile uint32_t filter_flag;
};

Status Table::Open(const Options& options,
                   RandomAccessFile* file,
                   uint64_t size,
                   Table** table) {
  *table = NULL;
  if (size < Footer::kEncodedLength) {
    return Status::InvalidArgument("file is too short to be an sstable");
  }

  char footer_space[Footer::kEncodedLength];
  // stop valgrind uninitialize warning
  // let footer.DecodeFrom returned status do the talking for read of bad info
  memset(footer_space, 0, Footer::kEncodedLength);

  Slice footer_input;
  Status s = file->Read(size - Footer::kEncodedLength, Footer::kEncodedLength,
                        &footer_input, footer_space);
  if (!s.ok()) return s;

  Footer footer;
  s = footer.DecodeFrom(&footer_input);
  if (!s.ok()) return s;

  // Read the index block
  BlockContents contents;
  Block* index_block = NULL;
  if (s.ok()) {
    s = ReadBlock(file, ReadOptions(), footer.index_handle(), &contents);
    if (s.ok()) {
      index_block = new Block(contents);
    }
  }

  if (s.ok()) {
    // We've successfully read the footer and the index block: we're
    // ready to serve requests.
    Rep* rep = new Table::Rep;
    rep->options = options;
    rep->file = file;
    rep->file_size = size;
    rep->metaindex_handle = footer.metaindex_handle();
    rep->index_block = index_block;
    rep->cache_id = (options.block_cache ? options.block_cache->NewId() : 0);
    rep->filter_data = NULL;
    rep->filter_data_size = 0;
    rep->filter = NULL;
    rep->filter_policy = NULL;
    rep->filter_flag = 0;
    *table = new Table(rep);
    (*table)->ReadMeta(footer);
  } else {
    if (index_block) delete index_block;
  }

  return s;
}

void Table::ReadMeta(const Footer& footer) {

  // TODO(sanjay): Skip this if footer.metaindex_handle() size indicates
  // it is an empty block.
  std::string key;
  ReadOptions opt;
  BlockContents contents;

  if (!ReadBlock(rep_->file, opt, footer.metaindex_handle(), &contents).ok()) {
    // Do not propagate errors since meta info is not needed for operation
    return;
  }
  Block* meta = new Block(contents);

  Iterator* iter = meta->NewIterator(BytewiseComparator());

  // read filter only if policy set
  if (NULL != rep_->options.filter_policy) {
      bool found,first;
      const FilterPolicy * policy, * next;

      first=true;
      next=NULL;

      do
      {
          found=false;

          if (first)
          {
              policy=rep_->options.filter_policy;
              next=FilterInventory::ListHead;
              first=false;
          }   // if
          else
          {
              policy=next;
              if (NULL!=policy)
                  next=policy->GetNext();
              else
                  next=NULL;
          }   // else

          if (NULL!=policy)
          {
              key = "filter.";
              key.append(policy->Name());
              iter->Seek(key);
              if (iter->Valid() && iter->key() == Slice(key))
              {
                  // store information needed to load bloom filter
                  //  at a later time
                  Slice v = iter->value();
                  rep_->filter_handle.DecodeFrom(&v);
                  rep_->filter_policy = policy;

                  found=true;
              }   // if
          }   //if
      } while(!found && NULL!=policy);
  }   // if

  // always read counters
  key="stats.sst1";
  iter->Seek(key);
  if (iter->Valid() && iter->key() == Slice(key)) {
      ReadSstCounters(iter->value());
  }

  delete iter;
  delete meta;
}


// public version that reads filter at some time
//  after open ... true if filter read
bool
Table::ReadFilter()
{
    bool ret_flag;

    ret_flag=false;

    if (0!=rep_->filter_handle.size()
        && NULL!=rep_->filter_policy
        && 1 == inc_and_fetch(&rep_->filter_flag))
    {
        gPerfCounters->Inc(ePerfBlockFilterRead);

        ReadFilter(rep_->filter_handle, rep_->filter_policy);
        ret_flag=(NULL != rep_->filter);

        // only attempt the read once
        rep_->filter_handle.set_size(0);
    }   // if

    return(ret_flag);
}   // ReadFilter

// Private version of ReadFilter that does the actual work
void
Table::ReadFilter(
    BlockHandle & filter_handle,
    const FilterPolicy * policy)
{
  // We might want to unify with ReadBlock() if we start
  // requiring checksum verification in Table::Open.
  ReadOptions opt;
  BlockContents block;
  if (!ReadBlock(rep_->file, opt, filter_handle, &block).ok()) {
    return;
  }
  if (block.heap_allocated) {
    rep_->filter_data = block.data.data();     // Will need to delete later
    rep_->filter_data_size = block.data.size();
  }

  rep_->filter = new FilterBlockReader(policy, block.data);
}


void Table::ReadSstCounters(const Slice& sst_counters_handle_value) {
  Slice v = sst_counters_handle_value;
  BlockHandle counters_handle;
  if (!counters_handle.DecodeFrom(&v).ok()) {
    return;
  }

  // We might want to unify with ReadBlock() if we start
  // requiring checksum verification in Table::Open.
  ReadOptions opt;
  BlockContents block;
  if (!ReadBlock(rep_->file, opt, counters_handle, &block).ok()) {
    return;
  }
  if (block.heap_allocated) {
    rep_->sst_counters.DecodeFrom(block.data);
    delete [] block.data.data();
  }

}

SstCounters Table::GetSstCounters() const
{
    return(rep_->sst_counters);
}   // Table::GetSstCounters


Table::~Table() {
  delete rep_;
}

static void DeleteBlock(void* arg, void* ignored) {
  delete reinterpret_cast<Block*>(arg);
}

static void DeleteCachedBlock(const Slice& key, void* value) {
  Block* block = reinterpret_cast<Block*>(value);
  delete block;
}

static void ReleaseBlock(void* arg, void* h) {
  Cache* cache = reinterpret_cast<Cache*>(arg);
  Cache::Handle* handle = reinterpret_cast<Cache::Handle*>(h);
  cache->Release(handle);
}

// Convert an index iterator value (i.e., an encoded BlockHandle)
// into an iterator over the contents of the corresponding block.
Iterator* Table::BlockReader(void* arg,
                             const ReadOptions& options,
                             const Slice& index_value) {
  Table* table = reinterpret_cast<Table*>(arg);
  Cache* block_cache = table->rep_->options.block_cache;
  Block* block = NULL;
  Cache::Handle* cache_handle = NULL;

  BlockHandle handle;
  Slice input = index_value;
  Status s = handle.DecodeFrom(&input);
  // We intentionally allow extra stuff in index_value so that we
  // can add more features in the future.

  if (s.ok()) {
    BlockContents contents;
    if (block_cache != NULL) {
      char cache_key_buffer[16];
      EncodeFixed64(cache_key_buffer, table->rep_->cache_id);
      EncodeFixed64(cache_key_buffer+8, handle.offset());
      Slice key(cache_key_buffer, sizeof(cache_key_buffer));
      cache_handle = block_cache->Lookup(key);
      if (cache_handle != NULL) {
        block = reinterpret_cast<Block*>(block_cache->Value(cache_handle));
        gPerfCounters->Inc(ePerfBlockCached);
      } else {
        s = ReadBlock(table->rep_->file, options, handle, &contents);
        gPerfCounters->Inc(ePerfBlockRead);
        if (s.ok()) {
          block = new Block(contents);
          if (contents.cachable && options.fill_cache) {
            cache_handle = block_cache->Insert(
                key, block,
                (block->size() + /*block_cache->EntryOverheadSize() +*/ sizeof(cache_key_buffer)),
                &DeleteCachedBlock);
          }
        }
      }
    } else {
      s = ReadBlock(table->rep_->file, options, handle, &contents);
        gPerfCounters->Inc(ePerfBlockRead);
      if (s.ok()) {
        block = new Block(contents);
      }
    }
  }

  Iterator* iter;
  if (block != NULL) {
    iter = block->NewIterator(table->rep_->options.comparator);
    if (cache_handle == NULL) {
      iter->RegisterCleanup(&DeleteBlock, block, NULL);
    } else {
      iter->RegisterCleanup(&ReleaseBlock, block_cache, cache_handle);
    }
  } else {
    iter = NewErrorIterator(s);
  }
  return iter;
}

Iterator* Table::NewIterator(const ReadOptions& options) const {
  return NewTwoLevelIterator(
      rep_->index_block->NewIterator(rep_->options.comparator),
      &Table::BlockReader, const_cast<Table*>(this), options);
}

Status Table::InternalGet(const ReadOptions& options, const Slice& k,
                          void* arg,
                          bool (*saver)(void*, const Slice&, const Slice&)) {
  Status s;
  Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
  iiter->Seek(k);
  if (iiter->Valid()) {
    Slice handle_value = iiter->value();
    FilterBlockReader* filter = rep_->filter;
    BlockHandle handle;
    if (filter != NULL &&
        handle.DecodeFrom(&handle_value).ok() &&
        !filter->KeyMayMatch(handle.offset(), k)) {
      // Not found
        gPerfCounters->Inc(ePerfBlockFiltered);
    } else {
      Iterator* block_iter = BlockReader(this, options, iiter->value());
      block_iter->Seek(k);
      if (block_iter->Valid()) {
        bool match;
        match=(*saver)(arg, block_iter->key(), block_iter->value());
        if (!match && NULL!=filter)
            gPerfCounters->Inc(ePerfBlockFilterFalse);
        if (match)
            gPerfCounters->Inc(ePerfBlockValidGet);
      }

      s = block_iter->status();
      delete block_iter;
    }
  }
  if (s.ok()) {
    s = iiter->status();
  }
  delete iiter;
  return s;
}


uint64_t Table::ApproximateOffsetOf(const Slice& key) const {
  Iterator* index_iter =
      rep_->index_block->NewIterator(rep_->options.comparator);
  index_iter->Seek(key);
  uint64_t result;
  if (index_iter->Valid()) {
    BlockHandle handle;
    Slice input = index_iter->value();
    Status s = handle.DecodeFrom(&input);
    if (s.ok()) {
      result = handle.offset();
    } else {
      // Strange: we can't decode the block handle in the index block.
      // We'll just return the offset of the metaindex block, which is
      // close to the whole file size for this case.
      result = rep_->metaindex_handle.offset();
    }
  } else {
    // key is past the last key in the file.  Approximate the offset
    // by returning the offset of the metaindex block (which is
    // right near the end of the file).
    result = rep_->metaindex_handle.offset();
  }
  delete index_iter;
  return result;
}


uint64_t
Table::GetFileSize()
{
    return(rep_->file_size);
};

Block *
Table::TEST_GetIndexBlock() {return(rep_->index_block);};

// Riak specific routine.  Calculates total footprint of an open
//  table in memory.
size_t
Table::TableObjectSize()
{
    return(sizeof(Table) + sizeof(Table::Rep) + rep_->index_block->size() + rep_->filter_data_size + rep_->file->ObjectSize()
           + sizeof(FilterBlockReader) + sizeof(Block));
};

size_t
Table::TEST_FilterDataSize() {return(rep_->filter_data_size);};


}  // namespace leveldb
