PREFIX ?= /usr/local

ccsrc = $(wildcard src/*.cc)
obj = $(ccsrc:.cc=.o)
dep = $(ccsrc:.cc=.d)
bin = xlivebg-gui

CXXFLAGS = -pedantic -Wall -g -MMD
LDFLAGS = -lfltk

$(bin): $(obj)
	$(CXX) -o $@ $(obj) $(LDFLAGS)

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
