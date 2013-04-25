CFILES=xcwd.c
CC=gcc
CFLAGS=-Wall -Werror -std=gnu89 #-DDEBUG -g
LDFLAGS=-lX11
EXE="xcwd"
O=${CFILES:.c=.o}
prefix=/usr/

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

