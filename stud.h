/**
  * Copyright 2011 Bump Technologies, Inc. All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without modification, are
  * permitted provided that the following conditions are met:
  *
  *    1. Redistributions of source code must retain the above copyright notice, this list of
  *       conditions and the following disclaimer.
  *
  *    2. Redistributions in binary form must reproduce the above copyright notice, this list
  *       of conditions and the following disclaimer in the documentation and/or other materials
  *       provided with the distribution.
  *
  * THIS SOFTWARE IS PROVIDED BY BUMP TECHNOLOGIES, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED
  * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
  * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BUMP TECHNOLOGIES, INC. OR
  * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  * The views and conclusions contained in the software and documentation are those of the
  * authors and should not be interpreted as representing official policies, either expressed
  * or implied, of Bump Technologies, Inc.
  *
  **/

#include <sys/socket.h>
#include <openssl/ssl.h>
#include <ev.h>

#include "ringbuffer.h"

/* What agent/state requests the shutdown--for proper half-closed
 * handling */
typedef enum _SHUTDOWN_REQUESTOR {
    SHUTDOWN_HARD,
    SHUTDOWN_DOWN,
    SHUTDOWN_UP
} SHUTDOWN_REQUESTOR;

/*
 * Proxied State
 *
 * All state associated with one proxied connection
 */
typedef struct proxystate {
    ringbuffer ring_down; /* pushing bytes from client to backend */
    ringbuffer ring_up;   /* pushing bytes from backend to client */

    ev_io ev_r_up;        /* Upstream write event */
    ev_io ev_w_up;        /* Upstream read event */

    ev_io ev_r_handshake; /* Downstream write event */
    ev_io ev_w_handshake; /* Downstream read event */

    ev_io ev_r_down;      /* Downstream write event */
    ev_io ev_w_down;      /* Downstream read event */

    int fd_up;            /* Upstream (client) socket */
    int fd_down;          /* Downstream (backend) socket */

    int want_shutdown:1;  /* Connection is half-shutdown */
    int handshaked:1;     /* Initial handshake happened */
    int renegotiation:1;  /* Renegotation is occuring */

    SSL *ssl;             /* OpenSSL SSL state */

    struct sockaddr_storage remote_ip;  /* Remote ip returned from `accept` */
} proxystate;

#define NULL_DEV "/dev/null"

char * stud_instance_name (char *name);
