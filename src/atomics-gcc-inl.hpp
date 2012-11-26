#ifndef __EELISH_ATOMICS__HPP
#error "atomics-gcc-inl.hpp can only be included from within atomics.hpp"
#endif

namespace eelish {

template<typename T>
bool Atomic<T>::boolean_cas(T old_value, T new_value) {
  Word old_word = reinterpret_cast<Word>(old_value);
  Word new_word = reinterpret_cast<Word>(new_value);
  return __sync_bool_compare_and_swap(&value_, old_word, new_word);
}

template<typename T>
T Atomic<T>::value_cas(T old_value, T new_value) {
  Word old_word = reinterpret_cast<Word>(old_value);
  Word new_word = reinterpret_cast<Word>(new_value);
  return __sync_val_compare_and_swap(&value_, old_word, new_word);
}

template<typename T>
T Atomic<T>::acquire_load() const {
  return reinterpret_cast<T>(__atomic_load_n(&value_, __ATOMIC_ACQUIRE));
}

template<typename T>
void Atomic<T>::release_store(T value) {
  Word word_value = reinterpret_cast<Word>(value);
  __atomic_store(&value_, &word_value, __ATOMIC_RELEASE);
}

template<typename T>
T Atomic<T>::nobarrier_load() const {
  return reinterpret_cast<T>(__atomic_load_n(&value_, __ATOMIC_RELAXED));
}

template<typename T>
void Atomic<T>::nobarrier_store(T value) {
  Word word_value = reinterpret_cast<Word>(value);
  __atomic_store(&value_, &word_value, __ATOMIC_RELAXED);
}

template<typename T>
void Atomic<T>::flush() {
  flush_cache(&value_, &value_ + 1);
}

void memory_fence() {
  __atomic_thread_fence(__ATOMIC_ACQ_REL);
}

template<typename T>
bool cas_success(volatile T **location, T *old_value, T *new_value) {
  return __sync_bool_compare_and_swap(location, old_value, new_value);
}

template<typename T>
T *cas_previous_value(volatile T **location, T *old_value, T *new_value) {
  return __sync_val_compare_and_swap(location, old_value, new_value);
}

}

#include "atomics-gcc-x86-inl.hpp"
