#ifndef __EELISH_FIXED_VECTOR__HPP
#error "fixed-vector-inl.hpp can only be included from within fixed-vector.hpp"
#endif

#include <cassert>

#include "utils.hpp"

namespace eelish {

// TODO: *IMPORTANT* comment discussing the rationale why FixedVector
// obeys the consistency principles mentioned in fixed-vector.hpp

// TODO: add a set_at function to FixedVector.  I don't think doing so
// should be overly difficult (as long as we are careful not to step
// on a value that is being popped currently), the harder part is
// coming up with a bunch of convincing test cases.

template<typename T, std::size_t Size>
FixedVector<T, Size>::FixedVector() {
  length_.raw_store(0);
  for (std::size_t i = 0; i < Size; i++) {
    buffer_[i].raw_store(reinterpret_cast<T *>(kInconsistent));
  }
}

template<typename T, std::size_t Size>
std::size_t FixedVector<T, Size>::push_back(T *value) {
  while (true) {
    Word index = length_.nobarrier_load();
    if (index >= Size) return -1;

    // We "make space" for the element we are going to insert by
    // incrementing the index.  We can't use an atomic add here since
    // we need to ensure we don't bump the index out of bounds and
    // make the container inconsistent.  There may be some clever way
    // around that, though; might be worth thinking about if atomic
    // adds are faster than atomic compare exchanges.
    if (!length_.boolean_cas(index, index + 1)) continue;

    // We can't let the actual store to the buffer be reordered ahead
    // of the length_ increment -- another thread might end up writing
    // to the same location.
    memory_fence();
    buffer_[index].nobarrier_store(value);
    return static_cast<std::size_t>(index);
  }
}

template<typename T, std::size_t Size>
T *FixedVector<T, Size>::pop_back(std::size_t *out_index) {
  int kRetryDelay = 1;
  while (true) {
    Word length = length_.nobarrier_load();
    if (length == 0) return reinterpret_cast<T *>(kOutOfRange);

    Word index = length - 1;
    T *value;

    // pop_back "primes" the value it is about to pop by setting a
    // bit.  It is illegal to pop "past" a primed element.
    if (unlikely(!buffer_[index].cas_prime(&value))) {
      Platform::Sleep(kRetryDelay);
      continue;
    }

    // We can't let this load be reordered to after the modifying the
    // length -- we might end up reading a completely different value.
    memory_fence();

    if (unlikely(!length_.boolean_cas(length, length - 1))) {
      // Something's changed.  Undo your doings and try again.  If
      // other pop_backs have respected the prime bit, this cannot
      // have been unprimed.
      bool result = buffer_[index].cas_unprime();
      assert(result);
      (void) result;
      continue;
    }

    if (out_index != NULL) *out_index = index;
    return value;
  }
}

template<typename T, std::size_t Size>
T *FixedVector<T, Size>::get(std::size_t index) {
  assert(index < Size);
  Word length = length_.nobarrier_load();
  T *out_of_range = reinterpret_cast<T *>(kOutOfRange);

  if (index >= length) return out_of_range;

  T *value = buffer_[index].nobarrier_load_unprimed();

  if (is_inconsistent(value)) {
    return out_of_range;
  } else {
    return value;
  }
}

template<typename T, std::size_t Size>
std::size_t FixedVector<T, Size>::length() const {
  return length_.nobarrier_load();
}

}
