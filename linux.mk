CFLAGS+=-pthread
LDFLAGS+=-lbsd

build/gen/shame.h:
	mkdir -p build/gen
	echo '#include <bsd/string.h>' > build/gen/shame.h
