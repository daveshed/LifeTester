# makefile that replaces the arduino IDE.
# visit http://theonlineshed.com/make-unix
# 1. compiles all c/cpp files in the working directory plus all the files in the
# arduino cores directory too.
# 2. All files are linked into .elf
# 3. .elf is converted to .hex
# 4. finally the .hex is uploaded on the specified port
# You can optionally compile only or upload only. Both happen by default

PROGRAM=MX7705Test
LIBS=../../Arduino ../../LifeTester ../../Hardware ../../Common
IDIRS=$(patsubst %,-I%,$(subst :, ,$(LIBS)))
DEPS=${PROGRAM}.cpp ../../Arduino/*.c* ../../Arduino/*.S ../../Hardware/MX7705.cpp ../../Common/*.c*
BUILD_DIR=Build
CC=avr-gcc
OBJCOPY=avr-objcopy
MMCU=-mmcu=atmega328p
DEFINES=-DDEBUG
CFLAGS=-Os -DF_CPU=16000000UL ${MMCU} ${DEFINES}
UPLOADER=avrdude
PORT=/dev/ttyACM0

# $@ = name of the target recipe being run
# $< name of first prerequisite
# $@ and $< are two of the so-called internal macros (also known as automatic
# variables) and stand for the target name and "implicit" source, respectively.
# $^ expands to a space delimited list of the prerequisites

# dependency should define build order but written explicitly here
all: ${BUILD_DIR}/*.o ${PROGRAM}.elf ${PROGRAM}.hex upload

# option to compile only without upload/install
build: ${BUILD_DIR}/*.o ${PROGRAM}.elf ${PROGRAM}.hex

# compile rule - all files in DEPS list turned into .o
# then move to build directory
${BUILD_DIR}/*.o: ${DEPS}
	mkdir -p Build/
	${CC} ${CFLAGS} -c $^ ${IDIRS}
	mv *.o ${BUILD_DIR}/

# linking rule
${PROGRAM}.elf: ${BUILD_DIR}/*.o
	${CC} ${MMCU} $^ -o ${BUILD_DIR}/$@

# make hex file - the default goal must go first
${PROGRAM}.hex: ${BUILD_DIR}/${PROGRAM}.elf
	${OBJCOPY} -O ihex -R .eeprom $< ${BUILD_DIR}/$@

upload: ${BUILD_DIR}/${PROGRAM}.hex
	${UPLOADER} -V -c arduino -p ATMEGA328P -P ${PORT} -b 57600 -D -U flash:w:$<

clean:
	rm -f ${BUILD_DIR}/*