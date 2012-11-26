#include "tests.hpp"

#include <cstdarg>
#include <cstdio>

using namespace eelish;
using namespace std;

void ThreadedTest::output(const char *format, ...) {
  if (!quiet_) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
  }
}
