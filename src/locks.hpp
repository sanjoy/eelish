#ifndef __EELISH_LOCKS__HPP
#define __EELISH_LOCKS__HPP

#include "platform.hpp"

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

}

#endif
