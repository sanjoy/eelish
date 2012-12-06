#ifndef __EELISH_PLATFORM__HPP
#define __EELISH_PLATFORM__HPP

#define unlikely(condition) __builtin_expect((condition), 0)
#define likely(condition) __builtin_expect((condition), 0)

namespace eelish {

class Platform {
 public:
  static inline long CurrentTimeInUSec();
  static void inline WaitOnMemory(int *location, int current_value);
  static void inline WakeWaiters(int *location, int count_waiters);
};

};

#include "platform-posix.hpp"

#endif
