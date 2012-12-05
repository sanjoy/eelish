#include "tests.hpp"
#include "fixed-vector.hpp"

#include <algorithm>
#include <cstdlib>

#include "platform.hpp"

using namespace eelish;
using namespace std;

namespace {

/// Instead of malloc'ing all the time, we simply cast integers to
/// pointers.  FixedVector assumes things about how a pointer looks,
/// so we need to shuffle some bits around.
long *to_pointer(int value) {
  intptr_t large_value = static_cast<intptr_t>(value);
  return reinterpret_cast<long *>(large_value << 2);
}

int to_integer(long *pointer) {
  intptr_t integral_value = reinterpret_cast<intptr_t>(pointer);
  return static_cast<int>(integral_value >> 2);
}

const int kVectorSize = 128 * 1024;
const int kSampleValue = 4242;

// We will compare the performance of FixedVector with a naive locked
// implementation.

template<typename T, size_t Size>
class NaiveFixedVector {
 public:
  NaiveFixedVector() : length_(0) { }

  size_t push_back(T *value) {
    assert(length_ < Size);
    MutexLocker lock(&mutex_);
    buffer_[length_] = value;
    return length_++;
  }

  T *pop_back(size_t *out_index) {
    MutexLocker lock(&mutex_);
    assert(length_ > 0);
    T *value = buffer_[--length_];
    if (out_index != NULL) *out_index = length_;
    return value;
  }

  T *get(size_t index) {
    MutexLocker lock(&mutex_);
    if (index < length_) {
      return buffer_[index];
    } else {
      return reinterpret_cast<T *>(kOutOfRange);
    }
  }

  size_t length() const { return length_; }

  inline static bool is_consistent(T *) { return true; }
  inline static bool is_out_of_range(T *value) {
    return (reinterpret_cast<intptr_t>(value) & (~kBitMask)) ==
        (kOutOfRange & (~kBitMask));
  }

 private:
  size_t length_;
  T *buffer_[Size];
  Mutex mutex_;

  static const intptr_t kOutOfRange = -2;
  static const intptr_t kBitMask = 3;
};


template<template<typename T, size_t S> class Vec>
struct VectorNamePrefix;

template<>
struct VectorNamePrefix<FixedVector> {
  static string prefix() { return "fixed-vector-"; }
};

template<>
struct VectorNamePrefix<NaiveFixedVector> {
  static string prefix() { return "naive-vector-"; }
};


template<template<typename T, size_t S> class Vec>
class FixedVectorTest : public ThreadedTest {
 public:
  explicit FixedVectorTest(const string &subname) :
    ThreadedTest(VectorNamePrefix<Vec>::prefix() + subname) {
  }

 protected:
  virtual void synch_init() {
    vector_ = new Vec<long, kVectorSize>;
  }

  virtual void synch_destroy() {
    delete vector_;
  }

  int definite_pop() {
    while (true) {
      long *attempt = vector_->pop_back(NULL);
      if (!FixedVector<long, kVectorSize>::is_out_of_range(attempt)) {
        return to_integer(attempt);
      }
    }
  }

  void definite_push(int value) {
    long *pointer_value = to_pointer(value);
    while (vector_->push_back(pointer_value) == static_cast<size_t>(-1))
      ;
  }

  Vec<long, kVectorSize> *vector_;
};


template<template<typename T, size_t S> class Vec>
class PushOnlyTest : public FixedVectorTest<Vec> {
 public:
  PushOnlyTest() : FixedVectorTest<Vec>("push-only") { }

 protected:
  virtual bool threaded_test() {
    int thread_count = ThreadedTest::get_thread_count();
    int iterations = kVectorSize / thread_count;

    for (int i = 0; i < iterations; i++) {
      FixedVectorTest<Vec>::vector_->push_back(to_pointer(kSampleValue));
    }

    return true;
  }

  virtual bool synch_verify() {
    if (!FixedVectorTest<Vec>::synch_verify()) {
      return false;
    }

    int thread_count = ThreadedTest::get_thread_count();
    int per_thread_pushes = kVectorSize  / thread_count;
    size_t expected_length = per_thread_pushes * thread_count;

    check_i(FixedVectorTest<Vec>::vector_->length(), ==, expected_length,
            return false);

    for (size_t i = 0; i < expected_length; i++) {
      // Note that calling this macro like this is not safe with
      // several running threads.
      check_i(FixedVectorTest<Vec>::vector_->get(i), ==,
              to_pointer(kSampleValue), return false);
    }

    return true;
  }
};


template<template<typename T, size_t S> class Vec>
class PushPopTest : public FixedVectorTest<Vec> {
 public:
  PushPopTest() : FixedVectorTest<Vec>("push-pop") { }

 protected:
  virtual bool threaded_test() {
    int thread_count = ThreadedTest::get_thread_count();
    int per_thread_slots = kVectorSize  / thread_count;
    int iterations = per_thread_slots / kContiguity;

    for (int i = 0; i < iterations; i++) {
      for (int j = 0; j <  kContiguity; j++) {
        FixedVectorTest<Vec>::definite_push(kSampleValue);
      }
      for (int j = 0; j < kContiguity; j++) {
        int popped_value = FixedVectorTest<Vec>::definite_pop();
        check_i(popped_value, ==, kSampleValue, return false);
      }
    }

    return true;
  }

  virtual bool synch_verify() {
    check_i(FixedVectorTest<Vec>::vector_->length(), ==, 0, return false);
    return true;
  }

  static const int kContiguity = 8;
};


template<template<typename T, size_t S> class Vec>
class PushPopGetTest : public FixedVectorTest<Vec> {
 public:
  PushPopGetTest() : FixedVectorTest<Vec>("push-pop-get") { }

 protected:
  virtual bool threaded_test() {
    bool choice = rand() % 2 == 0;

    // I don't want to lock on histogram_mutex since locking in the
    // middle of the test might hide subtle bugs.
    int local_histogram[kLimit];
    fill(local_histogram, local_histogram + kLimit, 0);

    if (choice) {
      bool result = check_mutate(local_histogram) && check_get();
      update_global_histogram(local_histogram);
      return result;
    } else {
      bool result = check_get() && check_mutate(local_histogram);
      update_global_histogram(local_histogram);
      return result;
    }
  }

  bool check_mutate(int *histogram) {
    assert(kLimit <
           ((2 * kVectorSize) / ThreadedTest::get_thread_count()));
    for (int i = 0; i < kLimit; i++) {
      FixedVectorTest<Vec>::vector_->push_back(to_pointer(i));
      FixedVectorTest<Vec>::vector_->push_back(to_pointer(i));
      int value = FixedVectorTest<Vec>::definite_pop();
      histogram[value]++;
      check_i(value, <, kLimit, return false);
      check_i(value, >=, 0, return false);
    }
    return true;
  }

  bool check_get() {
    int index = 0;
    while (true) {
      long *value = FixedVectorTest<Vec>::vector_->get(index);
      if (FixedVector<long, kVectorSize>::is_out_of_range(value)) {
        return true;
      }
      int integer_value = to_integer(value);
      check_i(integer_value, <, kLimit, return false);
      check_i(integer_value, >=, 0, return false);
      index++;
    }
    return true;
  }

  void update_global_histogram(int *local_hist) {
    MutexLocker lock(&histogram_mutex_);
    for (int i = 0; i < kLimit; i++) {
      histogram_[i] += local_hist[i];
    }
  }

  virtual void synch_init() {
    FixedVectorTest<Vec>::synch_init();
    fill(histogram_, histogram_ + kLimit, 0);
  }

  virtual void synch_destroy() {
    FixedVectorTest<Vec>::synch_destroy();
  }

  virtual bool synch_verify() {
    int thread_count = ThreadedTest::get_thread_count();
    size_t expected_length = thread_count * kLimit;
    check_i(FixedVectorTest<Vec>::vector_->length(), ==, expected_length,
            return false);

    for (size_t i = 0; i < expected_length; i++) {
      check_i(expected_length, ==, FixedVectorTest<Vec>::vector_->length(),
              return false);
      int value = to_integer(FixedVectorTest<Vec>::vector_->get(i));
      check_i(value, <, kLimit, return false);
      check_i(value, >=, 0, return false);
      histogram_[value]++;
    }

    for (int i = 0; i < kLimit; i++) {
      check_i(histogram_[i], ==, 2 * thread_count, return false);
    }
    return true;
  }

  static const int kLimit = 32;
  int histogram_[kLimit];
  Mutex histogram_mutex_;
};

template<template<typename T, size_t Size> class Vec>
bool run_with_thread_count(bool quiet, int thread_count) {
  PushOnlyTest<Vec> push_only;
  PushPopTest<Vec> push_pop;
  PushPopGetTest<Vec> push_pop_get;

  return push_only.execute(quiet, thread_count) &&
      push_pop.execute(quiet, thread_count) &&
      push_pop_get.execute(quiet, thread_count);
}

}

int main() {
  for (int i = 1; i < 128; i++) {
    if (!run_with_thread_count<FixedVector>(false, i)) {
      return 1;
    }
  }
  return 0;
}
