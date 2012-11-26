#ifndef __EELISH_FIXED_VECTOR__HPP
#define __EELISH_FIXED_VECTOR__HPP

#include <cstring>

#include "atomics.hpp"

namespace eelish {

/// A mostly-lockless fixed-size Vector
///
///        This is the first time I've done any non-trivial lockless
///        programming.  If you think you've spotted a bug, please let
///        me know. :)
///
/// The consistency guarantee provided by FixedVector is this:
///
///   Let P0, P1 ... P(N - 1) be N concurrent changes or reads.  Then
///   it is always possible to "explain" their overall effect (due to
///   the writes) and the information gathered (due to the reads) from
///   a specific linearization of the concurrent operations.
///
/// As an example, consider the following four operations invoked
/// concurrently on an empty stack:
///
///   1. Push 5
///   2. Push 6
///   3. Push 7
///   4. Pop
///
/// All of the following are legal results:
///   Vector contents           | Return from Pop
///   [7, 5, 6]                 |  <error: empty stack>
///   [5, 7]                    |  6
///   [7, 6]                    |  5
///
/// ... and so forth.  I hope you get the idea.
///
/// The "mostly" comes from the fact that a thread cannot "pop past" a
/// pop currently in progress in another thread.  I'll attempt to
/// explain by example:
///
///   Consider the following vector [1, 2, 3].  Imagine two threads, A
///   and B, operating on the vector concurrently:
///
///      State       |   Thread A     |   Thread B
///                  |                |
///    [1, 2, 3]     |  Pop           |  Push 6
///    [1, 2, 3, 6]  |  Ongoing       |  Pop (gets 6)
///    [1, 2, 3]     |  Ongoing       |  Pop
///    [1, 2, 3]     |  Ongoing       |  Spins
///    [1, 2]        |  Done (gets 3) |  Spins
///    [1]           |                |  Done (gets 2)
///
/// In other words, an ongoing pop "owns" the vector indices less than
/// and equal to the index current when it was invoked.  Reads are
/// allowed everywhere and writes (pushes and pops) are allowed as
/// long as the concerned indices are greater than index of the
/// greatest index with an ongoing pop operation.
///
/// While this seems to work in principle, it isn't ready for
/// production use.  Specifically, there are two situations I'd like
/// to point out (A and B are threads):
///
///   a. `A` issues a pop_back and crashes right after priming the
///      appropriate value.  A section of the hashtable is no
///      unusable.
///
///   b. `A` issues a push_back and crashes after changing the length
///      but before writing to buffer.  The hashtable in inconsistent
///      now; till that inconsistent (unwritten) value is popped off.
///
/// A vector of size `Size` and holding elements of type `T *`.  The
/// implementation steal the last two bits of the pointers, so keep
/// this in mind when doing naughty things.  Moreover, the range for
/// `T *` must not include the sentinels declared below.
template<typename T, std::size_t Size>
class FixedVector {
 public:
  FixedVector();

  /// Push a value into the vector.  Returns the index at which the
  /// value is inserted, or -1 if the FixedVector is currently full.
  std::size_t push_back(T *value);

  /// Pop a value from the tail of the vector.  The index of the value
  /// is returned in `out_index`.  `pop_back` on an empty vector
  /// returns kOutOfRange, which can be tested using
  /// `is_out_of_range`.
  T *pop_back(std::size_t *out_index);

  /// Fetches a value from the vector.  Returns kOutOfRange for an
  /// invalid index (check using is_out_of_range).
  T *get(std::size_t index);

  std::size_t length() const;

  inline static bool is_inconsistent(T *value) {
    return (reinterpret_cast<intptr_t>(value) & (~kBitMask)) ==
        (kInconsistent & (~kBitMask));
  }

  inline static bool is_out_of_range(T *value) {
    return (reinterpret_cast<intptr_t>(value) & (~kBitMask)) ==
        (kOutOfRange & (~kBitMask));
  }

 private:
  Atomic<Word> length_;
  Atomic<T *> buffer_[Size];

  /// Sentinels.  We expect no pointer to have these exact values.
  static const intptr_t kInconsistent = -1;
  static const intptr_t kOutOfRange = -2;
  static const intptr_t kBitMask = 3;
};

}

#include "fixed-vector-inl.hpp"

#endif
