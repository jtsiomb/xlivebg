PREFIX ?= /usr/local

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = xlivebg-gui

warn = -Wall
def = -DMINIGLUT_USE_LIBC
inc = -I/usr/Motif-2.1/include -I/usr/local/include -I/usr/X11R6/include \
	-I../libs/treestore/src
libdir = -L/usr/Motif-2.1/lib32 -L/usr/local/lib -L/usr/X11R6/lib

CFLAGS = $(warn) -g -MMD $(def) $(inc) $(CFLAGS_cfg)
LDFLAGS = $(libdir) $(LDFLAGS_cfg) ../libs/treestore/libtreestore.a \
		  -lXm -lXt -lX11 -lGL -lm

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(bin) $(obj)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: install
install: $(bin)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(bin) $(DESTDIR)$(PREFIX)/bin/$(bin)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(bin)
