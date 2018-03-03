BUILD_DIR = Build
CPPUTEST_HOME = /usr/local
PROJECT_HOME = $(shell cd ../../ && pwd)
MOCKS_HOME = ${PROJECT_HOME}/Mocks
INCLUDES = -I${CPPUTEST_HOME} -I${PROJECT_HOME}/Arduino -I${PROJECT_HOME}/LifeTester \
-I${PROJECT_HOME}/Mocks -I${PROJECT_HOME}/Common -I${PROJECT_HOME}/Hardware
LIBS = -L${CPPUTEST_HOME}/lib -lCppUTest -lCppUTestExt
DEBUG_FLAGS = -g -ggdb
DEFINES = -DUNIT_TEST

all: make_tests run_tests

make_tests:
	mkdir -p ${BUILD_DIR}
	g++ AllTests.cpp ${MOCKS_HOME}/MockArduino.c ${MOCKS_HOME}/TestUtils.cpp \
	${MOCKS_HOME}/MockLedFlash.cpp ${MOCKS_HOME}/MockIoWrapper.cpp TestIV.cpp \
	../IV.cpp ${INCLUDES} ${LIBS} ${DEBUG_FLAGS} ${DEFINES} -o ${BUILD_DIR}/tests

run_tests: make_tests
	./${BUILD_DIR}/tests

clean:
	rm -r ${BUILD_DIR}