// -------------------------------------------------------------------
//
// throttle.h
//
// Copyright (c) 2011-2013 Basho Technologies, Inc. All Rights Reserved.
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

#include <pthread.h>


namespace leveldb
{

void ThrottleInit();

void SetThrottleWriteRate(uint64_t Micros, uint64_t Keys, bool IsLevel0);

uint64_t GetThrottleWriteRate();
uint64_t GetUnadjustedThrottleWriteRate();

// clock_gettime but only updated once every 60 seconds (roughly)
//  (SetCachedTimeMicros() intended for unit tests)
uint64_t GetCachedTimeMicros();
void SetCachedTimeMicros(uint64_t);

// step 1 in two step shutdown
void ThrottleStopThreads();

// step 2 in two step shutdown
void ThrottleClose();

}  // namespace leveldb
