CFILES=xcwd.c
CC=gcc
CFLAGS=-Wall -Werror -std=gnu89 #-DDEBUG -g
LDFLAGS=-lX11
EXE="xcwd"
O=${CFILES:.c=.o}
prefix=/usr/

${EXE}: clean ${O}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${O}

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -vf *.o

distclean: clean
	rm -vf ${EXE}

install: ${EXE}
	install -m 0755 ${EXE} $(prefix)/bin

