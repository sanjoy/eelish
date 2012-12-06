#ifndef __EELISH_TESTS__HPP
#define __EELISH_TESTS__HPP

#include <map>
#include <string>

#include "platform.hpp"

namespace eelish {

class ThreadedTest {
 public:
  bool execute(bool quiet, int thread_count);

  virtual bool threaded_test() = 0;

 protected:
  explicit ThreadedTest(const std::string &name) :
      test_name_(name),
      quiet_(false) {
    initialize_platform();
  }

  /// The synch_* functions are all executed in the master thread.
  virtual void synch_init() { }
  virtual bool synch_verify() { return true; }
  virtual void synch_destroy() { }

  int get_thread_count() const { return thread_count_; }

  /// Printf style output function.  Prints things only if the tests
  /// isn't quiet.
  void output(const char *format, ...);
  void always_output(const char *format, ...);

  virtual ~ThreadedTest() { destroy_platform(); }

 private:
  std::string test_name_;
  int thread_count_;
  bool quiet_;

  struct PlatformData;
  PlatformData *platform_data_;
  void initialize_platform();
  void destroy_platform();
};

class CommandLine {
 public:
  enum ArgType {
    BOOL = 1,
    INTEGER,
    STRING,
  };

  struct Arg {
    ArgType type;
    union {
      bool boolean;
      long integer;
      const char *string;
    };
  };

  static void Parse(std::map<std::string, Arg> *meta,
                    int argc, char **argv);
};

class Timer {
 public:
  explicit inline Timer(long *out_time) :
    out_time_(out_time),
    begin_time_(eelish::Platform::CurrentTimeInUSec()) { }

  inline ~Timer() {
    long end_time = eelish::Platform::CurrentTimeInUSec();
    if (out_time_ != NULL) {
      *out_time_ = end_time - begin_time_;
    }
  }

 private:
  long *out_time_;
  long begin_time_;
};


/// Checks if two integral expressions are satisfy a binary condition.
/// `lhs` and `rhs` must be pure.  We reuse tests as benchmarks and
/// we'd like to have such checks impact the actual performance as
/// little as possible; hence the __builtin_expect.
#define check_i(lhs, op, rhs, fail_action) do {                         \
    if (unlikely(!((lhs) op (rhs)))) {                                  \
	 eelish::ThreadedTest::output("(%s: %d): expected " #lhs " "	\
				    #op " " #rhs " but "		\
             #lhs " = %d, " #rhs " = %d\n", __FILE__, __LINE__, lhs,    \
             rhs);                                                      \
      fail_action;                                                      \
    }                                                                   \
  } while(0)

}

#endif
