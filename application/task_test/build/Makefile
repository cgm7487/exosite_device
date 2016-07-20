CC = gcc
CFLAGS = -g -Wall
INC = -I source -I include -I ../../../library/pt/include
LDFLAGS += -lrt -lm -pthread -static

.PHONY: clean

all: linux-test

linux-test: ../src/task_test.c
	${CC} ${CFLAGS} ${INC} ../src/task_test.c \
	     -o ../bin/task_test ${LDFLAGS}

clean:
	@rm -rf *.o *.a ../bin/* ../source/*.o
