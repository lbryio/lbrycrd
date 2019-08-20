// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "leveldb/options.h"

#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/expiry.h"
#include "leveldb/filter_policy.h"
#include "util/cache2.h"
#include "util/crc32c.h"

#include "leveldb/expiry.h"

#if !defined(LEVELDB_VSN)
#define LEVELDB_VSN develop
#endif

#define XSTR(x) #x
#define STR(x) XSTR(x)

namespace leveldb {

Options::Options()
    : comparator(BytewiseComparator()),
      create_if_missing(false),
      error_if_exists(false),
      paranoid_checks(false),
      verify_compactions(true),
      env(Env::Default()),
      info_log(NULL),
      write_buffer_size(60<<20),
      max_open_files(1000),
      block_cache(NULL),
      block_size(4096),
      block_size_steps(16),
      block_restart_interval(16),
      compression(kLZ4Compression),
      filter_policy(NULL),
      is_repair(false),
      is_internal_db(false),
      total_leveldb_mem(2684354560ll),
      block_cache_threshold(32<<20),
      limited_developer_mem(false),
      mmap_size(0),
      delete_threshold(1000),
      fadvise_willneed(false),
      tiered_slow_level(0),
      cache_object_warming(true)
{

}


void
Options::Dump(
    Logger * log) const
{
    Log(log,"                       Version: %s %s", STR(LEVELDB_VSN), CompileOptionsString());
    Log(log,"            Options.comparator: %s", comparator->Name());
    Log(log,"     Options.create_if_missing: %d", create_if_missing);
    Log(log,"       Options.error_if_exists: %d", error_if_exists);
    Log(log,"       Options.paranoid_checks: %d", paranoid_checks);
    Log(log,"    Options.verify_compactions: %d", verify_compactions);
    Log(log,"                   Options.env: %p", env);
    Log(log,"              Options.info_log: %p", info_log);
    Log(log,"     Options.write_buffer_size: %zd", write_buffer_size);
    Log(log,"        Options.max_open_files: %d", max_open_files);
    Log(log,"           Options.block_cache: %p", block_cache);
    Log(log,"            Options.block_size: %zd", block_size);
    Log(log,"      Options.block_size_steps: %d", block_size_steps);
    Log(log,"Options.block_restart_interval: %d", block_restart_interval);
    Log(log,"           Options.compression: %d", compression);
    Log(log,"         Options.filter_policy: %s", filter_policy == NULL ? "NULL" : filter_policy->Name());
    Log(log,"             Options.is_repair: %s", is_repair ? "true" : "false");
    Log(log,"        Options.is_internal_db: %s", is_internal_db ? "true" : "false");
    Log(log,"     Options.total_leveldb_mem: %" PRIu64, total_leveldb_mem);
    Log(log," Options.block_cache_threshold: %" PRIu64, block_cache_threshold);
    Log(log," Options.limited_developer_mem: %s", limited_developer_mem ? "true" : "false");
    Log(log,"             Options.mmap_size: %" PRIu64, mmap_size);
    Log(log,"      Options.delete_threshold: %" PRIu64, delete_threshold);
    Log(log,"      Options.fadvise_willneed: %s", fadvise_willneed ? "true" : "false");
    Log(log,"     Options.tiered_slow_level: %d", tiered_slow_level);
    Log(log,"    Options.tiered_fast_prefix: %s", tiered_fast_prefix.c_str());
    Log(log,"    Options.tiered_slow_prefix: %s", tiered_slow_prefix.c_str());
    Log(log,"                        crc32c: %s", crc32c::IsHardwareCRC() ? "hardware" : "software");
    Log(log,"  Options.cache_object_warming: %s", cache_object_warming ? "true" : "false");
    Log(log,"       Options.ExpiryActivated: %s", ExpiryActivated() ? "true" : "false");

    if (NULL!=expiry_module.get())
        expiry_module->Dump(log);
    else
        Log(log,"         Options.expiry_module: NULL");

}   // Options::Dump

}  // namespace leveldb
