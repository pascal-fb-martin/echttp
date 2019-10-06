
OBJS= echttp_static.o echttp.o echttp_raw.o echttp_catalog.o

all: httpserver

clean:
	rm -f *.o httpserver

%.o: %.c
	gcc -c -g -O -o $@ $<

httpserver: httpserver.o $(OBJS)
	gcc -g -O -o httpserver httpserver.o $(OBJS)

