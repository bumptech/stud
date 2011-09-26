/*
 * shctx.c
 *
 * Copyright (C) 2011 EXCELIANCE 
 * 
 * Author: Emeric Brun - emeric@exceliance.fr
 *
 */
#include <sys/mman.h>
#ifdef USE_SYSCALL_FUTEX
#include <unistd.h>
#include <linux/futex.h>  
#include <sys/syscall.h>
#else /* USE_SYSCALL_FUTEX */
#include <pthread.h>
#endif /* USE_SYSCALL_FUTEX */

#ifdef USE_MEMCACHED
#include <libmemcached/memcached.h>
#define MEMCACHE_NAME "stud"
#define MEMCACHE_NAME_LEN 4
#endif

#include "stud.h"
#include "ebtree/ebmbtree.h"
#include "shctx.h"

#ifndef SHSESS_MAX_DATA_LEN
#define SHSESS_MAX_DATA_LEN 512
#endif

struct shared_session {
        struct ebmb_node key;
        unsigned char key_data[SSL_MAX_SSL_SESSION_ID_LENGTH];
        int data_len;
        unsigned char data[SHSESS_MAX_DATA_LEN];
        struct shared_session *p;
        struct shared_session *n;
};


struct shared_context {
#ifdef USE_SYSCALL_FUTEX
        unsigned int waiters;
#else /* USE_SYSCALL_FUTEX */
        pthread_mutex_t mutex;
#endif
        struct shared_session head;
#ifdef USE_MEMCACHED
	memcached_st memcached;
#endif /* USE_MEMCACHED */
};

/* Static shared context */
static struct shared_context *shctx = NULL;


/* Lock functions */
#ifdef USE_SYSCALL_FUTEX
static inline unsigned int xchg(unsigned int *ptr, unsigned int x)
{
	__asm volatile("lock xchgl %0,%1"
		     : "=r" (x), "+m" (*ptr)
		     : "0" (x)
		     : "memory");
	return x;
}

static inline unsigned int cmpxchg(unsigned int *ptr, unsigned int old, unsigned int new)
{
	unsigned int ret;

	__asm volatile("lock cmpxchgl %2,%1"
		     : "=a" (ret), "+m" (*ptr)
		     : "r" (new), "0" (old)
		     : "memory");
	return ret;
}

static inline unsigned char atomic_inc(unsigned int *ptr)
{
	unsigned char ret;
	__asm volatile("lock incl %0\n"
		     "setne %1\n"
		     : "+m" (*ptr), "=qm" (ret)
		     :
		     : "memory");
	return ret;
}

static inline unsigned char atomic_dec(unsigned int *ptr)
{
	unsigned char ret;
	__asm volatile("lock decl %0\n"
		     "setne %1\n"
		     : "+m" (*ptr), "=qm" (ret)
		     :
		     : "memory");
	return ret;
}

static inline void shared_context_lock(void)
{
	unsigned int x;

	x = cmpxchg(&shctx->waiters, 0, 1);
	if (x) {
		if (x != 2)
			x = xchg(&shctx->waiters, 2);

		while (x) {
			syscall(SYS_futex, &shctx->waiters, FUTEX_WAIT, 2, NULL, 0, 0);
			x = xchg(&shctx->waiters, 2);
		}
	}
}

static inline void shared_context_unlock(void)
{
	if (atomic_dec(&shctx->waiters)) {
		shctx->waiters = 0;
		syscall(SYS_futex, &shctx->waiters, FUTEX_WAKE, 1, NULL, 0, 0);
	}
}

#else /* USE_SYSCALL_FUTEX */

#define shared_context_lock(v) pthread_mutex_lock(&shctx->mutex)
#define shared_context_unlock(v) pthread_mutex_unlock(&shctx->mutex)

#endif

/* List Macros */

#define shared_session_remove(a)	(a)->n->p = (a)->p; \
					(a)->p->n = (a)->n;

#define shared_session_movelast(a)	shared_session_remove(a) \
					(a)->n = &shctx->head; \
					(a)->p = shctx->head.p; \
					shctx->head.p->n = a; \
					shctx->head.p = a;

#define shared_session_movefirst(a)	shared_session_remove(a) \
					(a)->p = &shctx->head; \
					(a)->n = shctx->head.n; \
					shctx->head.n->p = a; \
					shctx->head.n = a;




/* SSL context callbacks */

#ifdef USE_MEMCACHED
char *build_memkey(unsigned char *session_id, unsigned int session_id_length,
		   unsigned int *keylen) {
	static char memkey[MEMCACHE_NAME_LEN + 1
			   + 2*SSL_MAX_SSL_SESSION_ID_LENGTH + 1];
	*keylen = MEMCACHE_NAME_LEN      /* Prefix */
		+ 1			 /* : */
		+ session_id_length * 2; /* Hexadecimal encoding */
	memcpy(memkey, MEMCACHE_NAME, MEMCACHE_NAME_LEN);
	memkey[MEMCACHE_NAME_LEN] = ':';
	for (unsigned int i = 0; i < session_id_length; i++)
		snprintf(memkey + MEMCACHE_NAME_LEN + 1 + i * 2, 3,
			 "%02X", session_id[i]);
	return memkey;
}
#endif

int shctx_new_cb(SSL *ssl, SSL_SESSION *sess) {
	(void)ssl;
	struct shared_session *retshs;
	struct shared_session *lastshs;
	unsigned char *val_tmp;
	int val_len;

	if (sess->session_id_length > SSL_MAX_SSL_SESSION_ID_LENGTH)
		return 1;

	val_len=i2d_SSL_SESSION(sess, NULL);
	if(val_len > SHSESS_MAX_DATA_LEN)
		return 1;

	shared_context_lock();
    
	lastshs = shctx->head.p;
	ebmb_delete(&lastshs->key);

	memset(lastshs->key_data, 0, SSL_MAX_SSL_SESSION_ID_LENGTH);
	memcpy(lastshs->key_data, sess->session_id, sess->session_id_length);


	retshs = (struct shared_session *)ebmb_insert(&shctx->head.key.node.branches, &lastshs->key, SSL_MAX_SSL_SESSION_ID_LENGTH);

	retshs->data_len = val_len;
	val_tmp=retshs->data;
	i2d_SSL_SESSION(sess, &val_tmp);
    
	shared_session_movefirst(retshs);

	shared_context_unlock();

#ifdef USE_MEMCACHED
	/* Try to add it to memcached */
	if (OPTIONS.MEMCACHED) {
		memcached_return_t rc;
		unsigned int keylen;
		char *memkey = build_memkey(sess->session_id, sess->session_id_length, &keylen);
		rc = memcached_set(&shctx->memcached, memkey, keylen,
				   (const char *)retshs->data, val_len,
				   time(NULL) + SSL_SESSION_get_timeout(sess), 0);
		if (rc != MEMCACHED_SUCCESS)
			ERR("{cache} memcached_set() failed: %s\n",
			    memcached_strerror(&shctx->memcached, rc));
	}
#endif

	return 1; /* leave the session in local cache for reuse */
}

SSL_SESSION *shctx_get_cb(SSL *ssl, unsigned char *key, int key_len, int *do_copy) {
	(void)ssl;
	struct shared_session *retshs;
	unsigned char *val_tmp=NULL;
	SSL_SESSION *sess=NULL;

	*do_copy = 0; /* allow the session to be freed autmatically */

	if (key_len == SSL_MAX_SSL_SESSION_ID_LENGTH) {

		shared_context_lock();
  		retshs = (struct shared_session *)ebmb_lookup(&shctx->head.key.node.branches, key, key_len);
	
	}
	else if (key_len < SSL_MAX_SSL_SESSION_ID_LENGTH) {

		unsigned char tmp_key[SSL_MAX_SSL_SESSION_ID_LENGTH];

		memset(tmp_key, 0, SSL_MAX_SSL_SESSION_ID_LENGTH);
		memcpy(tmp_key, key, key_len);
		
		shared_context_lock();
		retshs = (struct shared_session *)ebmb_lookup(&shctx->head.key.node.branches, tmp_key, SSL_MAX_SSL_SESSION_ID_LENGTH);
	}
	else
		return NULL;

	if(retshs) {
		val_tmp=retshs->data;
		sess=d2i_SSL_SESSION(NULL, (const unsigned char **)&val_tmp, retshs->data_len);

		shared_session_movefirst(retshs);
	}

	shared_context_unlock();

#ifdef USE_MEMCACHED
	if (!sess && OPTIONS.MEMCACHED) {
		/* No session, try to lookup in memcached */
		memcached_return_t rc;
		unsigned int mkeylen;
		size_t val_len;
		uint32_t flags;
		char *memkey = build_memkey(key, key_len, &mkeylen);
		/* TODO: sync call to memcached_get(), performance problems */
		val_tmp = (unsigned char *)memcached_get(&shctx->memcached, memkey, mkeylen,
							 &val_len, &flags, &rc);
		if (rc != MEMCACHED_SUCCESS)
			ERR("{cache} memcached_get() failed: %s\n",
			    memcached_strerror(&shctx->memcached, rc));
		if (val_tmp) {
			unsigned char *tmp = val_tmp;
			if (!(sess = d2i_SSL_SESSION(NULL,
						     (const unsigned char **)&tmp,
						     val_len)))
				ERR("{cache} unable to decode SSL session\n");
			free(val_tmp);
		}
	}
#endif

	return sess;
}

void shctx_remove_cb(SSL_CTX *ctx, SSL_SESSION *sess)
{
	(void)ctx;
	struct shared_session *retshs;

	if (sess->session_id_length == SSL_MAX_SSL_SESSION_ID_LENGTH) {

		shared_context_lock();
		retshs = (struct shared_session *)ebmb_lookup(&shctx->head.key.node.branches, sess->session_id, sess->session_id_length);
	}
	else if (sess->session_id_length < SSL_MAX_SSL_SESSION_ID_LENGTH) {

		unsigned char key[SSL_MAX_SSL_SESSION_ID_LENGTH];

		memset(key, 0, SSL_MAX_SSL_SESSION_ID_LENGTH);
		memcpy(key, sess->session_id, sess->session_id_length);

		shared_context_lock();
		retshs = (struct shared_session *)ebmb_lookup(&shctx->head.key.node.branches, key, SSL_MAX_SSL_SESSION_ID_LENGTH);
	}
	else
		return;

	if(retshs) {
		ebmb_delete(&retshs->key);
		shared_session_movelast(retshs);
	}

	shared_context_unlock();

#ifdef USE_MEMCACHED
	/* Also delete from memcached */
	if (OPTIONS.MEMCACHED) {
		unsigned int keylen;
		char *memkey = build_memkey(sess->session_id, sess->session_id_length, &keylen);
		memcached_delete(&shctx->memcached, memkey, keylen, 0);
	}
#endif

	return;
}

/* Init shared memory context if not allocated and set SSL context callbacks
 * size is the max number of stored session. Also initialize optional memcached.
 * Returns: -1 on failure, 1 if performs initialization, and 0 if just perform
 * callbacks registration */
int shared_context_init(SSL_CTX *ctx, int size)
{
	int ret = 0;

	if (!shctx) {
		/* Shared memory */
		int i;

#ifndef USE_SYSCALL_FUTEX
		pthread_mutexattr_t attr;
#endif
		struct shared_session *prev,*cur;

		shctx = (struct shared_context *)mmap(NULL, sizeof(struct shared_context)+(size*sizeof(struct shared_session)),
								PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (!shctx) {
			ERR("{cache} unable to alloc memory for shared cache.\n");
			return -1;
		}

#ifdef USE_SYSCALL_FUTEX
		shctx->waiters = 0;
#else /* USE_SYSCALL_FUTEX */
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&shctx->mutex, &attr);
#endif
		memset(&shctx->head.key, 0, sizeof(struct ebmb_node));
		/* No duplicate authorized: */
		shctx->head.key.node.branches.b[1] = (void *)1;

		cur = &shctx->head;
		for ( i = 0 ; i < size ; i++) {
			prev = cur;
			cur = (struct shared_session *)((char *)prev + sizeof(struct shared_session));
			prev->n = cur;
			cur->p = prev;
		}
		cur->n = &shctx->head;
		shctx->head.p = cur;

		ret = 1;

		/* Memcached */
#ifdef USE_MEMCACHED
		if (OPTIONS.MEMCACHED) {
			memcached_server_list_st servers;
			memcached_return_t rc;
			memcached_create(&shctx->memcached);
			memcached_behavior_set(&shctx->memcached,
					       MEMCACHED_BEHAVIOR_BINARY_PROTOCOL, 1);
			memcached_behavior_set(&shctx->memcached,
					       MEMCACHED_BEHAVIOR_NO_BLOCK, 1);
			servers = memcached_servers_parse(OPTIONS.MEMCACHED);
			if (!servers) {
				ERR("{cache} unable to parse list of memcached servers.\n");
				return -1;
			}
			if ((rc = memcached_server_push(&shctx->memcached,
							servers)) != MEMCACHED_SUCCESS) {
				ERR("{cache} unable to add memcached servers to pool: %s\n",
				    memcached_strerror(&shctx->memcached, rc));
				return -1;
			}
			memcached_server_free(servers);

			/* Also disable tickets */
			SSL_CTX_set_options(ctx, SSL_OP_NO_TICKET);
		}
#endif

	}

	/* Set callbacks */
	SSL_CTX_sess_set_new_cb(ctx, shctx_new_cb);
	SSL_CTX_sess_set_get_cb(ctx, shctx_get_cb);
	SSL_CTX_sess_set_remove_cb(ctx, shctx_remove_cb);

        return ret;
}

