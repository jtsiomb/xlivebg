# change this to install elsewhere
PREFIX = /usr/local
name = xlivebg

src = $(wildcard src/*.c)
obj = $(src:.c=.o)
bin = $(name)

CFLAGS = -std=c89 -pedantic -Wall -g -DPREFIX=\"$(PREFIX)\" -DAPP_NAME=\"$(name)\"
LDFLAGS = -lX11 -lGL -lGLU

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: install
install: $(bin)
	mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/share/$(name)
	cp $(bin) $(DESTDIR)$(PREFIX)/bin/$(bin)
	cp data/* $(DESTDIR)$(PREFIX)/share/$(name)/

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(bin)
	rm -f $(DESTDIR)$(PREFIX)/share/$(name)/*
	rmdir $(DESTDIR)$(PREFIX)/share/$(name) || true
