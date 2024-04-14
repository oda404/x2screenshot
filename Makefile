
PREFIX ?= /usr/local

SRC = main.c
OBJS = ${SRC:%.c=%.c.o}

CFLAGS = -Ofast
LIBS = -lX11 

X2SCREENSHOT = x2screenshot

CC ?= clang

%.c.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

${X2SCREENSHOT}: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} ${LIBS} -o $@

install: ${X2SCREENSHOT}
	cp ${X2SCREENSHOT} ${PREFIX}/bin/${X2SCREENSHOT}

uninstall:
	rm -f ${PREFIX}/bin/${X2SCREENSHOT}
