#include "tests.hpp"
#include "fixed-vector.hpp"

#include <algorithm>
#include <cstdlib>

#include "platform.hpp"

using namespace eelish;
using namespace std;

class FixedVectorTest : public ThreadedTest {
 public:
  explicit FixedVectorTest(const string &subname) :
    ThreadedTest("fixed-vector-" + subname) {
  }

 protected:
  virtual void synch_init() {
    vector_ = new FixedVector<long, kVectorSize>;
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

  /// Instead of malloc'ing all the time, we simply cast integers to
  /// pointers.  FixedVector assumes things about how a pointer looks,
  /// so we need to shuffle some bits around.
  static long *to_pointer(int value) {
    intptr_t large_value = static_cast<intptr_t>(value);
    return reinterpret_cast<long *>(large_value << 2);
  }

  static int to_integer(long *pointer) {
    intptr_t integral_value = reinterpret_cast<intptr_t>(pointer);
    return static_cast<int>(integral_value >> 2);
  }

  static const int kVectorSize = 128 * 1024;

  FixedVector<long, kVectorSize> *vector_;
};


class PushOnlyTest : public FixedVectorTest {
 public:
  PushOnlyTest() : FixedVectorTest("push-only") { }

 protected:
  virtual bool threaded_test() {
    int iterations = kVectorSize / get_thread_count();

    for (int i = 0; i < iterations; i++) {
      vector_->push_back(to_pointer(42));
    }

    return true;
  }

  virtual bool synch_verify() {
    if (!FixedVectorTest::synch_verify()) {
      return false;
    }

    int per_thread_pushes = kVectorSize  / get_thread_count();
    size_t expected_length = per_thread_pushes * get_thread_count();

    check_i(vector_->length(), ==, expected_length, return false);

    for (size_t i = 0; i < expected_length; i++) {
      // Note that calling this macro like this is not safe with
      // several running threads.
      check_i(vector_->get(i), ==, to_pointer(42), return false);
    }

    return true;
  }
};


class PushPopTest : public FixedVectorTest {
 public:
  PushPopTest() : FixedVectorTest("push-pop") { }

 protected:
  virtual bool threaded_test() {
    int per_thread_slots = kVectorSize  / get_thread_count();
    int iterations = per_thread_slots / kContiguity;

    for (int i = 0; i < iterations; i++) {
      for (int j = 0; j <  kContiguity; j++) {
        definite_push(42);
      }
      for (int j = 0; j < kContiguity; j++) {
        int popped_value = definite_pop();
        check_i(popped_value, ==, 42, return false);
      }
    }

    return true;
  }

  virtual bool synch_verify() {
    check_i(vector_->length(), ==, 0, return false);
    return true;
  }

  static const int kContiguity = 8;
};


class PushPopGetTest : public FixedVectorTest {
 public:
  PushPopGetTest() : FixedVectorTest("push-pop-get") { }

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
    assert(kLimit < ((2 * kVectorSize) / get_thread_count()));
    for (int i = 0; i < kLimit; i++) {
      vector_->push_back(to_pointer(i));
      vector_->push_back(to_pointer(i));
      int value = definite_pop();
      histogram[value]++;
      check_i(value, <, kLimit, return false);
      check_i(value, >=, 0, return false);
    }
    return true;
  }

  bool check_get() {
    int index = 0;
    while (true) {
      long *value = vector_->get(index);
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
    FixedVectorTest::synch_init();
    fill(histogram_, histogram_ + kLimit, 0);
  }

  virtual void synch_destroy() {
    FixedVectorTest::synch_destroy();
  }

  virtual bool synch_verify() {
    size_t expected_length = get_thread_count() * kLimit;
    check_i(vector_->length(), ==, expected_length, return false);

    for (size_t i = 0; i < expected_length; i++) {
      check_i(expected_length, ==, vector_->length(), return false);
      int value = to_integer(vector_->get(i));
      check_i(value, <, kLimit, return false);
      check_i(value, >=, 0, return false);
      histogram_[value]++;
    }

    for (int i = 0; i < kLimit; i++) {
      check_i(histogram_[i], ==, 2 * get_thread_count(), return false);
    }
    return true;
  }

  static const int kLimit = 32;
  int histogram_[kLimit];
  Mutex histogram_mutex_;
};

bool run_with_thread_count(bool quiet, int thread_count) {
  PushOnlyTest push_only;
  PushPopTest push_pop;
  PushPopGetTest push_pop_get;

  return push_only.execute(quiet, thread_count) &&
      push_pop.execute(quiet, thread_count) &&
      push_pop_get.execute(quiet, thread_count);
}

int main() {
  for (int i = 1; i < 128; i++) {
    if (!run_with_thread_count(false, i)) {
      return 1;
    }
  }
  return 0;
}
