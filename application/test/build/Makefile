CC = gcc
CFLAGS = -g -Wall -std=c99
INC = -I ../../../include/exosite_http/include -I ../../../include/exosite_http/include/platform -I ../../../library/pt/include
LDFLAGS += -lrt -lm -pthread -static -L../../../lib -lexositehttp

.PHONY: clean

all: release linux-test

.c.o:
	${CC} ${CFLAGS} -lstatic ${INC} -Os -c ../../../library/exosite_http/source/exosite_http.c \
	     -o ../../../library/exosite_http/source/exosite_http.o

	${CC} ${CFLAGS} -lstatic ${INC} -Os -c ../../../library/exosite_http/source/utility.c \
	     -o ../../../library/exosite_http/source/utility.o

	${CC} ${CFLAGS} -lstatic ${INC} -Os -c ../../../library/exosite_http/source/platform/exosite_pal_linux_non_security.c \
	     -o ../../../library/exosite_http/source/platform/exosite_pal.o

release: ../../../library/exosite_http/source/exosite_http.o ../../../library/exosite_http/source/utility.o ../../../library/exosite_http/source/platform/exosite_pal.o
	ar rcs ../../../lib/libexositehttp.a ../../../library/exosite_http/source/exosite_http.o ../../../library/exosite_http/source/utility.o ../../../library/exosite_http/source/platform/exosite_pal.o

linux-test: ../source/test_linux_socket.c
	${CC} ${CFLAGS} ${INC} ../source/test_linux_socket.c \
	     -o ../bin/test_linux ${LDFLAGS}

clean:
	@rm -rf *.o *.a ../bin/* ../../../lib/libexositehttp.a ../../../library/exosite_http/source/*.o ../../../library/exosite_http/source/platform/*.o
