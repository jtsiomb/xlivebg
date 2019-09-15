# change this to install elsewhere
PREFIX = /usr/local
export PREFIX
name = xlivebg

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
bin = $(name)

incdir = -Isrc -Iinclude -Ilibs/treestore/src
libdir = -Llibs/treestore

CFLAGS = -std=gnu89 -pedantic -Wall -g -DPREFIX=\"$(PREFIX)\" $(incdir)
LDFLAGS = -rdynamic $(libdir) -lX11 -lGL -ldl -limago -ltreestore

.PHONY: all
all: $(bin) plugins bgimage.jpg

# download test image
bgimage.jpg:
	wget http://nuclear.mutantstargoat.com/sw/xlivebg/tmp/bgimage.jpg

$(bin): $(obj) libs
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: clean-all
clean-all: clean clean-plugins

.PHONY: install
install: $(bin)
	mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/share/$(name)
	cp $(bin) $(DESTDIR)$(PREFIX)/bin/$(bin)
	cp include/xlivebg.h $(DESTDIR)$(PREFIX)/include/xlivebg.h
#	cp data/* $(DESTDIR)$(PREFIX)/share/$(name)/

.PHONY: install-all
install-all: install install-plugins

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(bin)
	rm -f $(DESTDIR)$(PREFIX)/include/xlivebg.h
#	rm -f $(DESTDIR)$(PREFIX)/share/$(name)/*
#	rmdir $(DESTDIR)$(PREFIX)/share/$(name) || true

.PHONY: uninstall-all
uninstall-all: uninstall uninstall-plugins

# ---- plugins rules ----
.PHONY: plugins
plugins:
	$(MAKE) -C plugins

.PHONY: clean-plugins
clean-plugins:
	$(MAKE) -C plugins clean

.PHONY: install-plugins
install-plugins:
	$(MAKE) -C plugins install

.PHONY: uninstall-plugins
uninstall-plugins:
	$(MAKE) -C plugins uninstall


# ---- libs rules ----
.PHONY: libs
libs:
	$(MAKE) -C libs

.PHONY: clean-libs
clean-libs:
	$(MAKE) -C libs
