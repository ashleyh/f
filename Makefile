.PHONY: all
all: build/f

.PHONY: clean
clean:
	rm -rf build

LIBUV_IN=$(CURDIR)/deps/libuv
LIBUV_OUT=$(CURDIR)/build/deps/libuv
CC?=clang
CFLAGS?=-std=gnu11 -Wall -Wpedantic -Wextra -Werror -g

PLATFORM=$(shell uname -s)
include $(PLATFORM).mk

SHAME_H = src/$(PLATFORM)/shame.h

$(LIBUV_IN)/autogen.sh:
	git submodule update --init $(LIBUV_IN)

$(LIBUV_IN)/configure: $(LIBUV_IN)/autogen.sh
	cd $(LIBUV_IN) && ./autogen.sh

$(LIBUV_IN)/Makefile: $(LIBUV_IN)/configure
	cd $(LIBUV_IN) && ./configure --prefix=$(LIBUV_OUT)

$(LIBUV_OUT): $(LIBUV_IN)/Makefile
	make -C $(LIBUV_IN) install

LIBUV_H = $(LIBUV_OUT)/include/uv.h

$(LIBUV_H): $(LIBUV_OUT)

LIBUV_A = $(LIBUV_OUT)/lib/libuv.a

$(LIBUV_A): $(LIBUV_OUT)

src/f.h: src/sl.h

src/filter.c: src/filter.h

src/filter.h: src/f.h

src/main.c: src/*.h $(SHAME_H)

src/scandir.c: src/*.h src/$(PLATFORM)/shame.h

build/%.o: src/%.c $(LIBUV_H)
	$(CC) \
	  $(CFLAGS) \
	  -I$(LIBUV_OUT)/include \
	  -Isrc/$(PLATFORM) \
	  -c \
	  -o $@ \
	  $<

build/f: build/main.o build/scandir.o build/filter.o $(LIBUV_A)
	$(CC) \
	  $(CFLAGS) \
	  $(LDFLAGS) \
	  -o $@ \
	  $^ \
	  $(LIBUV_OUT)/lib/libuv.a
