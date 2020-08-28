WAYLAND_FLAGS = $(shell pkg-config wayland-client --cflags --libs)
WAYLAND_PROTOCOLS_DIR = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
WAYLAND_SCANNER = $(shell pkg-config --variable=wayland_scanner wayland-scanner)
XDG_SHELL_PROTOCOL = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml
CFLAGS ?= -Wall -Wextra -Wno-unused-parameter -fPIC -flto

all: xdg-shell.h libwlengine.so check

libwlengine.so: wlengine.o xdg-shell.o
	$(CC) $(CFLAGS) -shared -o libwlengine.so wlengine.o xdg-shell.o $(WAYLAND_FLAGS) -lrt

wlengine.o: wlengine.c
	$(CC) $(CFLAGS) -c wlengine.c

xdg-shell.o: xdg-shell.c
	$(CC) $(CFLAGS) -c xdg-shell.c

xdg-shell.h: $(XDG_SHELL_PROTOCOL)
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL_PROTOCOL) xdg-shell.h

xdg-shell.c: $(XDG_SHELL_PROTOCOL)
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL_PROTOCOL) xdg-shell.c

check: test
	./test

test: test.c libwlengine.so
	$(CC) $(CFLAGS) -o test test.c -L. -lwlengine

clean:
	$(RM) -f test libwlengine.so *.o xdg-shell.*