BUILD_DIR = Build
CPPUTEST_HOME = /usr/local
PROJECT_HOME = $(shell cd ../../ && pwd)
MOCKS_HOME = Mocks
ARDUINO_MOCK = ${PROJECT_HOME}/Hardware/tests/Support
INCLUDES = -I${CPPUTEST_HOME} -I${PROJECT_HOME}/Arduino \
-I${PROJECT_HOME}/LifeTester -I${PROJECT_HOME}/Mocks -I${PROJECT_HOME}/Common \
-I${PROJECT_HOME}/Hardware -I${PROJECT_HOME}/Hardware/tests/Support
LIBS = -L${CPPUTEST_HOME}/lib -lCppUTest -lCppUTestExt
DEBUG_FLAGS = -g -ggdb
DEFINES = -DUNIT_TEST

# note that tests are built separately - mock function implementations for state-
# machine are needed in controller tests.
all: make_test_controller make_test_statemachine run_tests

debug: DEFINES += -DDEBUG
debug: all

make_test_controller:
	@echo "********************************************************************"
	@echo "Building tests for Controller.cpp"
	@echo "********************************************************************"
	mkdir -p ${BUILD_DIR}
	g++ AllTests.cpp ${MOCKS_HOME}/MockLedFlash.cpp \
	${MOCKS_HOME}/MockIoWrapper.cpp ${ARDUINO_MOCK}/MockArduino.c \
	${MOCKS_HOME}/MockConfig.cpp TestController.cpp ../Controller.cpp \
	${INCLUDES} ${LIBS} ${DEBUG_FLAGS} ${DEFINES} -o ${BUILD_DIR}/TestController

make_test_statemachine:
	@echo "********************************************************************"
	@echo "Building tests for Statemachine.cpp"
	@echo "********************************************************************"
	mkdir -p ${BUILD_DIR}
	g++ AllTests.cpp ${MOCKS_HOME}/MockLedFlash.cpp \
	${MOCKS_HOME}/MockConfig.cpp ${MOCKS_HOME}/MockIoWrapper.cpp \
	${ARDUINO_MOCK}/MockArduino.c ../StateMachine.cpp TestStateMachine.cpp \
	${INCLUDES} ${LIBS} ${DEBUG_FLAGS} ${DEFINES} -o ${BUILD_DIR}/TestStateMachine

run_tests: make_test_controller make_test_statemachine
	./${BUILD_DIR}/TestStateMachine
	./${BUILD_DIR}/TestController

clean:
	rm -r ${BUILD_DIR}