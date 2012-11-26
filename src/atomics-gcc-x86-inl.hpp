#ifndef __EELISH_ATOMICS__HPP
#error "atomics-gcc-x86-inl.hpp can only be included from within \
        atomics-gcc-inl.hpp"
#endif

namespace eelish {

template<typename T>
inline void flush_cache(volatile T *begin, volatile T *end) { }

}
