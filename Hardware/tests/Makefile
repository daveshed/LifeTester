BUILD_DIR = Build
CPPUTEST_HOME = /usr/local
PROJECT_HOME = $(shell cd ../../ && pwd)
INCLUDES = -I${CPPUTEST_HOME} -I${PROJECT_HOME}/Arduino -I${PROJECT_HOME}/Hardware \
-ISupport/ -I${PROJECT_HOME}/Common
LIBS = -L${CPPUTEST_HOME}/lib -lCppUTest -lCppUTestExt
DEBUG_FLAGS = -g -ggdb
DEFINES = -DUNIT_TEST

all: make_tests run_tests

make_tests:
	mkdir -p ${BUILD_DIR}
	g++ AllTests.cpp Support/MockArduino.c Support/MockSpiCommon.cpp \
	TestLedFlash.cpp ../LedFlash.cpp TestTC77.cpp ../TC77.cpp \
	TestMX7705.cpp ../MX7705.cpp TestMCP4802.cpp ../MCP4802.cpp \
	${INCLUDES} ${LIBS} ${DEBUG_FLAGS} ${DEFINES} -o ${BUILD_DIR}/tests

run_tests: make_tests
	./${BUILD_DIR}/tests

clean:
	rm -r ${BUILD_DIR}