// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "db/skiplist.h"
#include <set>
#include "leveldb/env.h"
#include "util/arena.h"
#include "util/hash.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testharness.h"

namespace leveldb {

typedef uint64_t Key;

struct Comparator {
  int operator()(const Key& a, const Key& b) const {
    if (a < b) {
      return -1;
    } else if (a > b) {
      return +1;
    } else {
      return 0;
    }
  }
};

template<typename Key, class Comparator>
class SkipListTest : public SkipList<Key, Comparator>
{
 public:
  SkipListTest(Comparator cmp, Arena* arena) : SkipList<Key, Comparator>(cmp, arena) {}

  // check the validity of this SkipList object by calling the Valid() method
  // in the base class
  bool Valid() const { return SkipList<Key, Comparator>::Valid(); }

  void DisableSequentialInsertMode() { SkipList<Key, Comparator>::DisableSequentialInsertMode(); }
};

class SkipTest { };

TEST(SkipTest, Empty) {
  Arena arena;
  Comparator cmp;
  SkipListTest<Key, Comparator> list(cmp, &arena);
  ASSERT_TRUE(!list.Contains(10));
  ASSERT_TRUE(list.Valid());

  SkipListTest<Key, Comparator>::Iterator iter(&list);
  ASSERT_TRUE(!iter.Valid());
  iter.SeekToFirst();
  ASSERT_TRUE(!iter.Valid());
  iter.Seek(100);
  ASSERT_TRUE(!iter.Valid());
  iter.SeekToLast();
  ASSERT_TRUE(!iter.Valid());
}

TEST(SkipTest, InsertAndLookup) {
  const int N = 2000;
  const int R = 5000;
  Random rnd(1000);
  std::set<Key> keys;
  Arena arena;
  Comparator cmp;
  SkipListTest<Key, Comparator> list(cmp, &arena);
  for (int i = 0; i < N; i++) {
    Key key = rnd.Next() % R;
    if (keys.insert(key).second) {
      list.Insert(key);
    }
  }
  ASSERT_TRUE(list.Valid());

  for (int i = 0; i < R; i++) {
    if (list.Contains(i)) {
      ASSERT_EQ(keys.count(i), 1);
    } else {
      ASSERT_EQ(keys.count(i), 0);
    }
  }

  // Simple iterator tests
  {
    SkipListTest<Key, Comparator>::Iterator iter(&list);
    ASSERT_TRUE(!iter.Valid());

    iter.Seek(0);
    ASSERT_TRUE(iter.Valid());
    ASSERT_EQ(*(keys.begin()), iter.key());

    iter.SeekToFirst();
    ASSERT_TRUE(iter.Valid());
    ASSERT_EQ(*(keys.begin()), iter.key());

    iter.SeekToLast();
    ASSERT_TRUE(iter.Valid());
    ASSERT_EQ(*(keys.rbegin()), iter.key());
  }

  // Forward iteration test
  for (int i = 0; i < R; i++) {
    SkipListTest<Key, Comparator>::Iterator iter(&list);
    iter.Seek(i);

    // Compare against model iterator
    std::set<Key>::iterator model_iter = keys.lower_bound(i);
    for (int j = 0; j < 3; j++) {
      if (model_iter == keys.end()) {
        ASSERT_TRUE(!iter.Valid());
        break;
      } else {
        ASSERT_TRUE(iter.Valid());
        ASSERT_EQ(*model_iter, iter.key());
        ++model_iter;
        iter.Next();
      }
    }
  }

  // Backward iteration test
  {
    SkipListTest<Key, Comparator>::Iterator iter(&list);
    iter.SeekToLast();

    // Compare against model iterator
    for (std::set<Key>::reverse_iterator model_iter = keys.rbegin();
         model_iter != keys.rend();
         ++model_iter) {
      ASSERT_TRUE(iter.Valid());
      ASSERT_EQ(*model_iter, iter.key());
      iter.Prev();
    }
    ASSERT_TRUE(!iter.Valid());
  }
}

// We want to make sure that with a single writer and multiple
// concurrent readers (with no synchronization other than when a
// reader's iterator is created), the reader always observes all the
// data that was present in the skip list when the iterator was
// constructor.  Because insertions are happening concurrently, we may
// also observe new values that were inserted since the iterator was
// constructed, but we should never miss any values that were present
// at iterator construction time.
//
// We generate multi-part keys:
//     <key,gen,hash>
// where:
//     key is in range [0..K-1]
//     gen is a generation number for key
//     hash is hash(key,gen)
//
// The insertion code picks a random key, sets gen to be 1 + the last
// generation number inserted for that key, and sets hash to Hash(key,gen).
//
// At the beginning of a read, we snapshot the last inserted
// generation number for each key.  We then iterate, including random
// calls to Next() and Seek().  For every key we encounter, we
// check that it is either expected given the initial snapshot or has
// been concurrently added since the iterator started.
class ConcurrentTest {
 private:
  static const uint32_t K = 4;

  static uint64_t key(Key key) { return (key >> 40); }
  static uint64_t gen(Key key) { return (key >> 8) & 0xffffffffu; }
  static uint64_t hash(Key key) { return key & 0xff; }

  static uint64_t HashNumbers(uint64_t k, uint64_t g) {
    uint64_t data[2] = { k, g };
    return Hash(reinterpret_cast<char*>(data), sizeof(data), 0);
  }

  static Key MakeKey(uint64_t k, uint64_t g) {
    assert(sizeof(Key) == sizeof(uint64_t));
    assert(k <= K);  // We sometimes pass K to seek to the end of the skiplist
    assert(g <= 0xffffffffu);
    return ((k << 40) | (g << 8) | (HashNumbers(k, g) & 0xff));
  }

  static bool IsValidKey(Key k) {
    return hash(k) == (HashNumbers(key(k), gen(k)) & 0xff);
  }

  static Key RandomTarget(Random* rnd) {
    switch (rnd->Next() % 10) {
      case 0:
        // Seek to beginning
        return MakeKey(0, 0);
      case 1:
        // Seek to end
        return MakeKey(K, 0);
      default:
        // Seek to middle
        return MakeKey(rnd->Next() % K, 0);
    }
  }

  // Per-key generation
  struct State {
    port::AtomicPointer generation[K];
    void Set(int k, intptr_t v) {
      generation[k].Release_Store(reinterpret_cast<void*>(v));
    }
    intptr_t Get(int k) {
      return reinterpret_cast<intptr_t>(generation[k].Acquire_Load());
    }

    State() {
      for (int k = 0; k < K; k++) {
        Set(k, 0);
      }
    }
  };

  // Current state of the test
  State current_;

  Arena arena_;

  // SkipList is not protected by mu_.  We just use a single writer
  // thread to modify it.
  SkipList<Key, Comparator> list_;

 public:
  ConcurrentTest() : list_(Comparator(), &arena_) { }

  // REQUIRES: External synchronization
  void WriteStep(Random* rnd) {
    const uint32_t k = rnd->Next() % K;
    const intptr_t g = current_.Get(k) + 1;
    const Key key = MakeKey(k, g);
    list_.Insert(key);
    current_.Set(k, g);
  }

  void ReadStep(Random* rnd) {
    // Remember the initial committed state of the skiplist.
    State initial_state;
    for (int k = 0; k < K; k++) {
      initial_state.Set(k, current_.Get(k));
    }

    Key pos = RandomTarget(rnd);
    SkipList<Key, Comparator>::Iterator iter(&list_);
    iter.Seek(pos);
    while (true) {
      Key current;
      if (!iter.Valid()) {
        current = MakeKey(K, 0);
      } else {
        current = iter.key();
        ASSERT_TRUE(IsValidKey(current)) << current;
      }
      ASSERT_LE(pos, current) << "should not go backwards";

      // Verify that everything in [pos,current) was not present in
      // initial_state.
      while (pos < current) {
        ASSERT_LT(key(pos), K) << pos;

        // Note that generation 0 is never inserted, so it is ok if
        // <*,0,*> is missing.
        ASSERT_TRUE((gen(pos) == 0) ||
                    (gen(pos) > initial_state.Get(key(pos)))
                    ) << "key: " << key(pos)
                      << "; gen: " << gen(pos)
                      << "; initgen: "
                      << initial_state.Get(key(pos));

        // Advance to next key in the valid key space
        if (key(pos) < key(current)) {
          pos = MakeKey(key(pos) + 1, 0);
        } else {
          pos = MakeKey(key(pos), gen(pos) + 1);
        }
      }

      if (!iter.Valid()) {
        break;
      }

      if (rnd->Next() % 2) {
        iter.Next();
        pos = MakeKey(key(pos), gen(pos) + 1);
      } else {
        Key new_target = RandomTarget(rnd);
        if (new_target > pos) {
          pos = new_target;
          iter.Seek(new_target);
        }
      }
    }
  }
};
const uint32_t ConcurrentTest::K;

// Simple test that does single-threaded testing of the ConcurrentTest
// scaffolding.
TEST(SkipTest, ConcurrentWithoutThreads) {
  ConcurrentTest test;
  Random rnd(test::RandomSeed());
  for (int i = 0; i < 10000; i++) {
    test.ReadStep(&rnd);
    test.WriteStep(&rnd);
  }
}

class TestState {
 public:
  ConcurrentTest t_;
  int seed_;
  port::AtomicPointer quit_flag_;

  enum ReaderState {
    STARTING,
    RUNNING,
    DONE
  };

  explicit TestState(int s)
      : seed_(s),
        quit_flag_(NULL),
        state_(STARTING),
        state_cv_(&mu_) {}

  void Wait(ReaderState s) {
    MutexLock lock(&mu_);
    while (state_ != s) {
      state_cv_.Wait();
    }
  }

  void Change(ReaderState s) {
    MutexLock lock(&mu_);
    state_ = s;
    state_cv_.Signal();
  }

 private:
  port::Mutex mu_;
  ReaderState state_;
  port::CondVar state_cv_;
};

static void ConcurrentReader(void* arg) {
  TestState* state = reinterpret_cast<TestState*>(arg);
  Random rnd(state->seed_);
  int64_t reads = 0;
  state->Change(TestState::RUNNING);
  while (!state->quit_flag_.Acquire_Load()) {
    state->t_.ReadStep(&rnd);
    ++reads;
  }
  state->Change(TestState::DONE);
}

static void RunConcurrent(int run) {
  const int seed = test::RandomSeed() + (run * 100);
  Random rnd(seed);
  const int N = 1000;
  const int kSize = 1000;
  for (int i = 0; i < N; i++) {
    if ((i % 100) == 0) {
      fprintf(stderr, "Run %d of %d\n", i, N);
    }
    TestState state(seed + 1);
    Env::Default()->Schedule(ConcurrentReader, &state);
    state.Wait(TestState::RUNNING);
    for (int i = 0; i < kSize; i++) {
      state.t_.WriteStep(&rnd);
    }
    state.quit_flag_.Release_Store(&state);  // Any non-NULL arg will do
    state.Wait(TestState::DONE);
  }
}

TEST(SkipTest, Concurrent1) { RunConcurrent(1); }
TEST(SkipTest, Concurrent2) { RunConcurrent(2); }
TEST(SkipTest, Concurrent3) { RunConcurrent(3); }
TEST(SkipTest, Concurrent4) { RunConcurrent(4); }
TEST(SkipTest, Concurrent5) { RunConcurrent(5); }

static void
RunSequentialInsert(
  const int NumKeys,
  bool      AcquireLock,
  bool      ReverseInsert,
  bool      SequentialInsertModeEnabled )
{
  const int loopCount = 5; // repeat the whole process this many times and average the time spent
  std::vector<uint64_t> timeSpent;

  port::Mutex mutex;
  Env* env = Env::Default();

  fprintf( stderr,
           "Sequentially inserting %d keys in %s order,\n"
           "      seqential insert mode is initially %sabled,\n"
           "      %sacquiring a lock for each insert (averaging over %d runs)\n",
           NumKeys, ReverseInsert ? "reverse" : "forward",
           SequentialInsertModeEnabled ? "en" : "dis",
           AcquireLock ? "" : "not ", loopCount );

  int k;
  for ( k = 0; k < loopCount; ++k ) {
    int j;
    Arena arena;
    Comparator cmp;
    SkipListTest<Key, Comparator> list( cmp, &arena );

    // initially the SkipList should be in sequential mode
    ASSERT_TRUE( list.InSequentialInsertMode() );

    // were we instructed to disable sequential insert mode?
    if ( !SequentialInsertModeEnabled ) {
      list.DisableSequentialInsertMode();
      ASSERT_TRUE( !list.InSequentialInsertMode() );
    }

    uint64_t start = env->NowMicros();
    for ( j = 0; j < NumKeys; ++j ) {
      Key key = ReverseInsert ? NumKeys - 1 - j : j;

      if ( AcquireLock ) mutex.Lock();
      list.Insert( key );
      if ( AcquireLock ) mutex.Unlock();
    }
    uint64_t stop = env->NowMicros();
    timeSpent.push_back( stop - start );
    //fprintf( stderr, "  Time for run %d: %llu\n", k, timeSpent[k] );

    // if SequentialInsertModeEnabled is true, the SkipList should still be
    // in sequential mode iff ReverseInsert is false
    if ( SequentialInsertModeEnabled ) {
      ASSERT_TRUE( list.InSequentialInsertMode() != ReverseInsert );
    }
    else {
      ASSERT_TRUE( !list.InSequentialInsertMode() );
    }

    // ensure the SkipLlist is properly sorted
    if ( AcquireLock ) mutex.Lock();
    ASSERT_TRUE( list.Valid() );
    if ( AcquireLock ) mutex.Unlock();

    // ensure the SkipList contains all the keys we inserted
    for ( j = 0; j < NumKeys; ++j ) {
      ASSERT_TRUE( list.Contains( j ) );
    }
  }

  // throw out the low and high times and average the rest
  uint64_t totalTime, lowTime, highTime;
  totalTime = lowTime = highTime = timeSpent[0];
  for ( k = 1; k < loopCount; ++k ) {
    uint64_t currentTime = timeSpent[k];
    totalTime += currentTime;
    if ( lowTime > currentTime ) lowTime = currentTime;
    if ( highTime < currentTime ) highTime = currentTime;
  }

  totalTime -= (lowTime + highTime);

  uint64_t averageTime = (totalTime / (loopCount - 2));
  double timePerKey = (double)averageTime / (double)NumKeys;
  fprintf( stderr, "   Average insertion time: %" PRIu64 " (%f/key)\n", averageTime, timePerKey );
}

TEST(SkipTest, SequentialInsert_NoLock_ForwardInsert)
{
  int numKeys = 100000;
  bool acquireLock = false;
  bool reverseInsert = false;
  bool sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  sequentialInsertModeEnabled = false;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );
}

TEST(SkipTest, SequentialInsert_Lock_ForwardInsert)
{
  int numKeys = 100000;
  bool acquireLock = true;
  bool reverseInsert = false;
  bool sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  sequentialInsertModeEnabled = false;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );
}

TEST(SkipTest, SequentialInsert_NoLock_ReverseInsert)
{
  int numKeys = 100000;
  bool acquireLock = false;
  bool reverseInsert = true;
  bool sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );
}

TEST(SkipTest, SequentialInsert_Lock_ReverseInsert)
{
  int numKeys = 100000;
  bool acquireLock = true;
  bool reverseInsert = true;
  bool sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );
}

TEST(SkipTest, SequentialInsert_IncreasingNumberOfInserts)
{
  // test with increasing numbers of keys, with sequential-insert mode both
  // enabled and disabled; we're looking to see if per-key insertion times
  // trend upward as the number of keys increases
  int numKeys = 10000;
  bool acquireLock = false;
  bool reverseInsert = false;
  bool sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  sequentialInsertModeEnabled = false;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  numKeys = 100000;
  sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  sequentialInsertModeEnabled = false;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  numKeys = 1000000;
  sequentialInsertModeEnabled = true;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );

  sequentialInsertModeEnabled = false;
  RunSequentialInsert( numKeys, acquireLock, reverseInsert, sequentialInsertModeEnabled );
}

TEST(SkipTest, SequentialInsert_MixedInsertionModes)
{
  // start inserting sequentially, then switch to non-sequential inserts,
  // ensuring all works as intended
  int j, numSequentialKeys = 100000, numNonSequentialKeys = 100000;
  int totalNumKeys = numSequentialKeys + numNonSequentialKeys;
  Arena arena;
  Comparator cmp;
  SkipListTest<Key, Comparator> list( cmp, &arena );

  // initially the SkipList should be in sequential mode
  ASSERT_TRUE( list.InSequentialInsertMode() );

  // start inserting at key=1; when we insert 0 below, the list should switch
  // out of sequential insert mode
  for ( j = 1; j < numSequentialKeys; ++j ) {
    list.Insert( j );
  }

  // the SkipList should still be in sequential mode
  ASSERT_TRUE( list.InSequentialInsertMode() );
  ASSERT_TRUE( list.Valid() );

  list.Insert( 0 );
  ASSERT_TRUE( !list.InSequentialInsertMode() );
  ASSERT_TRUE( list.Valid() );

  // now insert the remaining keys in non-sequential order (they're not
  // random, but that doesn't matter here; just ensure we switch to
  // non-sequential mode and that all continues to work)
  for ( j = 0; j < numNonSequentialKeys; j += 2 ) {
    int key = totalNumKeys - j - 1;
    list.Insert( key );
  }
  for ( j = 0; j < numNonSequentialKeys; j += 2 ) {
    int key = numSequentialKeys + j;
    list.Insert( key );
  }

  ASSERT_TRUE( !list.InSequentialInsertMode() );
  ASSERT_TRUE( list.Valid() );

  // ensure the SkipList contains all the keys we inserted
  for ( j = 0; j < totalNumKeys; ++j ) {
    ASSERT_TRUE( list.Contains( j ) );
  }
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::RunAllTests();
}
