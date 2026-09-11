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

#else /* POSIX ---------------------------------------------------------------- */

#include <syslog.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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

#endif /* _WIN32 */
