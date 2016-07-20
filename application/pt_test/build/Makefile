CC = gcc
CFLAGS = -g -Wall
INC = -I source -I include -I ../../../library/pt/include
LDFLAGS += -lrt -lm -pthread -static

.PHONY: clean

all: linux-test

linux-test: ../src/example-small.c
	${CC} ${CFLAGS} ${INC} ../src/example-small.c \
	     -o ../bin/example-small ${LDFLAGS}

clean:
	@rm -rf *.o *.a ../bin/* ../source/*.o
