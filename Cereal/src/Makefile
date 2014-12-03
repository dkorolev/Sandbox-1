.PHONY: all indent clean check

INCLUDE_PATH=../3party/cereal-1.0.0/include
CPP=g++
CPPFLAGS=-std=c++11 -O3 -Wall -I ${INCLUDE_PATH}
LDFLAGS=-pthread -lgflags

PWD=$(shell pwd)
SRC=$(wildcard *.cc)
BIN=$(SRC:%.cc=build/%)

demo: build build/demo
	./build/demo

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

build: ../3party/cereal-1.0.0
	mkdir -p build

../3party/cereal-1.0.0:
	(cd ../3party; make)

build/%: %.cc *.h
	${CPP} ${CPPFLAGS} -o $@ $< ${LDFLAGS}
