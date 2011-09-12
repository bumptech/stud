all: stud

LIBEV=4.04
libev/.libs/libev.a: libev
	( cd $^ ; ./configure --enable-shared=no ; make )

libev:
	wget http://dist.schmorp.de/libev/libev-$(LIBEV).tar.gz -O - | tar -xzpf - && mv libev-$(LIBEV) libev && touch -c $@

stud: libev/.libs/libev.a stud.c
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -Ilibev -I/usr/local/include -L/usr/local/lib -Llibev/.libs -I. -o stud ringbuffer.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev -lm

# The -shared targets use shared memory between child processes
# for the SSL session cache--potentially a huge performance gain
# for large stud deployments with many children
EBTREE=6.0.6
ebtree/libebtree.a: ebtree
	make -C $^
ebtree:
	wget http://1wt.eu/tools/ebtree/ebtree-$(EBTREE).tar.gz -O - | tar -xzpf - && mv ebtree-$(EBTREE) ebtree && touch -c $@

stud-shared: ebtree/libebtree.a libev/.libs/libev.a stud.c shctx.c
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -Ilibev -I/usr/local/include -L/usr/local/lib -Llibev/.libs -Lebtree -I. -DUSE_SHARED_CACHE -o $@ ringbuffer.c shctx.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev -lm -lpthread -lebtree

stud-shared-futex: ebtree/libebtree.a libev/.libs/libev.a stud.c shctx.c
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -Ilibev -I/usr/local/include -L/usr/local/lib -Llibev/.libs -Lebtree -I. -DUSE_SHARED_CACHE -DUSE_SYSCALL_FUTEX -o $@ ringbuffer.c shctx.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev -lm -lebtree

install: stud
	cp stud /usr/local/bin

clean:
	rm -fr stud stud-* *.o libev ebtree

.PHONY: all clean install
