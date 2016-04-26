CFILES=xcwd.c
CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -O2
LDFLAGS=-lX11
EXE="xcwd"
prefix=/usr
UNAME:=$(shell uname)
O=${CFILES:.c=.o}

ifeq ($(UNAME), Linux)
    CFLAGS += -DLINUX
else
    ifeq ($(UNAME), FreeBSD)
        CFLAGS += -I/usr/local/include/ -DBSD
        LDFLAGS += -L/usr/local/lib -lutil
    else
        $(error Operating System not supported.)
    endif
endif

${EXE}: clean ${O}
	${CC} -o $@ ${O} ${CFLAGS} ${LDFLAGS}

.SUFFIXES: .c .o
.c.o:
	${CC} -c $< ${CFLAGS}

clean:
	rm -vf *.o

distclean: clean
	rm -vf ${EXE}

install: ${EXE}
	install -m 0755 ${EXE} $(prefix)/bin

