# Makes the Lifetester project and uploads - over serial connection/Atmeag328 breadboard
# 1. compiles all c/cpp files in the working directory plus all the files in the
# arduino cores directory too.
# 2. All files are linked into .elf
# 3. .elf is converted to .hex
# 4. finally the .hex is uploaded on the specified port
# You can optionally compile only or upload only. Both happen by default

# options:
# make debug - gives you extra debug information over serial
# make PORT=/dev/ttyACM1 - manually specifies port. Defaults to ...ACM0
# make I2C_ADDRESS=0x0B - manually specifies I2C address. Defaults to 0x0A

PROGRAM=Lifetester
LIBS=../Arduino ../LifeTester ../Hardware ../Common
IDIRS=$(patsubst %,-I%,$(subst :, ,$(LIBS)))
DEPS=*.c* ../Arduino/*.c* ../Arduino/*.S *.c* ../Hardware/*.c* ../Common/*.c* 
BUILD_DIR=Build
CC=avr-gcc
OBJCOPY=avr-objcopy
MMCU=-mmcu=atmega328p
I2C_ADDRESS?=0x0A
CFLAGS=-Os -DI2C_ADDRESS=${I2C_ADDRESS} -DF_CPU=16000000UL ${MMCU}
UPLOADER=avrdude
PORT?=/dev/ttyACM0

# $@ = name of the target recipe being run
# $< name of first prerequisite
# $@ and $< are two of the so-called internal macros (also known as automatic
# variables) and stand for the target name and "implicit" source, respectively.
# $^ expands to a space delimited list of the prerequisites

# dependency should define build order but written explicitly here
all: ${BUILD_DIR}/*.o ${PROGRAM}.elf ${PROGRAM}.hex upload

debug: CFLAGS += -DDEBUG
debug: all

# option to compile only without upload/install
build: ${BUILD_DIR}/*.o ${PROGRAM}.elf ${PROGRAM}.hex

# compile rule - all files in DEPS list turned into .o
# then move to build directory
${BUILD_DIR}/*.o: ${DEPS}
	mkdir -p ${BUILD_DIR}/
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
	rm -r ${BUILD_DIR}