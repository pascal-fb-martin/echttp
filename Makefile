
OBJS= echttp_static.o echttp.o echttp_raw.o echttp_catalog.o echttp_option.o

all: libechttp.a

install:
	mkdir -p /usr/local/lib
	cp libechttp.a /usr/local/lib
	chown root:root /usr/local/lib/libechttp.a
	chmod 644 /usr/local/lib/libechttp.a
	mkdir -p /usr/local/include
	cp echttp.h echttp_static.h /usr/local/include
	chown root:root /usr/local/include/echttp.h
	chown root:root /usr/local/include/echttp_static.h
	chmod 644 /usr/local/include/echttp.h /usr/local/include/echttp_static.h

clean:
	rm -f *.o *.a

%.o: %.c
	gcc -c -g -O -fPIC -o $@ $<

libechttp.a: $(OBJS)
	ar ru $@ $^
	ranlib $@

