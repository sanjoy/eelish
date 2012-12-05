#ifndef __EELISH_ATOMICS__HPP
#define __EELISH_ATOMICS__HPP

#include <stdint.h>

namespace eelish {

typedef uintptr_t Word;

/// Provides atomic access to a word.
template<typename T>
class Atomic {
 public:
  inline bool boolean_cas(T old_value, T new_value);
  inline T value_cas(T old_value, T new_value);

  inline T acquire_load() const;
  inline void release_store(T value);

  inline T nobarrier_load() const;
  inline void nobarrier_store(T value);

  inline T raw_load() const;
  inline void raw_store(T value);

  inline void flush();

  /// A "primed" word is a word with its least significant bit set.
  /// Whether such an operation is meaningful or not is determined by
  /// the context in which it is used.
  inline bool is_primed() const;

  /// cas_prime and cas_unprime atomically prime and unprime a word
  /// and return true if successful.  Note that cas_prime will fail if
  /// the word is already primed and cas_unprime will fail if the word
  /// isn't primed.
  inline bool cas_prime();
  inline bool cas_unprime();

  /// Convenience functions which return an unprimed value of type T.
  /// Helps avoid ugly casts in some cases.
#define LOAD_UNPRIMED_FUNCTION(kind)                            \
  inline T kind ## _load_unprimed() const {                     \
    Word word_value = reinterpret_cast<Word>(kind ##_load());   \
    return reinterpret_cast<T>(word_value & (~kPrimeBit));      \
  }

  LOAD_UNPRIMED_FUNCTION(acquire);
  LOAD_UNPRIMED_FUNCTION(nobarrier);
  LOAD_UNPRIMED_FUNCTION(raw);

#undef LOAD_UNPRIMED_FUNCTION

 private:
  volatile Word value_;
  static const Word kPrimeBit = static_cast<Word>(1);
};


inline void memory_fence();

template<typename T>
inline void flush_cache(volatile T *begin, volatile T *end);


template<typename T>
T Atomic<T>::raw_load() const {
  return reinterpret_cast<T>(value_);
}

template<typename T>
void Atomic<T>::raw_store(T value) {
  value_ = reinterpret_cast<Word>(value);
}

template<typename T>
inline bool Atomic<T>::is_primed() const {
  return reinterpret_cast<Word>(nobarrier_load()) & kPrimeBit;
}

template<typename T>
inline bool Atomic<T>::cas_prime() {
  Word old_value = reinterpret_cast<Word>(nobarrier_load()) & (~kPrimeBit);
  Word new_value = old_value | kPrimeBit;
  return boolean_cas(reinterpret_cast<T>(old_value),
                     reinterpret_cast<T>(new_value));
}

template<typename T>
inline bool Atomic<T>::cas_unprime() {
  Word old_value = reinterpret_cast<Word>(nobarrier_load()) | kPrimeBit;
  Word new_value = old_value & (~kPrimeBit);
  return boolean_cas(reinterpret_cast<T>(old_value),
                     reinterpret_cast<T>(new_value));
}

}

#ifdef __GNUC__
#include "atomics-gcc-inl.hpp"
#else
#error "eelish only understands gcc atomics"
#endif

#endif
