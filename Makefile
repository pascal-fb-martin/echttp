
OBJS= echttp.o \
      echttp_static.o \
      echttp_cors.o \
      echttp_tls.o \
      echttp_raw.o \
      echttp_hash.o \
      echttp_catalog.o \
      echttp_option.o \
      echttp_json.o \
      echttp_xml.o \
      echttp_parser.o

PUBLIC_INCLUDE=echttp.h \
              echttp_catalog.h \
              echttp_hash.h \
              echttp_static.h \
              echttp_cors.h \
              echttp_json.h \
              echttp_xml.h \
              echttp_parser.h

all: libechttp.a echttp_print echttp_get

install:
	mkdir -p /usr/local/lib
	cp libechttp.a /usr/local/lib
	chown root:root /usr/local/lib/libechttp.a
	chmod 644 /usr/local/lib/libechttp.a
	mkdir -p /usr/local/include
	cp $(PUBLIC_INCLUDE) /usr/local/include
	chown root:root /usr/local/include/echttp*.h
	chmod 644 /usr/local/include/echttp*.h
	cp echttp_print echttp_get /usr/local/bin
	chown root:root /usr/local/bin/echttp_*
	chmod 755 /usr/local/bin/echttp_*

clean:
	rm -f *.o *.a echttp_print echttp_get
	rm -f echttp_jsonprint echttp_jsonget

rebuild: clean all

uninstall:
	rm -f /usr/local/lib/libechttp.a
	rm -f /usr/local/include/echttp*.h
	rm -f /usr/local/bin/echttp_print /usr/local/bin/echttp_get
	rm -f /usr/local/bin/echttp_jsonprint /usr/local/bin/echttp_jsonget

purge: uninstall

%.o: %.c
	gcc -c -g -O -fPIC -o $@ $<

libechttp.a: $(OBJS)
	ar r $@ $^
	ranlib $@

echttp_print: echttp_print.o libechttp.a
	gcc -g -O -fPIC -o $@ echttp_print.o libechttp.a

echttp_get: echttp_get.o libechttp.a
	gcc -g -O -fPIC -o $@ echttp_get.o libechttp.a

