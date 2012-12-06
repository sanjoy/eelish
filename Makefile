.PHONY: all clean

CXX=g++
CXXFLAGS=-Wall -Werror -O4 -Isrc/ -DNDEBUG
LD=g++
LDFLAGS=-lpthread
BUILD_DIR=build

common-headers=$(addprefix src/, atomics.hpp			\
                                 atomics-gcc-inl.hpp		\
                                 atomics-gcc-x86-inl.hpp	\
                                 locks.hpp			\
                                 platform.hpp			\
                                 platform-linux.hpp		\
                                 platform-posix.hpp		\
                                 tests.hpp)
fixed-vector-headers=$(addprefix src/, fixed-vector.hpp fixed-vector-inl.hpp)
common-objects=$(addprefix ${BUILD_DIR}/, tests.o tests-pthread.o)

all: ${BUILD_DIR}/.d ${BUILD_DIR}/test-fixed-vector
clean:
	rm -rf ${BUILD_DIR}

${BUILD_DIR}/tests.o: ${common-headers} src/tests.cpp
	${CXX} ${CXXFLAGS} -c src/tests.cpp -o $@

${BUILD_DIR}/tests-pthread.o: ${common-headers} src/tests-pthread.cpp
	${CXX} ${CXXFLAGS} -c src/tests-pthread.cpp -o $@

${BUILD_DIR}/test-fixed-vector.o: ${common-headers} ${fixed-vector-headers} \
	src/test-fixed-vector.cpp
	${CXX} ${CXXFLAGS} -c src/test-fixed-vector.cpp -o $@

${BUILD_DIR}/test-fixed-vector: ${BUILD_DIR}/test-fixed-vector.o ${common-objects}
	${LD} ${LDFLAGS} ${BUILD_DIR}/test-fixed-vector.o ${common-objects} -o $@

${BUILD_DIR}/.d:
	mkdir -p $(BUILD_DIR)
	touch $@
