src = $(wildcard *.c)
ssrc = $(wildcard *.s)
obj = $(src:.c=.o) $(ssrc:.s=.o)
so = $(name).so

proj_root = ../..

CFLAGS = -pedantic -Wall -g -O3 -ffast-math -fPIC -I$(proj_root)/include \
		 $(CFLAGS_cfg) $(plugin_cflags)
LDFLAGS = $(LDFLAGS_cfg) $(plugin_ldflags)

$(so): $(obj)
	$(CC) -o $@ -shared $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(so)

.PHONY: install
install: $(so)
	mkdir -p $(DESTDIR)$(PREFIX)/lib/xlivebg
	cp $(so) $(DESTDIR)$(PREFIX)/lib/xlivebg/$(so)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/xlivebg/$(so)
