SYSTEST_SUB_DIRS = $(sort $(dir $(wildcard SystemTest/*/)))

all: LifeTester SystemTest

LifeTester:
	cd LifeTester && make build

SystemTest:
	for dir in ${SYSTEST_SUB_DIRS}; do \
		${MAKE} -C $$dir build; \
	done

clean:
	for dir in ${SYSTEST_SUB_DIRS}; do \
		${MAKE} -C $$dir clean; \
	done

.PHONY: all LifeTester SystemTest