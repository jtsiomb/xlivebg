src = $(wildcard src/*.c)
obj = $(src:.c=.o)
lib = libimago.a

CFLAGS = -Wall -g -O3

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
