#ifndef __EELISH_PLATFORM__HPP
#error "platform-posix.hpp can only be included from within platform.hpp"
#endif

#include <cassert>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

namespace eelish {

class Mutex {
 public:
  inline Mutex() { pthread_mutex_init(&mutex_, NULL); }

  inline void lock() {
    int result = pthread_mutex_lock(&mutex_);
    assert(result == 0 && "pthread_mutex_lock failed!");
    (void) result;
  }

  inline bool try_lock() { return pthread_mutex_trylock(&mutex_) == 0; }

  inline void unlock() {
    pthread_mutex_unlock(&mutex_);
  }

  inline ~Mutex() {
    pthread_mutex_destroy(&mutex_);
  }

 private:
  pthread_mutex_t mutex_;
};

long Platform::CurrentTimeInUSec() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return time.tv_sec * 1000000 + time.tv_usec;
}

void Platform::Sleep(int usecs) {
  usleep(usecs);
}

}

#include "platform-linux.hpp"
