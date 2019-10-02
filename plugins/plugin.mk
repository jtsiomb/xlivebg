src = $(wildcard *.c)
obj = $(src:.c=.o)
so = $(name).so

proj_root = ../..

CFLAGS = -pedantic -Wall -g -fPIC -I$(proj_root)/include $(plugin_cflags)
LDFLAGS = $(plugin_ldflags)

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
