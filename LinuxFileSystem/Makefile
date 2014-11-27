.PHONY: test all indent clean check coverage

# TODO(dkorolev): Remove Google libraries.

CPP=g++
CPPFLAGS=-std=c++11 -O3 -Wall
LDFLAGS=-pthread -lgflags
CPPFLAGS_FOR_COVERAGE=${CPPFLAGS} -fprofile-arcs -ftest-coverage
LDFLAGS_FOR_COVERAGE=${LDFLAGS}

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=build/%)
BIN_FOR_COVERAGE=$(SRC:%.cc=build/coverage/%)

test: all
	rm -rf .tmp
	mkdir .tmp
	./build/test && rm -rf .tmp

all: build ${BIN}

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	rm -rf build

check: build build/CHECK_OK

build/CHECK_OK: build *.h
	for i in *.h ; do \
		echo -n $(basename $$i)': ' ; \
		ln -sf ${PWD}/$$i ${PWD}/build/$$i.cc ; \
		if [ ! -f build/$$i.h.o -o build/$$i.h.cc -nt build/$$i.h.o ] ; then \
			${CPP} -I . ${CPPFLAGS} -c build/$$i.cc -o build/$$i.h.o ${LDFLAGS} || exit 1 ; echo 'OK' ; \
		else \
			echo 'Already OK' ; \
		fi \
	done && echo OK >$@

build:
	mkdir -p $@

build/%: %.cc *.h
	# TODO(dkorolev): Get rid of Google dependencies here.
	${CPP} ${CPPFLAGS} -o $@ $< /usr/src/gtest/libgtest.a ${LDFLAGS}

build/coverage:
	mkdir -p $@

build/coverage/%: %.cc *.h
	# TODO(dkorolev): Get rid of Google dependencies here.
	${CPP} ${CPPFLAGS_FOR_COVERAGE} -o $@ $< /usr/src/gtest/libgtest.a ${LDFLAGS_FOR_COVERAGE}

coverage: build/coverage ${BIN_FOR_COVERAGE}
	rm -rf .tmp; mkdir .tmp; ./build/coverage/test && rm -rf .tmp
	gcov test.cc
	geninfo . --output-file coverage.info
	genhtml coverage.info --output-directory build/coverage | grep -A 2 "^Overall"
	rm -rf coverage.info *.gcov *.gcda *.gcno
	echo ${PWD}/build/coverage/index.html

