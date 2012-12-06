#include "tests.hpp"
#include "fixed-vector.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>

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

const int kVectorSize = 4 * 1024 * 1024;
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

struct TestConfig {
  int thread_count_lower;
  int thread_count_upper;
  bool quiet;
  bool push_only;
  bool push_pop;
  bool push_pop_get;
  string test_type;

  void read_config(int argc, char **argv) {
    map<string, CommandLine::Arg> arg_info;
    arg_info["quiet"].type = CommandLine::BOOL;
    arg_info["quiet"].boolean = false;

    arg_info["push-only"].type = CommandLine::BOOL;
    arg_info["push-only"].boolean = true;

    arg_info["push-pop"].type = CommandLine::BOOL;
    arg_info["push-pop"].boolean = true;

    arg_info["push-pop-get"].type = CommandLine::BOOL;
    arg_info["push-pop-get"].boolean = true;

    arg_info["thread-count-lower"].type = CommandLine::INTEGER;
    arg_info["thread-count-lower"].integer = 1;

    arg_info["thread-count-upper"].type = CommandLine::INTEGER;
    arg_info["thread-count-upper"].integer = 128;

    arg_info["test-type"].type = CommandLine::STRING;
    arg_info["test-type"].string = "real";

    CommandLine::Parse(&arg_info, argc, argv);

    quiet = arg_info["quiet"].boolean;
    push_only = arg_info["push-only"].boolean;
    push_pop = arg_info["push-pop"].boolean;
    push_pop_get = arg_info["push-pop-get"].boolean;

    thread_count_lower = arg_info["thread-count-lower"].integer;
    thread_count_upper = arg_info["thread-count-upper"].integer;
    test_type = arg_info["test-type"].string;
  }
};

template<template<typename T, size_t Size> class Vec>
bool run_with_thread_count(TestConfig *config, int thread_count) {
  bool quiet = config->quiet;
  bool result = true;

  if (config->push_only) {
    result &= PushOnlyTest<Vec>().execute(quiet, thread_count);
  }
  if (config->push_pop) {
    result &= PushPopTest<Vec>().execute(quiet, thread_count);
  }
  if (config->push_pop_get) {
    PushPopGetTest<Vec>().execute(quiet, thread_count);
  }

  return result;
}


template<template<typename T, size_t Size> class Vec>
bool run_tests_on_container(TestConfig *config) {
  for (int i = config->thread_count_lower;
       i <= config->thread_count_upper;
       i++) {
    if (!run_with_thread_count<Vec>(config, i)) return false;
  }
  return true;
}


}

int main(int argc, char **argv) {
  TestConfig config;
  config.read_config(argc, argv);
  bool success = true;
  long time_taken;

  {
    Timer timer(&time_taken);
    if (config.test_type == "real") {
      success = run_tests_on_container<FixedVector>(&config);
    } else if (config.test_type == "fake") {
      success = run_tests_on_container<NaiveFixedVector>(&config);
    } else {
      cerr << "unknown test type `" << config.test_type << "`" << endl;
    }
  }

  cout << time_taken / 1000.0d << endl;
  if (!success) return 1;
  return 0;
}
