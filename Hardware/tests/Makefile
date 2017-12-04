# autogenerates mock functions from c files only using Cmock

# configuration options
CONFIG=project.yml
# things to mock
HEADERS=../../Arduino/Arduino.h
# path to cmock
CMOCK=/var/lib/gems/2.3.0/gems/ceedling-0.28.2/vendor/cmock/lib/cmock.rb
# mocks output directory
MOCK_DIR=mocks
# put buld files in another directory
BUILD_DIR=Build

run_gtests: ${BUILD_DIR}/runTests
	./$<
	
# builds executable
${BUILD_DIR}/runTests: ${BUILD_DIR}/Makefile 
	cd ${BUILD_DIR} && ${MAKE}

# builds makefile
${BUILD_DIR}/Makefile: ${MOCK_DIR}/Mock*
	mkdir -p ${BUILD_DIR}/
	cmake -H/home/dave/sketchbook/LifeTester_Unit_Tests/Hardware/tests -B/home/dave/sketchbook/LifeTester_Unit_Tests/Hardware/tests/Build/

# rule to make mock files
${MOCK_DIR}/Mock*: ${HEADERS}
	mkdir -p ${MOCK_DIR}/
	ruby ${CMOCK} -o${CONFIG} ${HEADERS}

clean:
	rm -rf ${MOCK_DIR}
	rm -rf ${BUILD_DIR}