test: sorted libc

httpserver: httpserver.c ../libechttp.a
	cc -O -g -o httpserver httpserver.c ../libechttp.a -lssl -lcrypto -lmagic

sorted:
	gcc -g -I.. -O2 -o sortedtest sortedtest.c ../echttp_sorted.c
	./sortedtest
	rm sortedtest

libc:
	gcc -g -I.. -O2 -o libctest libctest.c ../echttp_libc.c
	./libctest
	rm libctest
