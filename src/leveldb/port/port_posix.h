// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// See port_example.h for documentation for the following types/functions.

#ifndef STORAGE_LEVELDB_PORT_PORT_POSIX_H_
#define STORAGE_LEVELDB_PORT_PORT_POSIX_H_

// to properly pull in bits/posix_opt.h on Linux
#include <unistd.h>
#include <assert.h>

#if _POSIX_TIMERS >= 200801L
   #include <time.h> // declares clock_gettime()
#else
   #include <sys/time.h> // declares gettimeofday()
#endif

#undef PLATFORM_IS_LITTLE_ENDIAN
#if defined(OS_MACOSX)
  #include <machine/endian.h>
  #if defined(__DARWIN_LITTLE_ENDIAN) && defined(__DARWIN_BYTE_ORDER)
    #define PLATFORM_IS_LITTLE_ENDIAN \
        (__DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN)
  #endif
#elif defined(OS_SOLARIS)
  #include <sys/isa_defs.h>
  #ifdef _LITTLE_ENDIAN
    #define PLATFORM_IS_LITTLE_ENDIAN true
  #else
    #define PLATFORM_IS_LITTLE_ENDIAN false
  #endif
#elif defined(OS_FREEBSD) || defined(OS_OPENBSD) || defined(OS_NETBSD) ||\
      defined(OS_DRAGONFLYBSD) || defined(OS_ANDROID)
  #include <sys/types.h>
  #include <sys/endian.h>

  #if !defined(PLATFORM_IS_LITTLE_ENDIAN) && defined(_BYTE_ORDER)
    #define PLATFORM_IS_LITTLE_ENDIAN (_BYTE_ORDER == _LITTLE_ENDIAN)
  #endif
#else
  #include <endian.h>
#endif
#include <pthread.h>
#ifdef SNAPPY
#include <snappy.h>
#endif
#include <stdint.h>
#include <string>
#include "port/atomic_pointer.h"

#ifndef PLATFORM_IS_LITTLE_ENDIAN
#define PLATFORM_IS_LITTLE_ENDIAN (__BYTE_ORDER == __LITTLE_ENDIAN)
#endif

#if defined(OS_MACOSX) || defined(OS_SOLARIS) || defined(OS_FREEBSD) ||\
    defined(OS_NETBSD) || defined(OS_OPENBSD) || defined(OS_DRAGONFLYBSD) ||\
    defined(OS_ANDROID)
// Use fread/fwrite/fflush on platforms without _unlocked variants
#define fread_unlocked fread
#define fwrite_unlocked fwrite
#define fflush_unlocked fflush
#endif

#if defined(OS_MACOSX) || defined(OS_FREEBSD) ||\
    defined(OS_OPENBSD) || defined(OS_DRAGONFLYBSD)
// Use fsync() on platforms without fdatasync()
#define fdatasync fsync
#endif

// Some compilers do not provide access to nested classes of a declared friend class
// Defining PUBLIC_NESTED_FRIEND_ACCESS will cause those declarations to be made
// public as a workaround.  Added by David Smith, Basho.
#if defined(OS_MACOSX) || defined(OS_SOLARIS)
#define USED_BY_NESTED_FRIEND(a) public: a; private:
#define USED_BY_NESTED_FRIEND2(a,b) public: a,b; private:
#else
#define USED_BY_NESTED_FRIEND(a) a;
#define USED_BY_NESTED_FRIEND2(a,b) a,b;
#endif

#if defined(OS_ANDROID) && __ANDROID_API__ < 9
// fdatasync() was only introduced in API level 9 on Android. Use fsync()
// when targetting older platforms.
#define fdatasync fsync
#endif

namespace leveldb {
namespace port {

static const bool kLittleEndian = PLATFORM_IS_LITTLE_ENDIAN;
#undef PLATFORM_IS_LITTLE_ENDIAN

class CondVar;

class Mutex {
 public:
  Mutex(bool recursive=false); // true => creates a mutex that can be locked recursively
  ~Mutex();

  void Lock();
  void Unlock();
  void AssertHeld() {assert(0!=pthread_mutex_trylock(&mu_));}

 private:
  friend class CondVar;
  pthread_mutex_t mu_;

  // No copying
  Mutex(const Mutex&);
  void operator=(const Mutex&);
};


#if defined(_POSIX_SPIN_LOCKS) && 0<_POSIX_SPIN_LOCKS
class Spin {
 public:
  Spin();
  ~Spin();

  void Lock();
  void Unlock();
  void AssertHeld() {assert(0!=pthread_spin_trylock(&sp_));}

 private:
  friend class CondVar;
  pthread_spinlock_t sp_;

  // No copying
  Spin(const Spin&);
  void operator=(const Spin&);
};
#else
typedef Mutex Spin;
#endif


class CondVar {
 public:
  explicit CondVar(Mutex* mu);
  ~CondVar();
  void Wait();

  // waits on the condition variable until the specified time is reached
  bool // true => the condition variable was signaled, else timed out
  Wait(struct timespec* pTimespec);

  void Signal();
  void SignalAll();
 private:
  pthread_cond_t cv_;
  Mutex* mu_;
};

typedef pthread_once_t OnceType;
#define LEVELDB_ONCE_INIT PTHREAD_ONCE_INIT
extern void InitOnce(OnceType* once, void (*initializer)());


class RWMutex {
 public:
  RWMutex();
  ~RWMutex();

  void ReadLock();
  void WriteLock();
  void Unlock();
  void AssertHeld() { }

 private:
  pthread_rwlock_t mu_;

  // No copying
  RWMutex(const RWMutex&);
  void operator=(const RWMutex&);

};


inline bool Snappy_Compress(const char* input, size_t length,
                            ::std::string* output) {
#ifdef SNAPPY
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#endif

  return false;
}

inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                         size_t* result) {
#ifdef SNAPPY
  return snappy::GetUncompressedLength(input, length, result);
#else
  return false;
#endif
}

inline bool Snappy_Uncompress(const char* input, size_t length,
                              char* output) {
#ifdef SNAPPY
  return snappy::RawUncompress(input, length, output);
#else
  return false;
#endif
}

inline bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg) {
  return false;
}

// sets the name of the current thread
inline void SetCurrentThreadName(const char* threadName) {
  if (NULL == threadName) {
    threadName = "";
  }
#if defined(OS_MACOSX)
  pthread_setname_np(threadName);
//#elif defined(OS_LINUX)
#elif defined(__GLIBC__)
#if  __GLIBC_PREREQ(2,12)
  pthread_setname_np(pthread_self(), threadName);
#endif
#elif defined(OS_NETBSD)
  pthread_setname_np(pthread_self(), threadName, NULL);
#else
  // we have some other platform(s) to support
  //   defined(OS_FREEBSD) ... freebsd-9.2, Feb 19, 2016 not working
  //
  // NOTE: do not fail here since this functionality is optional
#endif
}

// similar to Env::NowMicros except guaranteed to return "time" instead
//  of potentially only ticks since reboot
const uint64_t UINT64_ONE_SECOND_MICROS=1000000;

inline uint64_t TimeMicros() {
#if _POSIX_TIMERS >= 200801L
    struct timespec ts;

    // this is rumored to be faster than gettimeofday(),
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000 + ts.tv_nsec/1000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif
} // TimeMicros

} // namespace port
} // namespace leveldb

#endif  // STORAGE_LEVELDB_PORT_PORT_POSIX_H_
