// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port/port_posix.h"

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "leveldb/env.h"
#include "util/logging.h"

namespace leveldb {
namespace port {

static void PthreadCall(const char* label, int result) {
  if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    Log(NULL, "pthread %s: %s\n", label, strerror(result));
    abort();
  }
}

Mutex::Mutex(bool recursive) {
  if (recursive) {
    pthread_mutexattr_t attr;

    PthreadCall("init mutex attr", pthread_mutexattr_init(&attr));
    PthreadCall("set mutex recursive", pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
    PthreadCall("init recursive mutex", pthread_mutex_init(&mu_, &attr));
    PthreadCall("destroy mutex attr", pthread_mutexattr_destroy(&attr));
  }
  else {
    PthreadCall("init mutex", pthread_mutex_init(&mu_, NULL));
  }
}

Mutex::~Mutex() { PthreadCall("destroy mutex", pthread_mutex_destroy(&mu_)); }

void Mutex::Lock() { PthreadCall("lock", pthread_mutex_lock(&mu_)); }

void Mutex::Unlock() { PthreadCall("unlock", pthread_mutex_unlock(&mu_)); }

#if defined(_POSIX_SPIN_LOCKS) && 0<_POSIX_SPIN_LOCKS
Spin::Spin() { PthreadCall("init spinlock", pthread_spin_init(&sp_, PTHREAD_PROCESS_PRIVATE)); }

Spin::~Spin() { PthreadCall("destroy spinlock", pthread_spin_destroy(&sp_)); }

void Spin::Lock() { PthreadCall("lock spin", pthread_spin_lock(&sp_)); }

void Spin::Unlock() { PthreadCall("unlock spin", pthread_spin_unlock(&sp_)); }
#endif

CondVar::CondVar(Mutex* mu)
    : mu_(mu) {
    PthreadCall("init cv", pthread_cond_init(&cv_, NULL));
}

CondVar::~CondVar() { PthreadCall("destroy cv", pthread_cond_destroy(&cv_)); }

void CondVar::Wait() {
  PthreadCall("wait", pthread_cond_wait(&cv_, &mu_->mu_));
}

bool CondVar::Wait(struct timespec* pTimespec) {
  bool signaled = true;
  int result = pthread_cond_timedwait(&cv_, &mu_->mu_, pTimespec);
  if (0 != result) {
    signaled = false;

    // the only expected errno is ETIMEDOUT; anything else is a real error
    if (ETIMEDOUT != result) {
      PthreadCall("timed wait", result);
    }
  }
  return signaled;
}

void CondVar::Signal() {
  PthreadCall("signal", pthread_cond_signal(&cv_));
}

void CondVar::SignalAll() {
  PthreadCall("broadcast", pthread_cond_broadcast(&cv_));
}

void InitOnce(OnceType* once, void (*initializer)()) {
  PthreadCall("once", pthread_once(once, initializer));
}

RWMutex::RWMutex() { PthreadCall("init mutex", pthread_rwlock_init(&mu_, NULL)); }

RWMutex::~RWMutex() { PthreadCall("destroy mutex", pthread_rwlock_destroy(&mu_)); }

void RWMutex::ReadLock() { PthreadCall("read lock", pthread_rwlock_rdlock(&mu_)); }

void RWMutex::WriteLock() { PthreadCall("write lock", pthread_rwlock_wrlock(&mu_)); }

void RWMutex::Unlock() { PthreadCall("unlock", pthread_rwlock_unlock(&mu_)); }

}  // namespace port
}  // namespace leveldb
