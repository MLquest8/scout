# Scout
# See LICENSE file for copyright and license details.

include config.mk

SRC = scout.c utils.c
OBJ = ${SRC:.c=.o}

all: options scout

options:
	@echo scout build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@
	chmod 666 $@

scout: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f scout ${OBJ} scout-${VERSION}.tar.gz

dist: clean
	mkdir -p scout-${VERSION}
	cp -R config.def.h config.mk LICENSE Makefile\
		README scout.1 ${SRC} utils.h scout-${VERSION}
	tar -cf scout-${VERSION}.tar scout-${VERSION}
	gzip scout-${VERSION}.tar
	rm -rf scout-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f scout ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/scout
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < scout.1 > ${DESTDIR}${MANPREFIX}/man1/scout.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/scout.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/scout\
		${DESTDIR}${MANPREFIX}/man1/scout.1

.PHONY: all options clean dist install uninstall

#for debugging
del:
	rm -f scout ${OBJ} config.h

new: del all