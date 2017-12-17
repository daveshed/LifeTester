BUILD_DIR = Build
CPPUTEST_HOME = /usr/local
PROJECT_HOME = $(shell cd ../../ && pwd)
INCLUDES = -I${CPPUTEST_HOME} -I${PROJECT_HOME}/Arduino -I${PROJECT_HOME}/Hardware
LIBS = -L${CPPUTEST_HOME}/lib -lCppUTest -lCppUTestExt

all: make_tests run_tests

make_tests:
	mkdir -p ${BUILD_DIR}
	g++ AllTests.cpp tests.cpp ../LedFlash.cpp ${INCLUDES} ${LIBS} -DUNIT_TEST -o ${BUILD_DIR}/tests

run_tests: make_tests
	./${BUILD_DIR}/tests

clean:
	rm -r ${BUILD_DIR}