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

#ifndef STUD_H
#define STUD_H

#include <syslog.h>

/* Command line Options */
typedef enum {
    ENC_TLS,
    ENC_SSL
} ENC_TYPE;

typedef struct stud_options {
    ENC_TYPE ETYPE;
    int WRITE_IP_OCTET;
    int WRITE_PROXY_LINE;
    const char* CHROOT;
    uid_t UID;
    gid_t GID;
    const char *FRONT_IP;
    const char *FRONT_PORT;
    const char *BACK_IP;
    const char *BACK_PORT;
    long NCORES;
    const char *CERT_FILE;
    const char *CIPHER_SUITE;
    int BACKLOG;
#ifdef USE_SHARED_CACHE
    int SHARED_CACHE;
#ifdef USE_MEMCACHED
    const char *MEMCACHED;
#endif /* USE_MEMCACHED */
#endif /* USE_SHARED_CACHE */
    int QUIET;
    int SYSLOG;
} stud_options;

extern stud_options OPTIONS;

#define LOG(...)                                        \
    do {                                                \
      if (!OPTIONS.QUIET) fprintf(stdout, __VA_ARGS__); \
      if (OPTIONS.SYSLOG) syslog(LOG_INFO, __VA_ARGS__);                    \
    } while(0)

#define ERR(...)                    \
    do {                            \
      fprintf(stderr, __VA_ARGS__); \
      if (OPTIONS.SYSLOG) syslog(LOG_ERR, __VA_ARGS__); \
    } while(0)

#endif
