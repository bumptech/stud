/**
 * log.h
 *
 * Author: Brane F. Gracnar
 *
 */

// "Initializes" logging subsystem
void log_init (void);

// Writes timestamp (including microseconds) to dst.
// If dst == NULL uses it's own buffer. 
char * ts_as_str (char *dst);

// Writes string representation of proxystate structure to dst.
// If dst == NULL uses it's own buffer.
char * ps_as_str (proxystate *ps, char *dst);

// Writes string representation of a socket to dst.
// If dst == NULL uses it's own buffer.
char * sock_as_str (int sock, char *dst);

char * log_level2str (int level);
int log_str2level (char *str);

/**
 * Log level names
 */

#define LSTR_FATAL      "FATAL"
#define LSTR_ERR        "ERROR"
#define LSTR_WARN       "WARNING"
#define LSTR_NOTICE     "NOTICE"
#define LSTR_INFO       "INFO"
#define LSTR_DEBUG      "DEBUG"
#define LSTR_UNKNOWN    "UNKNOWN"

/**
 * General purpose logging functions
 */

void log_msg (int level, char *fmt, ...);

#define log_fatal(...)          log_msg(LOG_EMERG, __VA_ARGS__)
#define log_err(...)            log_msg(LOG_ERR, __VA_ARGS__)
#define log_warn(...)           log_msg(LOG_WARNING, __VA_ARGS__)
#define log_notice(...)         log_msg(LOG_NOTICE, __VA_ARGS__)
#define log_info(...)           log_msg(LOG_INFO, __VA_ARGS__)
#define log_debug(...)          log_msg(LOG_DEBUG, __VA_ARGS__)

/**
 * Proxy state logging "functions"
 */

void log_msg_ps (int level, proxystate *ps, char *fmt, ...);

#define log_fatal_ps(...)        log_msg_ps(LOG_EMERG, __VA_ARGS__)
#define log_err_ps(...)          log_msg_ps(LOG_ERR, __VA_ARGS__)
#define log_warn_ps(...)         log_msg_ps(LOG_WARNING, __VA_ARGS__)
#define log_notice_ps(...)       log_msg_ps(LOG_NOTICE, __VA_ARGS__)
#define log_info_ps(...)         log_msg_ps(LOG_INFO, __VA_ARGS__)
#define log_debug_ps(...)        log_msg_ps(LOG_DEBUG, __VA_ARGS__)

/**
 * Socket logging "functions"
 */

void log_msg_sock (int level, int sock, char *fmt, ...);

#define log_fatal_sock(...)      log_msg_sock(LOG_EMERG, __VA_ARGS__)
#define log_err_sock(...)        log_msg_sock(LOG_ERR, __VA_ARGS__)
#define log_warn_sock(...)       log_msg_sock(LOG_WARNING, __VA_ARGS__)
#define log_notice_sock(...)     log_msg_sock(LOG_NOTICE, __VA_ARGS__)
#define log_info_sock(...)       log_msg_sock(LOG_INFO, __VA_ARGS__)
#define log_debug_sock(...)      log_msg_sock(LOG_DEBUG, __VA_ARGS__)

/**
 * Socket and proxy state logging functions
 */

void log_msg_lps (int level, int listener, proxystate *ps, char *fmt, ...);

#define log_fatal_lps(...)      log_msg_lps(LOG_EMERG, __VA_ARGS__)
#define log_err_lps(...)        log_msg_lps(LOG_ERR, __VA_ARGS__)
#define log_warn_lps(...)       log_msg_lps(LOG_WARNING, __VA_ARGS__)
#define log_notice_lps(...)     log_msg_lps(LOG_NOTICE, __VA_ARGS__)
#define log_info_lps(...)       log_msg_lps(LOG_INFO, __VA_ARGS__)
#define log_debug_lps(...)      log_msg_lps(LOG_DEBUG, __VA_ARGS__)


/**
 * Fatal logging "functions"
 */

void fail (char *fmt, ...);

#define die(...)                                        \
  log_msg(LOG_CRIT, __VA_ARGS__);                      \
  exit(1)

#define die_ps(...)                                     \
  log_msg_ps(LOG_CRIT, __VA_ARGS__);                   \
  exit(1)

#define die_sock(...)                                   \
  log_msg_sock(LOG_CRIT, __VA_ARGS__);                 \
  exit(1)
