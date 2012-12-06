#ifndef __EELISH_PLATFORM__HPP
#error "platform-linux.hpp can only be included from within platform-posix.hpp"
#endif

#include <cstdio>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include "utils.hpp"

namespace eelish {

template<typename T>
void Platform::WaitOnMemory(Atomic<T> location, T current_value) {
  assert_static(sizeof(T) == 2 * sizeof(int) || sizeof(T) == sizeof(int));

  volatile int *uaddr;
  if (sizeof(T) == 2 * sizeof(int)) {
    uaddr = reinterpret_cast<volatile int *>(location.raw_location()) + 1;
  } else {
    uaddr = reinterpret_cast<volatile int *>(location.raw_location());
  }
  int int_value = static_cast<int>(reinterpret_cast<intptr_t>(current_value));

  printf("  waiting on %p (%p, %d)!\n", uaddr, current_value, int_value);
  int result = syscall(SYS_futex, uaddr, FUTEX_WAIT, int_value, NULL, NULL, 0);
  printf("  done!\n");

  assert(result != -1);
  (void) result;
}

template<typename T>
void Platform::WakeWaiters(Atomic<T> location, int num_waiters) {
  assert_static(sizeof(T) == 2 * sizeof(int) || sizeof(T) == sizeof(int));

  volatile int *uaddr;
  if (sizeof(T) == 2 * sizeof(int)) {
    uaddr = reinterpret_cast<volatile int *>(location.raw_location()) + 1;
  } else {
    uaddr = reinterpret_cast<volatile int *>(location.raw_location());
  }

  int result = syscall(SYS_futex, uaddr, FUTEX_WAKE, num_waiters, NULL,
                       NULL, 0);

  if (result != 0)
    printf("woke %d workers %p\n", result, uaddr);
  assert(result != -1);
  (void) result;
}

}
