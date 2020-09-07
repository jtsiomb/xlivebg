PREFIX ?= /usr/local

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = xlivebg-gui

warn = -pedantic -Wall
def = -DMINIGLUT_USE_LIBC
inc = ../libs/treestore/src

CFLAGS = $(warn) -g -MMD $(def) $(inc)
LDFLAGS = ../libs/treestore/libtreestore.a -lXm -lXt -lX11 -lGL -lm

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
	rm -f $(DESTDIR)$(PREFIX)/bin/$(bin)