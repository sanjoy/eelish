#ifndef __EELISH_UTILS__HPP
#define __EELISH_UTILS__HPP

namespace eelish {

template<bool Condition> struct STATIC_ASSERT_FAILED;
template<> struct STATIC_ASSERT_FAILED<true> { };

#define assert_static(condition)                                \
  enum { static_assert_dummy =                                  \
         sizeof(STATIC_ASSERT_FAILED < (bool) (condition) >) };

#define unlikely(condition) __builtin_expect((condition), 0)
#define likely(condition) __builtin_expect((condition), 0)

}

#endif
