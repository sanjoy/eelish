#ifndef __EELISH_PLATFORM__HPP
#define __EELISH_PLATFORM__HPP

// TODO: Check for POSIX here
#include "platform-posix.hpp"

namespace eelish {

class MutexLocker {
 public:
  explicit inline MutexLocker(Mutex *mutex) : mutex_(mutex) {
    mutex_->lock();
  }

  inline ~MutexLocker() { mutex_->unlock(); }

 private:
  Mutex *mutex_;
};

};

#endif
