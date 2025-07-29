prefix=/usr/local

INSTALL=/usr/bin/install

OBJS= echttp.o \
      echttp_static.o \
      echttp_cors.o \
      echttp_tls.o \
      echttp_raw.o \
      echttp_hash.o \
      echttp_sorted.o \
      echttp_catalog.o \
      echttp_encoding.o \
      echttp_option.o \
      echttp_json.o \
      echttp_reduce.o \
      echttp_xml.o \
      echttp_parser.o

PUBLIC_INCLUDE=echttp.h \
              echttp_encoding.h \
              echttp_catalog.h \
              echttp_hash.h \
              echttp_sorted.h \
              echttp_static.h \
              echttp_cors.h \
              echttp_json.h \
              echttp_reduce.h \
              echttp_xml.h \
              echttp_parser.h

all: libechttp.a echttp_print echttp_get

dev:
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/lib
	$(INSTALL) -m 0644 libechttp.a $(DESTDIR)$(prefix)/lib
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0644 $(PUBLIC_INCLUDE) $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0755 echttp_print echttp_get $(DESTDIR)$(prefix)/bin

install: dev

clean:
	rm -f *.o *.a echttp_print echttp_get
	rm -f echttp_jsonprint echttp_jsonget

rebuild: clean all

uninstall:
	rm -f $(DESTDIR)$(prefix)/lib/libechttp.a
	rm -f $(DESTDIR)$(prefix)/include/echttp*.h
	rm -f $(DESTDIR)$(prefix)/bin/echttp_print
	rm -f $(DESTDIR)$(prefix)/bin/echttp_get
	rm -f $(DESTDIR)$(prefix)/bin/echttp_jsonget
	rm -f $(DESTDIR)$(prefix)/bin/echttp_jsonprint

purge: uninstall

%.o: %.c
	gcc -c -Wall -g -Os -fPIC -o $@ $<

libechttp.a: $(OBJS)
	ar r $@ $^
	ranlib $@

echttp_print: echttp_print.o libechttp.a
	gcc -Os -fPIC -o $@ echttp_print.o libechttp.a

echttp_get: echttp_get.o libechttp.a
	gcc -Os -fPIC -o $@ echttp_get.o libechttp.a

