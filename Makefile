CFILES=xcwd.c
CC=cc
CFLAGS=-Wall -Wextra -O2
LDFLAGS=-L/usr/X11R6/lib -lX11
EXE=xcwd
prefix=/usr
UNAME:=$(shell uname)
O=${CFILES:.c=.o}

.PHONY: all clean distclean install
.SUFFIXES: .c .o

ifeq ($(UNAME), Linux)
    CFLAGS += -DLINUX
else
    ifeq ($(UNAME), OpenBSD)
        CFLAGS += -DBSD -I /usr/X11R6/include
        #LDFLAGS += -lutil
    else
        $(error Operating System not supported.)
    endif
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

