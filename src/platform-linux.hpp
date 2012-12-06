#ifndef __EELISH_PLATFORM__HPP
#error "platform-linux.hpp can only be included from within platform-posix.hpp"
#endif

#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

namespace eelish {

void Platform::WaitOnMemory(int *location, int current_value) {
  int result = syscall(SYS_futex, location, FUTEX_WAIT, current_value, NULL,
                       NULL, 0);
  assert(result != -1);
  (void) result;
}

void Platform::WakeWaiters(int *location, int count_waiters) {
  int result = syscall(SYS_futex, location, FUTEX_WAKE, count_waiters, NULL,
                       NULL, 0);
  assert(result != -1);
  (void) result;
}

}
