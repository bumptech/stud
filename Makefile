# You should use it this way:
#    make PREFIX=/usr/bin ...
#
# Variables useful for packagers:
#   DESTDIR is not set by default and is used for installation only. It might
#           be useful to install stud into a sandbox.
#   PREFIX  is set to "/usr/local" by default and is used for installation only.
#   SBINDIR is set to "$(PREFIX)/sbin" by default and is used for installation
#           only

#### Installation options.
DESTDIR =
PREFIX = /usr/local
SBINDIR = $(PREFIX)/sbin

all: stud

stud: stud.c ringbuffer.c ringbuffer.h
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/include/libev -I/usr/local/include -L/usr/local/lib -I. -o stud ringbuffer.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev

# The -shared targets use shared memory between child processes
# for the SSL session cache--potentially a huge performance gain
# for large stud deployments with many children
ebtree/libebtree.a: ebtree/*.c
	make -C ebtree

ebtree: ebtree/libebtree.a
	@echo "Please download libebtree at http://1wt.eu/tools/ebtree/ untar it. and create a symbolik link named 'ebtree' to point on it"

stud-shared: stud.c ringbuffer.c ringbuffer.h shctx.c shctx.h
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/include/libev -I/usr/local/include -L/usr/local/lib -Lebtree -I. -DUSE_SHARED_CACHE -o stud ringbuffer.c shctx.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev -lpthread -lebtree

stud-shared-futex: stud.c ringbuffer.c ringbuffer.h shctx.c shctx.h
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/include/libev -I/usr/local/include -L/usr/local/lib -Lebtree -I. -DUSE_SHARED_CACHE -DUSE_SYSCALL_FUTEX -o stud ringbuffer.c shctx.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev -lebtree

install-bin: stud
	install -d $(DESTDIR)$(SBINDIR)
	install stud $(DESTDIR)$(SBINDIR)

install: install-bin

clean:
	rm -f stud *.o
