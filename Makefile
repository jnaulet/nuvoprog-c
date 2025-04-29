app := nuvoprog-c

obj += nuvoprog-c.o
obj += probe.o
# deps
obj += intelhex/ihex.o

CFLAGS  := -Wall -std=c99 -I. -Iintelhex
LDFLAGS := -lusb-1.0

PREFIX := /usr/local

all: $(app)

$(app): $(obj)
	$(CC) $(CFLAGS) $(obj) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

intelhex/ihex.c:
	git submodule init
	git submodule update

debug: CFLAGS += -g3
debug: all

indent:
	indent -kr -as *.c *.h

install:
	install -D -m0755 -t $(PREFIX)/bin $(app)

clean:
	@rm -f $(obj) $(app) *~

.phony: debug indent install clean
