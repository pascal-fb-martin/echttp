# Copyright 2025, Pascal Martin
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#
# This make file create the echttp library, plus a handful of tool, mostly
# for testing purposes.
#
# The echttp library can be built as a shared object (it works), but sharing
# creates a deployement codependency. This could be managed through versioning,
# but that requires more work than it is worth.
#
# The shared object building rules were kept here, nonetheless, as they
# might become useful in different deployement contexts (containers?).

prefix=/usr/local

INSTALL=/usr/bin/install
LDCONFIG=/usr/sbin/ldconfig

PACKAGE=build/echttp

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
	rm -f $(DESTDIR)$(prefix)/lib/libechttp.so
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0644 $(PUBLIC_INCLUDE) $(DESTDIR)$(prefix)/include
	$(INSTALL) -m 0755 -d $(DESTDIR)$(prefix)/bin
	$(INSTALL) -m 0755 -s echttp_print echttp_get $(DESTDIR)$(prefix)/bin
	if [ "x$(DESTDIR)" = "x" ] ; then $(LDCONFIG) ; fi

install: dev

clean:
	rm -f *.o *.a *.so
	rm -f echttp_print echttp_get

rebuild: clean all

uninstall:
	rm -f $(DESTDIR)$(prefix)/lib/libechttp.a
	rm -f $(DESTDIR)$(prefix)/include/echttp*.h
	rm -f $(DESTDIR)$(prefix)/bin/echttp_print
	rm -f $(DESTDIR)$(prefix)/bin/echttp_get

purge: uninstall

# Build a private Debian package.
debian-package:
	rm -rf build
	mkdir -p $(PACKAGE)/DEBIAN
	sed "s/{arch}/`dpkg --print-architecture`/" < debian/control > $(PACKAGE)/DEBIAN/control
	cp debian/copyright $(PACKAGE)/DEBIAN
	cp debian/changelog $(PACKAGE)/DEBIAN
	make DESTDIR=$(PACKAGE) install
	cd build ; fakeroot dpkg-deb -b echttp .

%.o: %.c
	gcc -c -Wall -g -Os -fPIC -o $@ $<

libechttp.so: $(OBJS)
	gcc -shared -o $@ $^

libechttp.a: $(OBJS)
	ar r $@ $^
	ranlib $@

echttp_print: echttp_print.o libechttp.a
	gcc -Os -fPIC -o $@ echttp_print.o libechttp.a

echttp_get: echttp_get.o libechttp.a
	gcc -Os -fPIC -o $@ echttp_get.o libechttp.a

