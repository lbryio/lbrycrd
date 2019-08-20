// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
//
// Invariants:
//
// (1) Allocated nodes are never deleted until the SkipList is
// destroyed.  This is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) The contents of a Node except for the next/prev pointers are
// immutable after the Node has been linked into the SkipList.
// Only Insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...

#include <assert.h>
#include <stdlib.h>
#include "port/port.h"
#include "util/arena.h"
#include "util/random.h"

namespace leveldb {

class Arena;

template<typename Key, class Comparator>
class SkipList {
 private:
  struct Node;

 public:
  // Create a new SkipList object that will use "cmp" for comparing keys,
  // and will allocate memory using "*arena".  Objects allocated in the arena
  // must remain allocated for the lifetime of the skiplist object.
  explicit SkipList(Comparator cmp, Arena* arena);

  // Insert key into the list.
  // REQUIRES: nothing that compares equal to key is currently in the list.
  void Insert(const Key& key);

  // Returns true iff an entry that compares equal to key is in the list.
  bool Contains(const Key& key) const;

  // Returns true if all inserts have been sequentially increasing;
  // else this SkipList has had keys inserted in non-sequential order
  bool InSequentialInsertMode() const {
    return sequentialInsertMode_;
  }

  // Iteration over the contents of a skip list
  class Iterator {
   public:
    // Initialize an iterator over the specified list.
    // The returned iterator is not valid.
    explicit Iterator(const SkipList* list);

    // Returns true iff the iterator is positioned at a valid node.
    bool Valid() const;

    // Returns the key at the current position.
    // REQUIRES: Valid()
    const Key& key() const;

    // Advances to the next position.
    // REQUIRES: Valid()
    void Next();

    // Advances to the previous position.
    // REQUIRES: Valid()
    void Prev();

    // Advance to the first entry with a key >= target
    void Seek(const Key& target);

    // Position at the first entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    void SeekToFirst();

    // Position at the last entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    void SeekToLast();

   private:
    const SkipList* list_;
    Node* node_;
    // Intentionally copyable
  };

 protected:
  // Checks the structure of this SkipList object, ensuring the keys are
  // properly ordered
  //
  // This is protected since it is intended for use by unit tests; if a lock
  // is used to protect Insert(), then it should be used to protect this
  // method as well
  bool Valid() const;

  // Disables the sequential insert optimizations (used in performance testing)
  void DisableSequentialInsertMode() {
    sequentialInsertMode_ = false;
  }

 private:
  enum { kMaxHeight = 17 };

  // Immutable after construction
  Comparator const compare_;
  Arena* const arena_;    // Arena used for allocations of nodes

  Node* const head_;

  // Modified only by Insert().  Read racily by readers, but stale
  // values are ok.
  port::AtomicPointer max_height_;   // Height of the entire list

  inline int GetMaxHeight() const {
    return static_cast<int>(
        reinterpret_cast<intptr_t>(max_height_.NoBarrier_Load()));
  }

  // Read/written only by Insert().
  Random rnd_;

  // Points to the last node in the list; modified only by Insert()
  Node* tail_;

  // Pointers to the nodes previous to the tail node; have max_height_ entries
  Node* tailPrev_[kMaxHeight];

  // The height of the tail_ node
  int tailHeight_;

  // We track the tail node until we have a non-sequential insert
  bool sequentialInsertMode_;

  Node* NewNode(const Key& key, int height);
  int RandomHeight();
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }

  // Return true if key is greater than the data stored in "n"
  bool KeyIsAfterNode(const Key& key, Node* n) const;

  // Return the earliest node that comes at or after key.
  // Return NULL if there is no such node.
  //
  // If prev is non-NULL, fills prev[level] with pointer to previous
  // node at "level" for every level in [0..max_height_-1].
  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

  // Similar to FindGreaterOrEqual() except it uses the barrier-free
  // variant of Next(); this is used only by Insert() and it
  // checks the tail_ pointer in case we're doing a sequential insert
  Node* NoBarrier_FindGreaterOrEqual(const Key& key, Node** prev) const;

  // Return the latest node with a key < key.
  // Return head_ if there is no such node.
  Node* FindLessThan(const Key& key) const;

  // Return the last node in the list.
  // Return head_ if list is empty.
  Node* FindLast() const;

  // No copying allowed
  SkipList(const SkipList&);
  void operator=(const SkipList&);
};

// Implementation details follow
template<typename Key, class Comparator>
struct SkipList<Key,Comparator>::Node {
  explicit Node(const Key& k) : key(k) { }

  Key const key;

  // Accessors/mutators for links.  Wrapped in methods so we can
  // add the appropriate barriers as necessary.
  Node* Next(int n) {
    assert(n >= 0);
    // Use an 'acquire load' so that we observe a fully initialized
    // version of the returned Node.
    return reinterpret_cast<Node*>(next_[n].Acquire_Load());
  }
  void SetNext(int n, Node* x) {
    assert(n >= 0);
    // Use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    next_[n].Release_Store(x);
  }

  // No-barrier variants that can be safely used in a few locations.
  Node* NoBarrier_Next(int n) {
    assert(n >= 0);
    return reinterpret_cast<Node*>(next_[n].NoBarrier_Load());
  }
  void NoBarrier_SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].NoBarrier_Store(x);
  }

 private:
  // Array of length equal to the node height.  next_[0] is lowest level link.
  port::AtomicPointer next_[1];
};

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*
SkipList<Key,Comparator>::NewNode(const Key& key, int height) {
  char* mem = arena_->AllocateAligned(
      sizeof(Node) + sizeof(port::AtomicPointer) * (height - 1));
  return new (mem) Node(key);
}

template<typename Key, class Comparator>
inline SkipList<Key,Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = NULL;
}

template<typename Key, class Comparator>
inline bool SkipList<Key,Comparator>::Iterator::Valid() const {
  return node_ != NULL;
}

template<typename Key, class Comparator>
inline const Key& SkipList<Key,Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next(0);
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Prev() {
  // Instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(Valid());
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = NULL;
  }
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, NULL);
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = NULL;
  }
}

template<typename Key, class Comparator>
int SkipList<Key,Comparator>::RandomHeight() {
  // Increase height with probability 1 in kBranching
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < kMaxHeight && ((rnd_.Next() % kBranching) == 0)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight);
  return height;
}

template<typename Key, class Comparator>
bool SkipList<Key,Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
  // NULL n is considered infinite
  return (n != NULL) && (compare_(n->key, key) < 0);
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::FindGreaterOrEqual(const Key& key, Node** prev)
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (KeyIsAfterNode(key, next)) {
      // Keep searching in this list
      x = next;
    } else {
      if (prev != NULL) prev[level] = x;
      if (level == 0) {
        return next;
      } else {
        // Switch to next list
        level--;
      }
    }
  }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*
SkipList<Key,Comparator>::NoBarrier_FindGreaterOrEqual(const Key& key, Node** prev) const {
  int level = GetMaxHeight() - 1;

  // If we have only seen sequential inserts up to this point, we can use
  // the tail_ node
  if ( sequentialInsertMode_ ) {
    if (tail_ == NULL) {
      // The list is currently empty, so the node being inserted
      // will be the new tail_
      assert(level == 0);
      if (prev != NULL) prev[0] = head_;
      return NULL;
    }
    else if (KeyIsAfterNode(key, tail_)) {
      // The new key must be inserted after the current tail_ node
      if (prev != NULL) {
        int i;
        for (i = 0; i < tailHeight_; ++i) {
          prev[i] = tail_;
        }
        for (/*continue with i*/; i <= level; ++i) {
          prev[i] = tailPrev_[i];
        }
      }
      return NULL;
    }
  }

  Node* x = head_;
  while (true) {
    Node* next = x->NoBarrier_Next(level);
    if (KeyIsAfterNode(key, next)) {
      // Keep searching in this list
      x = next;
    } else {
      if (prev != NULL) prev[level] = x;
      if (level == 0) {
        return next;
      } else {
        // Switch to next list
        level--;
      }
    }
  }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node*
SkipList<Key,Comparator>::FindLessThan(const Key& key) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node* next = x->Next(level);
    if (next == NULL || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::FindLast()
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (next == NULL) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename Key, class Comparator>
SkipList<Key,Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(0 /* any key will do */, kMaxHeight)),
      max_height_(reinterpret_cast<void*>(1)),
      rnd_(0xdeadbeef),
      tail_(NULL),
      tailHeight_(0),
      sequentialInsertMode_(true) {
  for (int i = 0; i < kMaxHeight; i++) {
    head_->SetNext(i, NULL);
    tailPrev_[i] = NULL;
  }
}

template<typename Key, class Comparator>
void SkipList<Key,Comparator>::Insert(const Key& key) {
  // We use a barrier-free variant of FindGreaterOrEqual()
  // here since Insert() is externally synchronized.
  Node* prev[kMaxHeight];
  Node* x = NoBarrier_FindGreaterOrEqual(key, prev);

  // If we're still in sequential-insert mode, check if the new node is being
  // inserted at the end of the list, which is indicated by x being NULL
  if (sequentialInsertMode_) {
    if (x != NULL) {
      // we have a non-sequential (AKA random) insert, so stop maintaining
      // the tail bookkeeping overhead
      sequentialInsertMode_ = false;
    }
  }

  // Our data structure does not allow duplicate insertion
  assert(x == NULL || !Equal(key, x->key));

  int i, height = RandomHeight();
  if (height > GetMaxHeight()) {
    // We are extending max_height_ which means we need to fill in the blanks
    // in prev[] that were not filled in by NoBarrier_FindGreaterOrEqual()
    for (i = GetMaxHeight(); i < height; ++i) {
      prev[i] = head_;
    }
    //fprintf(stderr, "Change height from %d to %d\n", max_height_, height);

    // It is ok to mutate max_height_ without any synchronization
    // with concurrent readers.  A concurrent reader that observes
    // the new value of max_height_ will see either the old value of
    // new level pointers from head_ (NULL), or a new value set in
    // the loop below.  In the former case the reader will
    // immediately drop to the next level since NULL sorts after all
    // keys.  In the latter case the reader will use the new node.
    max_height_.NoBarrier_Store(reinterpret_cast<void*>(height));
  }

  x = NewNode(key, height);
  for (i = 0; i < height; ++i) {
    // NoBarrier_SetNext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
    prev[i]->SetNext(i, x);
  }

  // Do we need to update our tail_ pointer?
  if (sequentialInsertMode_) {
    Node* prevTail = tail_;
    int prevTailHeight = tailHeight_;

    tail_ = x;
    tailHeight_ = height;

    // We also need to update our tailPrev_ pointers; first we capture
    // the nodes already pointing to the new tail_
    for (i = 0; i < height; ++i) {
      tailPrev_[i] = prev[i];
    }

    // If the previous tail node was taller than the new tail node, then
    // the prev pointers above the current tail node's height (up to the
    // height of the previous tail node) are simply the previous tail node
    for (/*continue with i*/; i < prevTailHeight; ++i) {
      tailPrev_[i] = prevTail;
    }

    // NOTE: any prev pointers above prevTailHeight (up to max_height_) were
    // already set in tailPrev_ by previous calls to this method
  }
}

template<typename Key, class Comparator>
bool SkipList<Key,Comparator>::Contains(const Key& key) const {
  Node* x = FindGreaterOrEqual(key, NULL);
  if (x != NULL && Equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}

template<typename Key, class Comparator>
bool SkipList<Key,Comparator>::Valid() const
{
  // Note that we can use barrier-free overloads in this method since it is
  // protected by the same lock as Insert().

  // Ensure that the list is properly sorted; use an iterator for this check
  const Key* pPrevKey = NULL;
  typename SkipList<Key, Comparator>::Iterator iter(this);
  for ( iter.SeekToFirst(); iter.Valid(); iter.Next() ) {
    if ( pPrevKey != NULL ) {
      if ( compare_( *pPrevKey, iter.key() ) >= 0 ) {
        return false;
      }
    }
    pPrevKey = &iter.key();
  }

  // Now walk the linked list at each level and ensure it's sorted. Also track
  // how many nodes we see at each level; the number of nodes in the linked
  // list at level n must not be larger than the number of nodes at level n-1.
  std::vector<int> nodeCounts( GetMaxHeight() );
  int level;
  for ( level = GetMaxHeight() - 1; level >= 0; --level ) {
    int nodeCount = 0;
    pPrevKey = NULL;
    for ( Node* pNode = head_->NoBarrier_Next( level );
          pNode != NULL;
          pNode = pNode->NoBarrier_Next( level ) ) {
      ++nodeCount;
      if ( pPrevKey != NULL ) {
        if ( compare_( *pPrevKey, pNode->key ) >= 0 ) {
          return false;
        }
      }
      pPrevKey = &pNode->key;
    }
    nodeCounts[ level ] = nodeCount;
  }

  // Ensure the node counts do not increase as we move up the levels
  int prevNodeCount = nodeCounts[0];
  for ( level = 1; level < GetMaxHeight(); ++level ) {
    int currentNodeCount = nodeCounts[ level ];
    if ( currentNodeCount > prevNodeCount ) {
      return false;
    }
    prevNodeCount = currentNodeCount;
  }

  // Ensure that tail_ points to the last node
  if ( sequentialInsertMode_ ) {
    if ( tail_ == NULL ) {
      // tail_ is not set, so the list must be empty
      if ( tailPrev_[0] != NULL || head_->NoBarrier_Next(0) != NULL ) {
        return false;
      }
    }
    else {
      // we have a tail_ node; first ensure that its prev pointer actually
      // points to it
      if ( tailPrev_[0] == NULL || tailPrev_[0]->NoBarrier_Next(0) != tail_ ) {
        return false;
      }
      if ( compare_( tailPrev_[0]->key, tail_->key ) >= 0 ) {
        return false;
      }

      // now check the rest of the pointers in tailPrev_; up to tailHeight_,
      // the next pointer of the node in tailPrev_ should point to tail_; after
      // that, the next pointer should be NULL
      for ( level = 1; level < GetMaxHeight(); ++level ) {
        Node* tailPrev = tailPrev_[ level ];
        if ( tailPrev == NULL ) {
          return false;
        }
        if ( level < tailHeight_ ) {
          if ( tailPrev->NoBarrier_Next( level ) != tail_ ) {
            return false;
          }
          if ( compare_( tailPrev->key, tail_->key ) >= 0 ) {
            return false;
          }
        }
        else {
          if ( tailPrev->NoBarrier_Next( level ) != NULL ) {
            return false;
          }
        }
      }

      // the remainder of the tailPrev_ pointers (above max_height_)
      // should be NULL
      for ( /*continue with level*/; level < kMaxHeight; ++level ) {
        if ( tailPrev_[ level ] != NULL ) {
          return false;
        }
      }

      // now ensure that FindLast() returns tail_
      Node* lastNode = FindLast();
      if ( lastNode != tail_ ) {
        return false;
      }
    }
  }

  // if we get here, all is good
  return true;
}

}  // namespace leveldb
