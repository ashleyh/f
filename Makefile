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

ifeq ($(PLATFORM),Linux)
  include linux.mk
else
  include generic.mk
endif

$(LIBUV_IN)/autogen.sh:
	git submodule update --init $(LIBUV_IN)

$(LIBUV_IN)/configure: $(LIBUV_IN)/autogen.sh
	cd $(LIBUV_IN) && ./autogen.sh

$(LIBUV_IN)/Makefile: $(LIBUV_IN)/configure
	cd $(LIBUV_IN) && ./configure --prefix=$(LIBUV_OUT)

$(LIBUV_OUT)/lib/libuv.a: $(LIBUV_IN)/Makefile
	make -C $(LIBUV_IN) install

build/%.o: src/%.c $(LIBUV_OUT)/lib/libuv.a build/gen/shame.h
	$(CC) \
	  $(CFLAGS) \
	  -I$(LIBUV_OUT)/include \
	  -Ibuild/gen \
	  -c \
	  -o $@ $<

build/f: build/main.o
	$(CC) \
	  $(CFLAGS) \
	  $(LDFLAGS) \
	  -o $@ \
	  $< \
	  $(LIBUV_OUT)/lib/libuv.a
