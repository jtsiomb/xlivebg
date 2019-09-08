src = $(wildcard *.c)
obj = $(src:.c=.o)
so = $(name).so

proj_root = ../..

CFLAGS = -pedantic -Wall -g -I$(proj_root)/include

$(so): $(obj)
	$(CC) -o $@ -shared $(obj)

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
