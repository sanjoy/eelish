#include "tests.hpp"

#include <iostream>
#include <pthread.h>
#include <stdint.h>

using namespace std;
using namespace eelish;

struct ThreadedTest::PlatformData {
  pthread_mutex_t condition_mutex;
  pthread_cond_t condition;
};

void ThreadedTest::initialize_platform() {
  platform_data_ = new PlatformData();
  pthread_mutex_init(&platform_data_->condition_mutex, NULL);
  pthread_cond_init(&platform_data_->condition, NULL);
}

void ThreadedTest::destroy_platform() {
  pthread_mutex_destroy(&platform_data_->condition_mutex);
  pthread_cond_destroy(&platform_data_->condition);
  delete platform_data_;
}

static void *thread_function(void *data) {
  ThreadedTest *test = reinterpret_cast<ThreadedTest *>(data);
  bool result = test->threaded_test();
  return reinterpret_cast<void *>(static_cast<uintptr_t>(result));
}
bool ThreadedTest::execute(bool quiet, int thread_count) {
  // TODO: Have thread_count default to 1.5 times the number of cores.
  // At the very least, have a function that returns the number of
  // cores so that the actual tests can choose a sane value for
  // thread_count.

  quiet_ = quiet;

  output("executing %s with %d threads\n", test_name_.c_str(), thread_count);

  thread_count_ = thread_count;

  bool successful = true;
  synch_init();

  pthread_t *thread_ids = new pthread_t[thread_count];
  for (int i = 0; i < thread_count; i++) {
    pthread_create(&thread_ids[i], NULL, thread_function, this);
  }

  for (int i = 0; i < thread_count; i++) {
    void *test_successful;
    pthread_join(thread_ids[i], &test_successful);
    successful &=
        static_cast<bool>(reinterpret_cast<uintptr_t>(test_successful));
  }

  if (successful) {
    successful = synch_verify();
  }

  synch_destroy();

  if (successful) {
    output("%s passed!\n", test_name_.c_str());
  } else {
    output("%s failed!\n", test_name_.c_str());
  }

  return successful;
}
