src = $(wildcard src/*.c)
obj = $(src:.c=.o)
lib = libtreestore.a

CFLAGS = -pedantic -Wall -g -O3

$(lib): $(obj)
	$(AR) rcs $@ $<

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
