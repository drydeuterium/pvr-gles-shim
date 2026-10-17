# pvr-gles-shim

PREFIX  ?= /usr/local
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -fPIC
LDFLAGS ?= -shared

LIB := pvr-gles-shim.so
SRC := pvr-gles-shim.c

.PHONY: all install uninstall clean

all: $(LIB)

$(LIB): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

install: $(LIB)
	install -Dm755 $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB)
	install -Dm644 $(SRC) $(DESTDIR)$(PREFIX)/src/pvr-gles-shim/$(SRC)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/$(LIB)
	rm -rf $(DESTDIR)$(PREFIX)/src/pvr-gles-shim

clean:
	rm -f $(LIB)
