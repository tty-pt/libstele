/*
 * Copyright (c) 2026, tty-pt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#ifndef TTYPT_STELE_H
#define TTYPT_STELE_H

/**
 * @file stele.h
 * @brief Cross-platform system shims: logging, dl, env, strings.
 *
 * Portable wrappers (POSIX mirrors / Win32 emulation) for the low-level
 * facilities the site runtime needs on both platforms.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#ifdef __APPLE__
/** @brief Mark a symbol as weak-linked (macOS weak import). */
#define WEAK __attribute__((weak_import))
#else
/** @brief Mark a symbol as weak (ELF weak attribute). */
#define WEAK __attribute__((weak))
#endif

/** @brief Mark intentionally-unused symbols or parameters. */
#define UNUSED __attribute__((unused))

/**
 * @brief Log severity levels, syslog-style.
 */
enum {
    /** Error conditions. */
    STL_LOG_ERR,
    /** Warning conditions. */
    STL_LOG_WARNING,
    /** Notable events. */
    STL_LOG_NOTICE,
    /** Informational messages. */
    STL_LOG_INFO,
    /** Debug-level messages. */
    STL_LOG_DEBUG
};

typedef void (*stllog_t)(int type, const char *fmt, ...);
extern stllog_t stllog WEAK;
extern stllog_t stelelog WEAK;

void stllog_set(stllog_t logger);
stllog_t stl_get_logger(void);
void stl_openlog(const char *ident);
void stl_syslog(int type, const char *fmt, ...);
void stl_closelog(void);

/**
 * @brief Log a formatted message tagged with the calling function's name.
 */
#ifdef _WIN32
#define LOG(TYPE, ...) { \
	stllog_t _logger = stl_get_logger(); \
	if (_logger) { \
		_logger(STL_LOG_##TYPE, "%s: ", __func__); \
		_logger(STL_LOG_##TYPE, __VA_ARGS__); \
	} \
}
#else
#define LOG(TYPE, ...) { \
	stllog_t _logger = stllog ? stllog : stelelog; \
	if (_logger) { \
		_logger(STL_LOG_##TYPE, "%s: ", __func__); \
		_logger(STL_LOG_##TYPE, __VA_ARGS__); \
	} \
}
#endif

#define WARN(...) LOG(WARNING, __VA_ARGS__)
#define ERR(...) LOG(ERR, __VA_ARGS__)

#define CBUG(cond, ...) \
	if (cond) { \
		ERR(__VA_ARGS__); \
		abort(); \
	}

size_t stl_strlcpy(char *dst, const char *src, size_t size);
size_t stl_strlcat(char *dst, const char *src, size_t size);

#define STL_RTLD_NODELETE 1

void *stl_dlopen(const char *filename, int flags);
void *stl_dlsym(void *handle, const char *symbol);
int stl_dlclose(void *handle);
char *stl_dlerror(void);

int stl_mkstemps(char *template, int suffixlen);
int stl_fsync(int fd);

int stl_setenv(const char *name, const char *value, int overwrite);
int stl_unsetenv(const char *name);
char *stl_getenv(const char *name);

struct tm *stl_gmtime_r(const time_t *timep, struct tm *result);
char *stl_realpath(const char *path, char *resolved_path);
int stl_mkdir(const char *path);
int stl_ftruncate(int fd, off_t length);
int stl_dprintf(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Advisory file lock flags */
#define STL_LOCK_SH 1
#define STL_LOCK_EX 2
#define STL_LOCK_NB 4
#define STL_LOCK_UN 8

int stl_flock(int fd, int op);
uint64_t stl_now_ms(void);
void stl_sleep_ms(unsigned ms);
int stl_pid_alive(pid_t pid);
uint64_t stl_self_rss_bytes(void);

/* Cross-platform polling and socket shims */
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
typedef SOCKET stl_socket_t;
typedef unsigned long nfds_t;
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
typedef int stl_socket_t;
#endif

int stl_poll(struct pollfd *fds, nfds_t nfds, int timeout);
void stl_stop_on_signal(void (*handler)(int));
void stl_ignore_pipe(void);

int stl_sock_init(void);
stl_socket_t stl_sock_create(int domain, int type, int protocol);
int stl_sock_close(stl_socket_t fd);
ssize_t stl_sock_recv(stl_socket_t fd, void *buf, size_t len, int flags);
ssize_t stl_sock_send(stl_socket_t fd, const void *buf, size_t len, int flags);
int stl_sock_bind(stl_socket_t fd, const struct sockaddr *addr, socklen_t addrlen);
int stl_sock_listen(stl_socket_t fd, int backlog);
stl_socket_t stl_sock_accept(stl_socket_t fd, struct sockaddr *addr, socklen_t *addrlen);
int stl_sock_connect(stl_socket_t fd, const struct sockaddr *addr, socklen_t addrlen);
int stl_sock_set_nonblock(stl_socket_t fd, int nonblock);
int stl_sock_wouldblock(void);
const char *stl_sock_strerror(void);

/* stele_* aliases */
typedef stl_socket_t stele_socket_t;
typedef stllog_t stelelog_t;

#define stelelog stllog
#define stelelog_set stllog_set
#define stele_openlog stl_openlog
#define stele_syslog stl_syslog
#define stele_closelog stl_closelog
#define stele_strlcpy stl_strlcpy
#define stele_strlcat stl_strlcat
#define stele_dlopen stl_dlopen
#define stele_dlsym stl_dlsym
#define stele_dlclose stl_dlclose
#define stele_dlerror stl_dlerror
#define stele_mkstemps stl_mkstemps
#define stele_fsync stl_fsync
#define stele_setenv stl_setenv
#define stele_unsetenv stl_unsetenv
#define stele_getenv stl_getenv
#define stele_gmtime_r stl_gmtime_r
#define stele_realpath stl_realpath
#define stele_mkdir stl_mkdir
#define stele_ftruncate stl_ftruncate
#define stele_dprintf stl_dprintf
#define stele_flock stl_flock
#define stele_now_ms stl_now_ms
#define stele_sleep_ms stl_sleep_ms
#define stele_pid_alive stl_pid_alive
#define stele_self_rss_bytes stl_self_rss_bytes
#define stele_poll stl_poll
#define stele_stop_on_signal stl_stop_on_signal
#define stele_ignore_pipe stl_ignore_pipe
#define stele_sock_init stl_sock_init
#define stele_sock_create stl_sock_create
#define stele_sock_close stl_sock_close
#define stele_sock_recv stl_sock_recv
#define stele_sock_send stl_sock_send
#define stele_sock_bind stl_sock_bind
#define stele_sock_listen stl_sock_listen
#define stele_sock_accept stl_sock_accept
#define stele_sock_connect stl_sock_connect
#define stele_sock_set_nonblock stl_sock_set_nonblock
#define stele_sock_wouldblock stl_sock_wouldblock
#define stele_sock_strerror stl_sock_strerror

#ifdef __cplusplus
}
#endif

#endif /* TTYPT_STELE_H */
