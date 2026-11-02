#define _GNU_SOURCE
#include "./../include/ttypt/stele.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define UNUSED __attribute__((unused))

#ifndef __GLIBC_PREREQ
# define __GLIBC_PREREQ(maj, min) 0
#endif

static void
stllog_stderr(int type UNUSED, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

stllog_t stllog = stllog_stderr;

void
stllog_set(stllog_t logger)
{
	stllog = logger;
}

stllog_t
stl_get_logger(void)
{
	return stllog ? stllog : stelelog;
}

/* Native strlcpy(3) is present on the BSDs/macOS and glibc >= 2.38.
 * Everywhere else (Windows, older glibc, musl) use a portable fallback. */
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
# if __GLIBC_PREREQ(2, 38)
#  define STL_HAVE_STRLCPY 1
# endif
#elif defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
        defined(__NetBSD__) || defined(__DragonFly__)
# define STL_HAVE_STRLCPY 1
#endif

#if defined(_WIN32) || !defined(STL_HAVE_STRLCPY)
size_t
stl_strlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen = strlen(src);

	if (size > 0) {
		size_t copylen = srclen;
		if (copylen >= size)
			copylen = size - 1;
		memcpy(dst, src, copylen);
		dst[copylen] = '\0';
	}
	return srclen;
}
#else
size_t
stl_strlcpy(char *dst, const char *src, size_t size)
{
	return strlcpy(dst, src, size);
}
#endif

#if defined(_WIN32) || !defined(STL_HAVE_STRLCPY)
size_t
stl_strlcat(char *dst, const char *src, size_t size)
{
	size_t dlen = strlen(dst);
	size_t slen = strlen(src);
	if (dlen >= size)
		return size + slen;
	size_t copy = size - dlen - 1;
	if (copy > slen)
		copy = slen;
	memcpy(dst + dlen, src, copy);
	dst[dlen + copy] = '\0';
	return dlen + slen;
}
#else
size_t
stl_strlcat(char *dst, const char *src, size_t size)
{
	return strlcat(dst, src, size);
}
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>
#include <errno.h>

void stl_openlog(const char *ident UNUSED) {}
void stl_syslog(int priority UNUSED, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}
void stl_closelog(void) {}

void *stl_dlopen(const char *path, int flags UNUSED) {
    if (!path) return NULL;
    return (void *)LoadLibraryA(path);
}

void *stl_dlsym(void *handle, const char *symbol) {
    if (!handle || !symbol) return NULL;
    return (void *)(uintptr_t)GetProcAddress((HMODULE)handle, symbol);
}

int stl_dlclose(void *handle) {
    if (!handle) return -1;
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

static char s_dlerror_buf[256];
char *stl_dlerror(void) {
    DWORD err = GetLastError();
    if (!err) return NULL;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, 0, s_dlerror_buf, sizeof(s_dlerror_buf), NULL);
    SetLastError(0);
    return s_dlerror_buf;
}

int stl_mkstemps(char *tmpl, int suffixlen) {
    (void)suffixlen;
    return _open(tmpl, _O_RDWR | _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
}

int stl_fsync(int fd) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    return FlushFileBuffers(h) ? 0 : -1;
}

int stl_setenv(const char *name, const char *value, int overwrite) {
    if (!name || !value) return -1;
    if (!overwrite && getenv(name)) return 0;
    return _putenv_s(name, value);
}

int stl_unsetenv(const char *name) {
    if (!name) return -1;
    return _putenv_s(name, "");
}

char *stl_getenv(const char *name) {
    return getenv(name);
}

struct tm *stl_gmtime_r(const time_t *timer, struct tm *result) {
    if (!timer || !result) return NULL;
    return gmtime_s(result, timer) == 0 ? result : NULL;
}

char *stl_realpath(const char *path, char *resolved_path) {
    if (!path) return NULL;
    return _fullpath(resolved_path, path, 1024);
}

int stl_mkdir(const char *path) {
    return _mkdir(path);
}

int stl_ftruncate(int fd, off_t length) {
    return _chsize(fd, length);
}

int stl_dprintf(int fd, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    if (n > 0) {
        _write(fd, buf, n);
    }
    return n;
}

int stl_flock(int fd, int op) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov = {0};
    if (op == STL_LOCK_UN)
        return UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &ov) ? 0 : -1;
    DWORD flags = ((op & STL_LOCK_EX) ? LOCKFILE_EXCLUSIVE_LOCK : 0) |
                  ((op & STL_LOCK_NB) ? LOCKFILE_FAIL_IMMEDIATELY : 0);
    return LockFileEx(h, flags, 0, MAXDWORD, MAXDWORD, &ov) ? 0 : -1;
}

uint64_t stl_now_ms(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

void stl_sleep_ms(unsigned ms) {
    Sleep(ms);
}

int stl_pid_alive(pid_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return -1;
    DWORD code;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return (code == STILL_ACTIVE) ? 0 : -1;
}

uint64_t stl_self_rss_bytes(void) {
    return 0;
}

int stl_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return WSAPoll((WSAPOLLFD *)fds, (ULONG)nfds, timeout);
}

void stl_stop_on_signal(void (*handler)(int)) { (void)handler; }
void stl_ignore_pipe(void) {}

int stl_sock_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}

stl_socket_t stl_sock_create(int domain, int type, int protocol) {
    stl_sock_init();
    return (stl_socket_t)socket(domain, type, protocol);
}

int stl_sock_close(stl_socket_t fd) {
    return closesocket((SOCKET)fd);
}

ssize_t stl_sock_recv(stl_socket_t fd, void *buf, size_t len, int flags) {
    return recv((SOCKET)fd, (char *)buf, (int)len, flags);
}

ssize_t stl_sock_send(stl_socket_t fd, const void *buf, size_t len, int flags) {
    return send((SOCKET)fd, (const char *)buf, (int)len, flags);
}

int stl_sock_bind(stl_socket_t fd, const struct sockaddr *addr, socklen_t addrlen) {
    return bind((SOCKET)fd, addr, addrlen);
}

int stl_sock_listen(stl_socket_t fd, int backlog) {
    return listen((SOCKET)fd, backlog);
}

stl_socket_t stl_sock_accept(stl_socket_t fd, struct sockaddr *addr, socklen_t *addrlen) {
    int alen = addrlen ? (int)*addrlen : 0;
    stl_socket_t res = (stl_socket_t)accept((SOCKET)fd, addr, addrlen ? &alen : NULL);
    if (addrlen) *addrlen = (socklen_t)alen;
    return res;
}

int stl_sock_connect(stl_socket_t fd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect((SOCKET)fd, addr, addrlen);
}

int stl_sock_set_nonblock(stl_socket_t fd, int nonblock) {
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket((SOCKET)fd, FIONBIO, &mode);
}

int stl_sock_wouldblock(void) {
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK;
}

const char *stl_sock_strerror(void) {
    static char buf[128];
    snprintf(buf, sizeof(buf), "WSA error %d", WSAGetLastError());
    return buf;
}

#else
/* POSIX */
#include <syslog.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>
#include <poll.h>

void stl_openlog(const char *ident) {
    openlog(ident, LOG_PID | LOG_NDELAY, LOG_USER);
}

void stl_syslog(int priority, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsyslog(priority, fmt, va);
    va_end(va);
}

void stl_closelog(void) {
    closelog();
}

void *stl_dlopen(const char *path, int flags) {
    int posix_flags = RTLD_NOW | RTLD_LOCAL;
    if (flags & STL_RTLD_NODELETE)
#ifdef RTLD_NODELETE
        posix_flags |= RTLD_NODELETE;
#else
        (void)0;
#endif
    return dlopen(path, posix_flags);
}

void *stl_dlsym(void *handle, const char *symbol) {
    return dlsym(handle, symbol);
}

int stl_dlclose(void *handle) {
    return dlclose(handle);
}

char *stl_dlerror(void) {
    return dlerror();
}

int stl_mkstemps(char *tmpl, int suffixlen) {
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__DragonFly__) || \
    (defined(__GLIBC__) && __GLIBC_PREREQ(2, 11)) || \
    (defined(__linux__) && !defined(__GLIBC__))
    return mkstemps(tmpl, suffixlen);
#else
    (void)suffixlen;
    return mkstemp(tmpl);
#endif
}

int stl_fsync(int fd) {
    return fsync(fd);
}

int stl_setenv(const char *name, const char *value, int overwrite) {
    return setenv(name, value, overwrite);
}

int stl_unsetenv(const char *name) {
    return unsetenv(name);
}

char *stl_getenv(const char *name) {
    return getenv(name);
}

struct tm *stl_gmtime_r(const time_t *timer, struct tm *result) {
    return gmtime_r(timer, result);
}

char *stl_realpath(const char *path, char *resolved_path) {
    return realpath(path, resolved_path);
}

int stl_mkdir(const char *path) {
    return mkdir(path, 0755);
}

int stl_ftruncate(int fd, off_t length) {
    return ftruncate(fd, length);
}

int stl_dprintf(int fd, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int n = vdprintf(fd, fmt, va);
    va_end(va);
    return n;
}

int stl_flock(int fd, int op) {
	int posix_op = 0;
	if (op & STL_LOCK_SH) posix_op |= LOCK_SH;
	if (op & STL_LOCK_EX) posix_op |= LOCK_EX;
	if (op & STL_LOCK_NB) posix_op |= LOCK_NB;
	if (op & STL_LOCK_UN) posix_op |= LOCK_UN;
	return flock(fd, posix_op);
}

uint64_t stl_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void stl_sleep_ms(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int stl_pid_alive(pid_t pid) {
    return kill(pid, 0);
}

uint64_t stl_self_rss_bytes(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages = 0;
    long resident = 0;
    if (fscanf(f, "%ld %ld", &pages, &resident) == 2) {
        fclose(f);
        return (uint64_t)resident * (uint64_t)sysconf(_SC_PAGESIZE);
    }
    fclose(f);
    return 0;
}

int stl_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return poll(fds, nfds, timeout);
}

void stl_stop_on_signal(void (*handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void stl_ignore_pipe(void) {
    signal(SIGPIPE, SIG_IGN);
}

int stl_sock_init(void) {
    return 0;
}

stl_socket_t stl_sock_create(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int stl_sock_close(stl_socket_t fd) {
    return close(fd);
}

ssize_t stl_sock_recv(stl_socket_t fd, void *buf, size_t len, int flags) {
    return recv(fd, buf, len, flags);
}

ssize_t stl_sock_send(stl_socket_t fd, const void *buf, size_t len, int flags) {
    return send(fd, buf, len, flags);
}

int stl_sock_bind(stl_socket_t fd, const struct sockaddr *addr, socklen_t addrlen) {
    return bind(fd, addr, addrlen);
}

int stl_sock_listen(stl_socket_t fd, int backlog) {
    return listen(fd, backlog);
}

stl_socket_t stl_sock_accept(stl_socket_t fd, struct sockaddr *addr, socklen_t *addrlen) {
    return accept(fd, addr, addrlen);
}

int stl_sock_connect(stl_socket_t fd, const struct sockaddr *addr, socklen_t addrlen) {
    return connect(fd, addr, addrlen);
}

int stl_sock_set_nonblock(stl_socket_t fd, int nonblock) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nonblock)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int stl_sock_wouldblock(void) {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

const char *stl_sock_strerror(void) {
    return strerror(errno);
}

#endif /* _WIN32 */
