CFILES=xcwd.c
CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -O2
LDFLAGS=-lX11
EXE=xcwd
prefix=/usr
UNAME:=$(shell uname)
O=${CFILES:.c=.o}

.PHONY: all clean distclean install
.SUFFIXES: .c .o

ifeq ($(UNAME), Linux)
    CFLAGS += -DLINUX
else ifeq ($(UNAME), FreeBSD)
    CFLAGS += -I/usr/local/include/ -DFREEBSD
    LDFLAGS += -L/usr/local/lib -lutil
else ifeq ($(UNAME), OpenBSD)
    CC=cc
    CFLAGS += -I /usr/X11R6/include -DOPENBSD
    LDFLAGS +=-L /usr/X11R6/lib
else
   $(error Operating System not supported.)
endif

all: ${EXE}

clean:
	rm -vf *.o

distclean: clean
	rm -vf ${EXE}

install: ${EXE}
	install -m 0755 ${EXE} $(prefix)/bin


${EXE}: ${O}
	${CC} -o $@ ${O} ${CFLAGS} ${LDFLAGS}


.c.o:
	${CC} -c $< ${CFLAGS}

