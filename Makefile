# [g]make USE_xxxx=1
#
# USE_SHARED_CACHE   :   enable/disable a shared session cache (disabled by default)

DESTDIR =
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man

UPSTARTDIR = /etc/init
CONFDIR = /etc/stud
CONFFILE = stud.cfg

CFLAGS  = -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -D_GNU_SOURCE -I/usr/local/include
LDFLAGS = -lssl -lcrypto -lev -L/usr/local/lib
OBJS    = stud.o ringbuffer.o configuration.o

all: realall

# Shared cache feature
ifneq ($(USE_SHARED_CACHE),)
CFLAGS += -DUSE_SHARED_CACHE -DUSE_SYSCALL_FUTEX
OBJS   += shctx.o ebtree/libebtree.a
ALL    += ebtree

ebtree/libebtree.a: $(wildcard ebtree/*.c)
	make -C ebtree
ebtree:
	@[ -d ebtree ] || ( \
		echo "*** Download libebtree at http://1wt.eu/tools/ebtree/" ; \
		echo "*** Untar it and make a link named 'ebtree' to point on it"; \
		exit 1 )
endif

# No config file support?
ifneq ($(NO_CONFIG_FILE),)
CFLAGS += -DNO_CONFIG_FILE
endif

ALL += stud
realall: $(ALL)

stud: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

install: $(ALL)
	install -d $(DESTDIR)$(BINDIR)
	install stud $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(MANDIR)/man8
	install -m 644 stud.8 $(DESTDIR)$(MANDIR)/man8

	install -d $(UPSTARTDIR)
	install upstart/stud.conf $(UPSTARTDIR)

	install -d $(CONFDIR)
	stud --default-config > $(CONFDIR)/$(CONFFILE)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/stud
	rm -f $(DESTDIR)$(MANDIR)/man8/stud.8
	rmdir $(DESTDIR)$(MANDIR)/man8 --ignore-fail-on-non-empty

	rm -f $(UPSTARTDIR)/stud.conf

	rm -f $(CONFDIR)/$(CONFFILE)
	rmdir $(CONFDIR) --ignore-fail-on-non-empty

clean:
	rm -f stud $(OBJS)


.PHONY: all realall
