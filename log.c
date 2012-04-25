/**
 * log.c
 *
 * Author: Brane F. Gracnar
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <ev.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <errno.h>
#include <syslog.h>


#include "ringbuffer.h"
#include "stud.h"
#include "configuration.h"
#include "log.h"


#define TIMESTR_BUF_LEN         40
#define TIMESTR_FMT             "%Y/%m/%d %H:%M:%S.%%d"
#define SOCKSTR_BUF_LEN         48
#define PSSTR_BUF_LEN           100
#define LOG_BUF_LEN             1024

#define DOLOG(l) (l <= CONFIG->LOG_LEVEL) ? 1 : 0


char buf_timestr[TIMESTR_BUF_LEN];
char sockstr_buf[SOCKSTR_BUF_LEN];
char psstr_buf[PSSTR_BUF_LEN];

static int logging_initialized = 0;

extern stud_config *CONFIG;
extern int child_num;

char * ts_as_str (char *dst) {
  if (dst == NULL) dst = buf_timestr;

  char fmtbuf[TIMESTR_BUF_LEN];
  memset(fmtbuf, '\0', sizeof(fmtbuf));
  memset(dst, '\0', TIMESTR_BUF_LEN);

  // get cached loop time...
  ev_tstamp t_ev = ev_now(EV_DEFAULT);

  time_t t = (time_t) t_ev;
  struct tm *lt = localtime(&t);
  strftime(fmtbuf, sizeof(fmtbuf), TIMESTR_FMT, lt);

  // add microtime
  int frac = (t_ev - (long) t_ev) * 1000000;
  snprintf(dst, TIMESTR_BUF_LEN, fmtbuf, frac);

  return dst;
}

char * sock_as_str (int sock, char *dst) {
  if (dst == NULL) dst = sockstr_buf;
  memset(dst, '\0', SOCKSTR_BUF_LEN);

  struct sockaddr_storage addr;
  char ipstr[INET6_ADDRSTRLEN];
  int port = 0;

  socklen_t len = sizeof(addr);
  getpeername(sock, (struct sockaddr*)&addr, &len);
  
  // IPv4 socket?
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    port = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
  }
  // IPv6 socket?
  else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
    port = ntohs(s->sin6_port);
    inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
  }
  // UNIX domain socket?
  else if (addr.ss_family == AF_UNIX) {
    
  }

  snprintf(dst, SOCKSTR_BUF_LEN, "[%s]:%d", ipstr, port);
  return dst;
}

char * ps_as_str (proxystate *ps, char *dst) {
  char up_buf[SOCKSTR_BUF_LEN];
  char down_buf[SOCKSTR_BUF_LEN];

  if (dst == NULL) dst = psstr_buf;
  memset(dst, '\0', PSSTR_BUF_LEN);
    
  sock_as_str(ps->fd_up, up_buf);
  sock_as_str(ps->fd_down, down_buf);
  
  snprintf(dst, PSSTR_BUF_LEN, "%s => %s", up_buf, down_buf);
  
  return dst;
}

char buf_pcsig[10];
char * proc_signature (char *dst) {
  if (child_num < 0 || child_num >= CONFIG->NCORES)
    return "master";
  else {
    if (dst == NULL) dst = buf_pcsig;
    
    // already cached stuff?
    if (dst == buf_pcsig && buf_pcsig[0] != '\0') return dst;
    
    memset(dst, '\0', sizeof(buf_pcsig));
    snprintf(dst, sizeof(buf_pcsig), "worker #%d", child_num);
  }
  
  return dst;
}

char * log_level2str (int level) {
  switch (level) {
    case LOG_EMERG:
      return LSTR_FATAL;
      break;
    case LOG_ERR:
      return LSTR_ERR;
      break;
    case LOG_WARNING:
      return LSTR_WARN;
      break;
    case LOG_NOTICE:
      return LSTR_NOTICE;
      break;
    case LOG_INFO:
      return LSTR_INFO;
      break;
    case LOG_DEBUG:
      return LSTR_DEBUG;
      break;
    default:
      return LSTR_UNKNOWN;
  }
}

int log_str2level (char *str) {
  int r = LOG_NOTICE;
  if (str == NULL || strlen(str) < 1) return r;
  
  if (strcasecmp(str, LSTR_FATAL) == 0)
    r = LOG_EMERG;
  else if (strcasecmp(str, LSTR_ERR) == 0)
    r = LOG_ERR;
  else if (strcasecmp(str, LSTR_WARN) == 0 || strcasecmp(str, "warn") == 0)
    r = LOG_WARNING;
  else if (strcasecmp(str, LSTR_NOTICE) == 0)
    r = LOG_NOTICE;
  else if (strcasecmp(str, LSTR_INFO) == 0)
    r = LOG_INFO;
  else if (strcasecmp(str, LSTR_DEBUG) == 0)
    r = LOG_DEBUG;

  return r;
}

char buf_syslog_ident[128];
char * log_syslog_ident (void) {
  char *n = stud_instance_name(NULL);
  if (n == NULL || strlen(n) < 1)
    return "stud";
  
  snprintf(buf_syslog_ident, sizeof(buf_syslog_ident), "stud/%s", n);
  return buf_syslog_ident;
}

void log_init (void) {
  if (logging_initialized) return;
  if (CONFIG->SYSLOG) {
    openlog(log_syslog_ident(), LOG_CONS | LOG_PID | LOG_NDELAY, CONFIG->SYSLOG_FACILITY);
  }
}

void log_msg (int level, char *fmt, ...) {
  va_list args;
  char buf[LOG_BUF_LEN];
  
  //printf("LCFG LLEVEL: %d, LLEVEL: %d\n", CONFIG->LOG_LEVEL, level);
  if (! DOLOG(level)) return;
  //printf("DAEMONIZE: %d\n", CONFIG->DAEMONIZE);
  //printf("SYSLOG: %d\n", CONFIG->SYSLOG);
  if (CONFIG->DAEMONIZE && ! CONFIG->SYSLOG) return;

  // format log message
  memset(buf, '\0', LOG_BUF_LEN);
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (! CONFIG->DAEMONIZE) {
    fprintf(stderr, "[%s] %-10s %-7s %s\n", ts_as_str(NULL), proc_signature(NULL), log_level2str(level), buf);
  }
  
  if (CONFIG->SYSLOG) {
    syslog(level, "%s %s", proc_signature(NULL), buf);
  }
}


void log_msg_ps (int level, proxystate *ps, char *fmt, ...) {
  va_list args;
  char buf[LOG_BUF_LEN];
  
  if (! DOLOG(level)) return;

  // format log message
  memset(buf, '\0', LOG_BUF_LEN);
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  
  log_msg(level, "%s %s", ps_as_str(ps, NULL), buf);
}

void log_msg_sock (int level, int sock, char *fmt, ...) {
  va_list args;
  char buf[LOG_BUF_LEN];
  
  if (! DOLOG(level)) return;

  // format log message
  memset(buf, '\0', LOG_BUF_LEN);
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  
  log_msg(level, "%s %s", sock_as_str(sock, NULL), buf);
}

void log_msg_lps (int level, int listener, proxystate *ps, char *fmt, ...) {
  va_list args;
  char buf[LOG_BUF_LEN];
  
  if (! DOLOG(level)) return;

  // format log message
  memset(buf, '\0', LOG_BUF_LEN);
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  
  log_msg(
    level, "[%s %s] %s", sock_as_str(listener, NULL),
    ps_as_str(ps, NULL), buf
  );  
}

void fail (char *fmt, ...) {
  va_list args;
  char buf[LOG_BUF_LEN];

  // format log message
  memset(buf, '\0', LOG_BUF_LEN);
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  
  die("%s %s", buf, strerror(errno));
}
