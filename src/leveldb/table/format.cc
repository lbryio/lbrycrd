// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/format.h"

#include "leveldb/env.h"
#include "leveldb/perf_count.h"
#include "port/port.h"
#include "table/block.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/lz4.h"
#include "db/log_writer.h"

namespace leveldb {

static struct
{
    uint32_t filler_;  //!< don't know and don't care
    uint32_t zero_restarts_;  //!< path to an EmptyIterator
} gEmptyBlock={0,0};


void BlockHandle::EncodeTo(std::string* dst) const {
  // Sanity check that all fields have been set
  assert(offset_ != ~static_cast<uint64_t>(0));
  assert(size_ != ~static_cast<uint64_t>(0));
  PutVarint64(dst, offset_);
  PutVarint64(dst, size_);
}

Status BlockHandle::DecodeFrom(Slice* input) {
  if (GetVarint64(input, &offset_) &&
      GetVarint64(input, &size_)) {
    return Status::OK();
  } else {
    return Status::Corruption("bad block handle");
  }
}

void Footer::EncodeTo(std::string* dst) const {
#ifndef NDEBUG
  const size_t original_size = dst->size();
#endif
  metaindex_handle_.EncodeTo(dst);
  index_handle_.EncodeTo(dst);
  dst->resize(2 * BlockHandle::kMaxEncodedLength);  // Padding
  PutFixed32(dst, static_cast<uint32_t>(kTableMagicNumber & 0xffffffffu));
  PutFixed32(dst, static_cast<uint32_t>(kTableMagicNumber >> 32));
  assert(dst->size() == original_size + kEncodedLength);
}

Status Footer::DecodeFrom(Slice* input) {
  const char* magic_ptr = input->data() + kEncodedLength - 8;
  const uint32_t magic_lo = DecodeFixed32(magic_ptr);
  const uint32_t magic_hi = DecodeFixed32(magic_ptr + 4);
  const uint64_t magic = ((static_cast<uint64_t>(magic_hi) << 32) |
                          (static_cast<uint64_t>(magic_lo)));
  if (magic != kTableMagicNumber) {
    return Status::InvalidArgument("not an sstable (bad magic number)");
  }

  Status result = metaindex_handle_.DecodeFrom(input);
  if (result.ok()) {
    result = index_handle_.DecodeFrom(input);
  }
  if (result.ok()) {
    // We skip over any leftover data (just padding for now) in "input"
    const char* end = magic_ptr + 8;
    *input = Slice(end, input->data() + input->size() - end);
  }
  return result;
}

Status ReadBlock(RandomAccessFile* file,
                 const ReadOptions& options,
                 const BlockHandle& handle,
                 BlockContents* result)
{
  char * buf, * ubuf;
  const char * data;

  buf=NULL;
  ubuf=NULL;
  data=NULL;
  result->data = Slice();
  result->cachable = false;
  result->heap_allocated = false;

  // Read the block contents as well as the type/crc footer.
  // See table_builder.cc for the code that built this structure.
  size_t n = static_cast<size_t>(handle.size());
  buf = new char[n + kBlockTrailerSize];
  Slice contents;
  Status s = file->Read(handle.offset(), n + kBlockTrailerSize, &contents, buf);
  if (s.ok())
  {
      if (contents.size() != n + kBlockTrailerSize) {
          s=Status::Corruption("truncated block read");
      }
  }   // if

  // Check the crc of the type and the block contents
  if (s.ok())
  {
      data = contents.data();    // Pointer to where Read put the data
      if (options.verify_checksums) {
          const uint32_t crc = crc32c::Unmask(DecodeFixed32(data + n + 1));
          const uint32_t actual = crc32c::Value(data, n + 1);
          if (actual != crc) {
              s = Status::Corruption("block checksum mismatch");
          }   // if
      }   // if
  }   // if

  if (s.ok())
  {
      switch (data[n]) {
          case kNoCompression:
              if (data != buf) {
                  // File implementation gave us pointer to some other data.
                  // Use it directly under the assumption that it will be live
                  // while the file is open.
                  delete[] buf;
                  buf=NULL;
                  result->data = Slice(data, n);
                  result->heap_allocated = false;
                  result->cachable = false;  // Do not double-cache
              } else {
                  result->data = Slice(buf, n);
                  result->heap_allocated = true;
                  result->cachable = true;
              }   // else
              // Ok
              break;

          case kSnappyCompression: {
              size_t ulength = 0;
              if (!port::Snappy_GetUncompressedLength(data, n, &ulength)) {
                  s = Status::Corruption("corrupted compressed block contents");
              }

              if (s.ok())
              {
                  ubuf = new char[ulength];
                  if (!port::Snappy_Uncompress(data, n, ubuf)) {
                      s=Status::Corruption("corrupted compressed block contents");
                  }
              }   // if

              if (s.ok())
              {
                  delete[] buf;
                  buf=NULL;
                  result->data = Slice(ubuf, ulength);
                  result->heap_allocated = true;
                  result->cachable = true;
              }   // if
              break;
          }

          case kLZ4Compression: {
              size_t ulength = DecodeFixed32(data);
              size_t ret_val;
              ubuf = new char[ulength];

              ret_val=LZ4_decompress_safe(data+4, ubuf, n-4, ulength);
              if (ret_val != ulength)
              {
                  s=Status::Corruption("corrupted LZ4 compressed block");
              }   // if

              if (s.ok())
              {
                  delete[] buf;
                  buf=NULL;
                  result->data = Slice(ubuf, ulength);
                  result->heap_allocated = true;
                  result->cachable = true;
              }   // if
              break;
          }

          default:
              s=Status::Corruption("bad block type");
              break;
      }   // switch
  }   // if

  // clean up error and decide what to do with it
  if (!s.ok())
  {
      gPerfCounters->Inc(ePerfReadBlockError);

      if (options.IsCompaction() && 0!=options.GetDBName().length())
      {
          // this process is slow.  assumption is that it does not happen often.
          if (NULL!=data)
          {
              std::string new_name;
              WritableFile *bad_file;
              log::Writer *bad_logger;
              Status s2;

              bad_file=NULL;
              bad_logger=NULL;

              // potentially create the "lost" directory.  It might already exist.
              new_name=options.GetDBName();
              new_name+="/lost";
              options.GetEnv()->CreateDir(new_name);

              // create / append file to hold removed blocks
              new_name+="/BLOCKS.bad";
              s2=options.GetEnv()->NewAppendableFile(new_name, &bad_file, 4*1024);
              if (s2.ok())
              {
                  // need a try/catch
                  bad_logger=new log::Writer(bad_file);
                  bad_logger->AddRecord(Slice(data, n));
                  Log(options.GetInfoLog(),"Moving corrupted block to lost/BLOCKS.bad (size %zd)", n);

                  // Close also deletes bad_file
                  bad_logger->Close();
                  delete bad_logger;
                  bad_logger=NULL;
                  bad_file=NULL;
              }   // if
              else
              {
                  Log(options.GetInfoLog(), "Unable to create file for bad/corrupted blocks: %s", new_name.c_str());
              }   // else
          }   // if

          // lie to the upper layers to keep compaction from going into an infinite loop
          s = Status::OK();
      }   // if

      delete [] buf;
      delete [] ubuf;

      result->data = Slice((char *)&gEmptyBlock, sizeof(gEmptyBlock));
      result->cachable = false;
      result->heap_allocated = false;
  }   // if

  return s;
}

}  // namespace leveldb
