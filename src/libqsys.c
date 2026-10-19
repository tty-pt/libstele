#define _GNU_SOURCE
#include "./../include/ttypt/qsys.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define UNUSED __attribute__((unused))

static void
qsyslog_stderr(int type UNUSED, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

qsyslog_t qsyslog = qsyslog_stderr;

void
qsyslog_set(qsyslog_t logger)
{
	qsyslog = logger;
}

/* Native strlcpy(3) is present on the BSDs/macOS and glibc >= 2.38.
 * Everywhere else (Windows, older glibc, musl) use a portable fallback. */
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
# if __GLIBC_PREREQ(2, 38)
#  define QSYS_HAVE_STRLCPY 1
# endif
#elif defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__) || \
        defined(__NetBSD__) || defined(__DragonFly__)
# define QSYS_HAVE_STRLCPY 1
#endif

#if defined(_WIN32) || !defined(QSYS_HAVE_STRLCPY)
size_t
qsys_strlcpy(char *dst, const char *src, size_t size)
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
qsys_strlcpy(char *dst, const char *src, size_t size)
{
	return strlcpy(dst, src, size);
}
#endif



#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>
#include <process.h>
#include <psapi.h>
#include <signal.h>

static HANDLE hEventLog = NULL;
static char ident_buf[128] = "Application";

void qsys_openlog(const char *ident) {
    if (ident)
        strncpy(ident_buf, ident, sizeof(ident_buf) - 1);
    hEventLog = RegisterEventSourceA(NULL, ident_buf);
}

void qsys_syslog(int priority, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    WORD type = EVENTLOG_INFORMATION_TYPE;
    if (priority <= 3)
        type = EVENTLOG_ERROR_TYPE;
    else if (priority == 4)
        type = EVENTLOG_WARNING_TYPE;

    const char *strings[1] = { buf };

    if (hEventLog) {
        ReportEventA(hEventLog, type, 0, 0, NULL, 1, 0, strings, NULL);
    } else {
        fprintf(stderr, "%s: %s\n", ident_buf, buf);
    }
}

void qsys_closelog(void) {
    if (hEventLog) {
        DeregisterEventSource(hEventLog);
        hEventLog = NULL;
    }
}

/* qsys_dlopen/dlsym/dlclose/dlerror — Windows */

void *
qsys_dlopen(const char *path, int flags UNUSED)
{
    return (void *) LoadLibraryA(path);
}

void *
qsys_dlsym(void *handle, const char *symbol)
{
    void *sym = NULL;
    FARPROC fp = GetProcAddress((HMODULE) handle, symbol);
    memcpy(&sym, &fp, sizeof(sym));
    return sym;
}

int
qsys_dlclose(void *handle)
{
    return !FreeLibrary((HMODULE) handle);
}

static char dlerror_buf[256];

const char *
qsys_dlerror(void)
{
    DWORD err_code = GetLastError();
    if (err_code == 0)
        return NULL;
    memset(dlerror_buf, 0, sizeof(dlerror_buf));
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        dlerror_buf, sizeof(dlerror_buf), NULL);
    return dlerror_buf;
}

/* qsys_mkstemps/qsys_fsync/qsys_setenv/qsys_unsetenv — Windows */

int
qsys_mkstemps(char *tmpl, int suffixlen)
{
    size_t len, body;
    char *tmp;
    int fd = -1;
    int tries;

    if (suffixlen < 0) {
        errno = EINVAL;
        return -1;
    }
    len = strlen(tmpl);
    if ((size_t) suffixlen >= len) {
        errno = EINVAL;
        return -1;
    }
    body = len - (size_t) suffixlen;
    if (body < 6 || memcmp(tmpl + body - 6, "XXXXXX", 6) != 0) {
        errno = EINVAL;
        return -1;
    }

    tmp = malloc(len + 1);
    if (!tmp)
        return -1;

    for (tries = 0; tries < 100; tries++) {
        memcpy(tmp, tmpl, len + 1);
        tmp[body] = '\0';
        if (_mktemp_s(tmp, body + 1) != 0)
            break;
        memcpy(tmp + body, tmpl + body, (size_t) suffixlen + 1);
        fd = _open(tmp, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
                   _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            memcpy(tmpl, tmp, len + 1);
            free(tmp);
            return fd;
        }
        if (errno != EEXIST)
            break;
    }

    free(tmp);
    if (fd < 0 && errno == 0)
        errno = EEXIST;
    return -1;
}

int
qsys_fsync(int fd)
{
    return _commit(fd);
}

int
qsys_setenv(const char *name, const char *value, int overwrite)
{
    errno_t e;

    if (!overwrite && getenv(name))
        return 0;
    e = _putenv_s(name, value);
    if (e) {
        errno = e;
        return -1;
    }
    return 0;
}

int
qsys_unsetenv(const char *name)
{
    errno_t e = _putenv_s(name, "");

    if (e) {
        errno = e;
        return -1;
    }
    return 0;
}

struct tm *
qsys_gmtime_r(const time_t *timer, struct tm *result)
{
    return gmtime_s(result, timer) == 0 ? result : NULL;
}

char *
qsys_realpath(const char *path, char *resolved_path)
{
    if (resolved_path)
        return _fullpath(resolved_path, path, _MAX_PATH);
    char *buf = malloc(_MAX_PATH);
    if (!buf)
        return NULL;
    if (!_fullpath(buf, path, _MAX_PATH)) {
        free(buf);
        return NULL;
    }
    return buf;
}

int
qsys_mkdir(const char *path)
{
    return _mkdir(path);
}

int
qsys_ftruncate(int fd, off_t length)
{
    return _chsize_s(fd, (long long)length) == 0 ? 0 : (errno = EIO, -1);
}

int
qsys_dprintf(int fd, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return n;
    if ((size_t)n >= sizeof(buf))
        n = (int)sizeof(buf) - 1;
    return _write(fd, buf, (unsigned)n) == n ? n : -1;
}

int
qsys_flock(int fd, int op)
{
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    OVERLAPPED ov;
    if (h == INVALID_HANDLE_VALUE || h == (HANDLE)-1) {
        errno = EBADF;
        return -1;
    }
    memset(&ov, 0, sizeof(ov));
    if (op == QSYS_LOCK_UN)
        return UnlockFileEx(h, 0, 1, 0, &ov) ? 0 : -1;
    if (LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK |
            ((op & QSYS_LOCK_NB) ? LOCKFILE_FAIL_IMMEDIATELY : 0),
            0, 1, 0, &ov))
        return 0;
    errno = GetLastError() == ERROR_LOCK_VIOLATION ? EWOULDBLOCK : EACCES;
    return -1;
}

uint64_t
qsys_now_ms(void)
{
    return (uint64_t)GetTickCount64();
}

void
qsys_sleep_ms(unsigned ms)
{
    Sleep(ms);
}

int
qsys_pid_alive(pid_t pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    DWORD code = 0;
    int alive = 0;
    if (h) {
        if (GetExitCodeProcess(h, &code))
            alive = code == STILL_ACTIVE;
        CloseHandle(h);
    }
    return alive ? 0 : -1;
}

uint64_t
qsys_self_rss_bytes(void)
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (uint64_t)pmc.WorkingSetSize;
    return 0;
}

int
qsys_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    return WSAPoll(fds, (ULONG)nfds, timeout);
}

void
qsys_stop_on_signal(void (*handler)(int))
{
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
}

void
qsys_ignore_pipe(void)
{
}

/* Socket shims - Windows */
int qsys_sock_init(void)
{
    static int done;
    if (!done) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return -1;
        done = 1;
    }
    return 0;
}

qsys_socket_t qsys_sock_create(int domain, int type, int protocol)
{
    qsys_sock_init();
    return (qsys_socket_t)socket(domain, type, protocol);
}

int qsys_sock_close(qsys_socket_t fd)
{
    return closesocket((SOCKET)fd);
}

ssize_t qsys_sock_recv(qsys_socket_t fd, void *buf, size_t len, int flags)
{
    return (ssize_t)recv((SOCKET)fd, (char *)buf, (int)len, flags);
}

ssize_t qsys_sock_send(qsys_socket_t fd, const void *buf, size_t len, int flags)
{
    return (ssize_t)send((SOCKET)fd, (const char *)buf, (int)len, flags);
}

int qsys_sock_bind(qsys_socket_t fd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind((SOCKET)fd, addr, (int)addrlen);
}

int qsys_sock_listen(qsys_socket_t fd, int backlog)
{
    return listen((SOCKET)fd, backlog);
}

qsys_socket_t qsys_sock_accept(qsys_socket_t fd, struct sockaddr *addr, socklen_t *addrlen)
{
    int alen = addrlen ? (int)*addrlen : 0;
    qsys_socket_t res = (qsys_socket_t)accept((SOCKET)fd, addr, addrlen ? &alen : NULL);
    if (addrlen) *addrlen = (socklen_t)alen;
    return res;
}

int qsys_sock_connect(qsys_socket_t fd, const struct sockaddr *addr, socklen_t addrlen)
{
    return connect((SOCKET)fd, addr, (int)addrlen);
}

int qsys_sock_set_nonblock(qsys_socket_t fd, int nonblock)
{
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket((SOCKET)fd, FIONBIO, &mode) == 0 ? 0 : -1;
}

int qsys_sock_wouldblock(void)
{
    int e = WSAGetLastError();
    return e == WSAEWOULDBLOCK;
}

const char *qsys_sock_strerror(void)
{
    static char buf[40];
    snprintf(buf, sizeof(buf), "winsock error %d", WSAGetLastError());
    return buf;
}

#else /* POSIX ---------------------------------------------------------------- */

#include <syslog.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>

void qsys_openlog(const char *ident) {
    openlog(ident, LOG_PID | LOG_CONS, LOG_USER);
}

void qsys_syslog(int priority, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsyslog(priority, fmt, ap);
    va_end(ap);
}

void qsys_closelog(void) {
    closelog();
}

/* qsys_dlopen/dlsym/dlclose/dlerror — POSIX */

void *
qsys_dlopen(const char *path, int flags)
{
    int posix_flags = RTLD_NOW | RTLD_LOCAL;
    if (flags & QSYS_RTLD_NODELETE)
        posix_flags |= RTLD_NODELETE;
    return dlopen(path, posix_flags);
}

void *
qsys_dlsym(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

int
qsys_dlclose(void *handle)
{
    return dlclose(handle);
}

const char *
qsys_dlerror(void)
{
    return dlerror();
}

/* qsys_mkstemps/qsys_fsync/qsys_setenv/qsys_unsetenv — POSIX */

int
qsys_mkstemps(char *tmpl, int suffixlen)
{
    return mkstemps(tmpl, suffixlen);
}

int
qsys_fsync(int fd)
{
    return fsync(fd);
}

int
qsys_setenv(const char *name, const char *value, int overwrite)
{
    return setenv(name, value, overwrite);
}

int
qsys_unsetenv(const char *name)
{
    return unsetenv(name);
}

struct tm *
qsys_gmtime_r(const time_t *timer, struct tm *result)
{
	return gmtime_r(timer, result);
}

char *
qsys_realpath(const char *path, char *resolved_path)
{
	return realpath(path, resolved_path);
}

int
qsys_mkdir(const char *path)
{
	return mkdir(path, 0755);
}

int
qsys_ftruncate(int fd, off_t length)
{
	return ftruncate(fd, length);
}

int
qsys_dprintf(int fd, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int ret = vdprintf(fd, fmt, ap);
	va_end(ap);
	return ret;
}

int
qsys_flock(int fd, int op)
{
	int posix_op = 0;
	if (op & QSYS_LOCK_SH) posix_op |= LOCK_SH;
	if (op & QSYS_LOCK_EX) posix_op |= LOCK_EX;
	if (op & QSYS_LOCK_NB) posix_op |= LOCK_NB;
	if (op & QSYS_LOCK_UN) posix_op |= LOCK_UN;
	return flock(fd, posix_op);
}

uint64_t
qsys_now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void
qsys_sleep_ms(unsigned ms)
{
	struct timespec req = {
		.tv_sec = ms / 1000,
		.tv_nsec = (long)(ms % 1000) * 1000000
	};
	nanosleep(&req, NULL);
}

int
qsys_pid_alive(pid_t pid)
{
	return kill(pid, 0);
}

uint64_t
qsys_self_rss_bytes(void)
{
	struct rusage ru;
	if (getrusage(RUSAGE_SELF, &ru) == 0)
		return (uint64_t)ru.ru_maxrss * 1024;
	return 0;
}

int
qsys_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	return poll(fds, nfds, timeout);
}

void
qsys_stop_on_signal(void (*handler)(int))
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
}

void
qsys_ignore_pipe(void)
{
	signal(SIGPIPE, SIG_IGN);
}

/* Socket shims - POSIX */
int qsys_sock_init(void)
{
    return 0;
}

qsys_socket_t qsys_sock_create(int domain, int type, int protocol)
{
    return socket(domain, type, protocol);
}

int qsys_sock_close(qsys_socket_t fd)
{
    return close(fd);
}

ssize_t qsys_sock_recv(qsys_socket_t fd, void *buf, size_t len, int flags)
{
    return recv(fd, buf, len, flags);
}

ssize_t qsys_sock_send(qsys_socket_t fd, const void *buf, size_t len, int flags)
{
    return send(fd, buf, len, flags);
}

int qsys_sock_bind(qsys_socket_t fd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(fd, addr, addrlen);
}

int qsys_sock_listen(qsys_socket_t fd, int backlog)
{
    return listen(fd, backlog);
}

qsys_socket_t qsys_sock_accept(qsys_socket_t fd, struct sockaddr *addr, socklen_t *addrlen)
{
    return accept(fd, addr, addrlen);
}

int qsys_sock_connect(qsys_socket_t fd, const struct sockaddr *addr, socklen_t addrlen)
{
    return connect(fd, addr, addrlen);
}

int qsys_sock_set_nonblock(qsys_socket_t fd, int nonblock)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    if (nonblock)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int qsys_sock_wouldblock(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

const char *qsys_sock_strerror(void)
{
    return strerror(errno);
}

#endif /* _WIN32 */
