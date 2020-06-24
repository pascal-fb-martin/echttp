
OBJS= echttp_static.o echttp.o echttp_raw.o echttp_catalog.o echttp_option.o echttp_json.o

all: libechttp.a echttp_jsonprint echttp_jsonget

install:
	mkdir -p /usr/local/lib
	cp libechttp.a /usr/local/lib
	chown root:root /usr/local/lib/libechttp.a
	chmod 644 /usr/local/lib/libechttp.a
	mkdir -p /usr/local/include
	cp echttp.h echttp_static.h echttp_json.h /usr/local/include
	chown root:root /usr/local/include/echttp*.h
	chmod 644 /usr/local/include/echttp*.h
	cp echttp_jsonprint echttp_jsonget /usr/local/bin
	chown root:root /usr/local/bin/echttp_*
	chmod 755 /usr/local/bin/echttp_*

clean:
	rm -f *.o *.a echttp_jsonprint echttp_jsonget

rebuild: clean all

uninstall:
	rm -f /usr/local/lib/libechttp.a
	rm -f /usr/local/include/echttp.h /usr/local/include/echttp_static.h
	rm -f /usr/local/bin/echttp_jsonprint /usr/local/bin/echttp_jsonget

purge: uninstall

%.o: %.c
	gcc -c -g -O -fPIC -o $@ $<

libechttp.a: $(OBJS)
	ar r $@ $^
	ranlib $@

echttp_jsonprint: echttp_jsonprint.o libechttp.a
	gcc -g -O -fPIC -o $@ echttp_jsonprint.o libechttp.a

echttp_jsonget: echttp_jsonget.o libechttp.a
	gcc -g -O -fPIC -o $@ echttp_jsonget.o libechttp.a

