PREFIX ?= /usr

WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)
XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml
CFLAGS ?= -Wall -Wextra -Wno-unused-parameter -fPIC -flto -O2

all: xdg-shell.h libwwl.so check

libwwl.so: wwl.o xdg-shell.o
	$(CC) $(CFLAGS) -shared -o libwwl.so wwl.o xdg-shell.o $(WAYLAND_FLAGS) -lrt

wwl.o: wwl.c
	$(CC) $(CFLAGS) -c wwl.c

xdg-shell.o: xdg-shell.c
	$(CC) $(CFLAGS) -c xdg-shell.c

xdg-shell.h: $(XDG_SHELL_PROTOCOL)
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) xdg-shell.h

xdg-shell.c: $(XDG_SHELL_PROTOCOL)
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) xdg-shell.c

check: test
	./test

test: test.c libwwl.so
	$(CC) $(CFLAGS) -o test test.c -L. -lwwl

install:
	mkdir -p $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp wwl.h $(DESTDIR)$(PREFIX)/include
	cp libwwl.so $(DESTDIR)$(PREFIX)/lib
	ln -s $(DESTDIR)$(PREFIX)/lib/libwwl.so.0 $(DESTDIR)$(PREFIX)/lib/libwwl.so

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/include/wwl.h
	rm -f $(DESTDIR)$(PREFIX)/lib/libwwl.so.0
	rm -f $(DESTDIR)$(PREFIX)/lib/libwwl.so
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/include
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/lib

clean:
	$(RM) -f test libwwl.so *.o xdg-shell.*