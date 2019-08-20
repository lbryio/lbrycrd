// -------------------------------------------------------------------
//
// cache_warm.cc
//
// Copyright (c) 2011-2016 Basho Technologies, Inc. All Rights Reserved.
//
// This file is provided to you under the Apache License,
// Version 2.0 (the "License"); you may not use this file
// except in compliance with the License.  You may obtain
// a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//
// -------------------------------------------------------------------

#include "db/table_cache.h"

namespace leveldb {


/**
 * Riak specific routine to push list of open files to disk
 */
Status
TableCache::SaveOpenFileList()
{
    return(Status::OK());
}   // TableCache::SaveOpenFiles


/**
 * Riak specific routine to read list of previously open files
 *  and preload them into the table cache
 */
Status
TableCache::PreloadTableCache()
{
    return(Status::OK());
}   // TableCache::PreloadTableCache

}  // namespace leveldb
