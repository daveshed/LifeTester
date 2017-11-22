# makefile that replaces the arduino IDE.
# visit http://theonlineshed.com/make-unix
# 1. compiles all c/cpp files in the working directory plus all the files in the
# arduino cores directory too.
# 2. All files are linked into .elf
# 3. .elf is converted to .hex
# 4. finally the .hex is uploaded on the specified port
# You can optionally compile only or upload only. Both happen by default

PROGRAM=MyBlinkTest
VPATH=/usr/share/arduino/hardware/arduino/cores/arduino
VARIANTS=/usr/share/arduino/hardware/arduino/variants/standard
# All c and cpp files in the arduino cores are compiled to o files
DEPS=${VPATH}/*.c ${VPATH}/*.cpp MyBlinkTest.cpp
BUILD_DIR=Build
CC=avr-gcc
OBJCOPY=avr-objcopy
MMCU=-mmcu=atmega328p
CFLAGS=-Os -DF_CPU=16000000UL ${MMCU}
PORT=/dev/ttyACM0

# $@ = name of the target recipe being run
# $< name of first prerequisite
# $@ and $< are two of the so-called internal macros (also known as automatic
# variables) and stand for the target name and "implicit" source, respectively.
# $^ expands to a space delimited list of the prerequisites

# dependency should define build order but written explicitly here
all: ${BUILD_DIR}/*.o ${PROGRAM}.elf ${PROGRAM}.hex upload

# option to compile only without upload/install
compile: ${BUILD_DIR}/*.o ${PROGRAM}.elf ${PROGRAM}.hex

# compile rule - all files in DEPS list turned into .o
# then move to build directory
${BUILD_DIR}/*.o: ${DEPS}
	mkdir -p Build/
	${CC} ${CFLAGS} -c $^ -I ${VARIANTS} -I ${VPATH}
	mv *.o ${BUILD_DIR}/

# linking rule
${PROGRAM}.elf: ${BUILD_DIR}/*.o
	${CC} ${MMCU} $^ -o ${BUILD_DIR}/$@

# make hex file - the default goal must go first
${PROGRAM}.hex: ${BUILD_DIR}/${PROGRAM}.elf
	${OBJCOPY} -O ihex -R .eeprom $< ${BUILD_DIR}/$@

upload: ${BUILD_DIR}/${PROGRAM}.hex
	avrdude -F -V -c arduino -p ATMEGA328P -P ${PORT} -b 115200 -U flash:w:${BUILD_DIR}/$<

clean:
	rm -f ${BUILD_DIR}/*