#ifndef __EELISH_PLATFORM__HPP
#define __EELISH_PLATFORM__HPP

#include "atomics.hpp"

namespace eelish {

class Platform {
 public:
  static inline long CurrentTimeInUSec();
  static inline void Sleep(int usecs);

  template<typename T>
  static void inline WaitOnMemory(Atomic<T> location, T current_value);

  template<typename T>
  static void inline WakeWaiters(Atomic<T> location, int num_waiters);
};

};

#include "platform-posix.hpp"

#endif
