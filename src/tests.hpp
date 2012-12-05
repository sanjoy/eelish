#ifndef __EELISH_TESTS__HPP
#define __EELISH_TESTS__HPP

#include <string>

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

}

/// Checks if two integral expressions are satisfy a binary condition.
/// `lhs` and `rhs` must be pure.
#define check_i(lhs, op, rhs, fail_action) do {                         \
    if (!((lhs) op (rhs))) {                                            \
	 eelish::ThreadedTest::output("(%s: %d): expected " #lhs " "	\
				    #op " " #rhs " but "		\
             #lhs " = %d, " #rhs " = %d\n", __FILE__, __LINE__, lhs,    \
             rhs);                                                      \
      fail_action;                                                      \
    }                                                                   \
  } while(0)

#endif
