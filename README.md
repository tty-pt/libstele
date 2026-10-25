<p align="center">
  <img src="logo.png" alt="libstele logo" width="220" />
</p>

# libstele

[![C99](https://img.shields.io/badge/C-C99-555?logo=c)](#)
[![BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](#)
[![Dependencies](https://img.shields.io/badge/dependencies-zero-success)](#)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20BSDs-blueviolet)](#)

> Zero-dependency system substrate for plain C: cross-platform sockets, event polling, pluggable logging, process diagnostics, monotonic timing, advisory file locking, dynamic module loading, and POSIX shims.

**libstele** (formerly `libqsys`) serves as the foundational system layer for the `tty-pt` ecosystem (including [axil](https://github.com/tty-pt/axil), [libcorm](https://github.com/tty-pt/libcorm), and [liblamina](https://github.com/tty-pt/liblamina)). It normalizes disparate POSIX, BSD, and Windows interfaces into a lean, predictable C99 API without pulling in heavy external runtimes like GLib, APR, or libuv.

> **Note on Naming & API Stability**: The project is transitioning to the name **libstele**. To ensure uninterrupted backward compatibility across the ecosystem, the public C API, headers (`<ttypt/qsys.h>`), symbols (`qsys_*`), shared library names (`libqsys.so`), and package identifiers (`qsys`) currently retain the `qsys` naming convention.

---

## Contents

- [Why libstele? (The 30-Second Elevator Pitch)](#why-libstele-the-30-second-elevator-pitch)
- [The Six Core Jobs It Does](#the-six-core-jobs-it-does)
- [Architecture & Platform Mapping](#architecture--platform-mapping)
- [Quickstart Examples](#quickstart-examples)
  - [1. Pluggable Logging & Custom Sinks](#1-pluggable-logging--custom-sinks)
  - [2. Cross-Platform Non-Blocking Sockets & Polling](#2-cross-platform-non-blocking-sockets--polling)
  - [3. Process Diagnostics & Monotonic Timing](#3-process-diagnostics--monotonic-timing)
  - [4. Advisory File Locking & Safe File I/O](#4-advisory-file-locking--safe-file-io)
- [API Reference](#api-reference)
  - [Logging & Syslog](#logging--syslog)
  - [Sockets & Event Polling](#sockets--event-polling)
  - [Process & Memory Diagnostics](#process--memory-diagnostics)
  - [Timing & Clocks](#timing--clocks)
  - [Dynamic Library Loading](#dynamic-library-loading)
  - [Filesystem, Locking & POSIX Shims](#filesystem-locking--posix-shims)
  - [Strings, Environment & Time](#strings-environment--time)
  - [Compiler Macro Normalization](#compiler-macro-normalization)
- [Installation & Build](#installation--build)
- [License](#license)

---

## Why libstele? (The 30-Second Elevator Pitch)

Building cross-platform micro-daemons, games, and high-performance servers in plain C usually forces an unpleasant compromise:

1. **Pull in bloated runtimes** like GLib or APR that introduce megabytes of dependencies, complicated type systems, and custom event loops.
2. **Scatter hundreds of `#ifdef _WIN32` blocks** throughout your networking, timing, file I/O, and process management code.

`libstele` provides a clean third path: **a zero-dependency, header-and-shared-library micro-substrate**. You write clean, standard C against `<ttypt/qsys.h>`, and the runtime transparently bridges POSIX and Win32 under the hood—mapping everything from Winsock initialisation and non-blocking flags to monotonic time, RSS measurement, and advisory file locks (`flock`).

---

## The Six Core Jobs It Does

1. **Pluggable Logging Pipeline & Assertions**: `LOG()`, `WARN()`, `ERR()`, and `CBUG()` macros with automatic caller function tagging (`__func__`), thread-safe `qsyslog_set()` sinks, and native syslog bridging (`qsys_openlog`, `qsys_syslog`, `qsys_closelog` / Windows Event Log).
2. **Cross-Platform Sockets & Event Polling**: Unified socket type (`qsys_socket_t`), transparent Winsock startup (`qsys_sock_init`), non-blocking control (`qsys_sock_set_nonblock`), error translation (`qsys_sock_strerror`), and poll multiplexing (`qsys_poll`).
3. **High-Resolution Timing & Process Diagnostics**: Monotonic timestamps in milliseconds (`qsys_now_ms`), portable millisecond sleep (`qsys_sleep_ms`), process liveness checks (`qsys_pid_alive`), and Resident Set Size queries (`qsys_self_rss_bytes`).
4. **Dynamic Module Loading & Safe Hot-Reloading**: Seamless dynamic library abstraction (`qsys_dlopen`, `qsys_dlsym`, `qsys_dlclose`, `qsys_dlerror`) featuring `QSYS_RTLD_NODELETE` semantics to keep symbol tables intact during plugin teardowns.
5. **POSIX Filesystem, Locking & Safe File I/O**: Cross-platform advisory file locking (`qsys_flock` with `QSYS_LOCK_SH/EX/NB/UN`), directory creation (`qsys_mkdir`), formatted descriptor printing (`qsys_dprintf`), descriptor truncation (`qsys_ftruncate`), temp files (`qsys_mkstemps`), and path canonicalization (`qsys_realpath`).
6. **Signal Handling & Environment Control**: Clean process termination traps (`qsys_stop_on_signal`), broken-pipe signal suppression (`qsys_ignore_pipe` to prevent socket disconnect crashes), and cross-platform environment setters (`qsys_setenv`, `qsys_unsetenv`).
7. **Universal Isomorphic C Contract (`<ttypt/iso.h>`)**: Platform-agnostic `ISO_EXPORT`, `ISO_EXPORT_AS`, and `ISO_IMPORT` macros for writing C modules that compile symmetrically into native `.so` shared libraries and WebAssembly `.wasm` browser reactors without preprocessor branching.

---

## Architecture & Platform Mapping

```text
  +-----------------------------------------------------------------------+
  |              Applications & Daemons (axil, cormd, sevenways)          |
  +-----------------------------------------------------------------------+
                                      |
                                      v
  +-----------------------------------------------------------------------+
  |                          libstele (libqsys)                           |
  |  +--------------------+  +--------------------+  +-----------------+  |
  |  |  Logging & Assert  |  |  Sockets & Polling |  | Process & Time  |  |
  |  |  LOG/WARN/ERR/CBUG |  |  qsys_sock_* / poll|  | now_ms/rss/alive|  |
  |  +--------------------+  +--------------------+  +-----------------+  |
  |  +--------------------+  +--------------------+  +-----------------+  |
  |  |  Dynamic Loading   |  |  Filesystem & Lock |  | Signals & Env   |  |
  |  |  qsys_dlopen/dlsym |  |  flock/mkdir/dprintf| | stop_on_sig/pipe|  |
  |  +--------------------+  +--------------------+  +-----------------+  |
  +-----------------------------------------------------------------------+
            |                                         |
     (POSIX / Unix)                             (Win32 / MSVC)
            v                                         v
  +--------------------+                    +--------------------+
  | dlopen / poll / BSD|                    | LoadLibrary / WS2  |
  | POSIX sockets / RSS|                    | PSAPI / EventLog   |
  +--------------------+                    +--------------------+
```

---

## Quickstart Examples

### 1. Pluggable Logging & Custom Sinks

Install a custom sink callback to route messages to files, memory buffers, or remote log aggregators:

```c
#include <stdio.h>
#include <ttypt/qsys.h>

static void file_logger(int level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[SEV-%d] ", level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

int main(void) {
    // Route logs to custom handler (defaults to stderr)
    qsyslog_set(file_logger);

    LOG(INFO, "Starting daemon initialization...");
    WARN("Cache capacity at %d%%", 90);

    // Conditional assertion: logs message and invokes abort() if condition is true
    int fd = -1;
    CBUG(fd < 0, "Failed to acquire master lock descriptor!");

    return 0;
}
```

### 2. Cross-Platform Non-Blocking Sockets & Polling

Set up a cross-platform non-blocking TCP server and poll for connections without writing a single `#ifdef _WIN32`:

```c
#include <stdio.h>
#include <string.h>
#include <ttypt/qsys.h>

int main(void) {
    // Transparently initializes Winsock on Windows, no-op on POSIX
    if (qsys_sock_init() != 0) {
        ERR("Failed to initialize socket runtime: %s", qsys_sock_strerror());
        return 1;
    }

    // Ignore SIGPIPE so disconnected clients don't terminate the process
    qsys_ignore_pipe();

    // Create listening socket
    qsys_socket_t server = qsys_sock_create(AF_INET, SOCK_STREAM, 0);
    qsys_sock_set_nonblock(server, 1);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    qsys_sock_bind(server, (struct sockaddr *)&addr, sizeof(addr));
    qsys_sock_listen(server, 128);
    LOG(INFO, "Server listening on port 8080...");

    // Multiplex readiness using qsys_poll
    struct pollfd pfd;
    pfd.fd = server;
    pfd.events = POLLIN;

    int ready = qsys_poll(&pfd, 1, 5000); // 5-second timeout
    if (ready > 0 && (pfd.revents & POLLIN)) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        qsys_socket_t client = qsys_sock_accept(server, (struct sockaddr *)&client_addr, &client_len);
        if (client >= 0) {
            LOG(INFO, "Client connected successfully!");
            qsys_sock_send(client, "HELLO\n", 6, 0);
            qsys_sock_close(client);
        }
    }

    qsys_sock_close(server);
    return 0;
}
```

### 3. Process Diagnostics & Monotonic Timing

Query resident set size (RSS), verify process liveness, and measure elapsed time using monotonic clocks:

```c
#include <stdio.h>
#include <ttypt/qsys.h>

static volatile int running = 1;
static void handle_exit(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    // Trap SIGINT and SIGTERM for clean shutdown
    qsys_stop_on_signal(handle_exit);

    uint64_t start_ms = qsys_now_ms();
    LOG(INFO, "Daemon started at %llu ms", (unsigned long long)start_ms);

    // Monitor memory consumption
    uint64_t rss = qsys_self_rss_bytes();
    LOG(INFO, "Initial memory RSS: %llu KB", (unsigned long long)(rss / 1024));

    // Simulate work loop
    qsys_sleep_ms(50);

    uint64_t elapsed = qsys_now_ms() - start_ms;
    LOG(INFO, "Cycle completed in %llu ms (running=%d)", (unsigned long long)elapsed, running);

    return 0;
}
```

### 4. Advisory File Locking & Safe File I/O

Safely lock files across concurrent processes and perform formatted output:

```c
#include <fcntl.h>
#include <unistd.h>
#include <ttypt/qsys.h>

int main(void) {
    int fd = open("/tmp/service.pid", O_RDWR | O_CREAT, 0644);
    if (fd < 0) return 1;

    // Acquire non-blocking exclusive lock
    if (qsys_flock(fd, QSYS_LOCK_EX | QSYS_LOCK_NB) != 0) {
        ERR("Another instance of this daemon is already running!");
        close(fd);
        return 1;
    }

    // Write process ID using portable dprintf
    qsys_ftruncate(fd, 0);
    qsys_dprintf(fd, "%d\n", getpid());
    qsys_fsync(fd);

    LOG(INFO, "Locked PID file and running...");

    // Release lock before exiting
    qsys_flock(fd, QSYS_LOCK_UN);
    close(fd);
    return 0;
}
```

---

## API Reference

### Logging & Syslog

| Symbol | Signature | Description |
| :--- | :--- | :--- |
| `LOG(TYPE, ...)` | `LOG(TYPE, fmt, ...)` | Log formatted message tagged with calling function name (`__func__`) |
| `WARN(...)` | `WARN(fmt, ...)` | Shorthand for `LOG(WARNING, ...)` |
| `ERR(...)` | `ERR(fmt, ...)` | Shorthand for `LOG(ERR, ...)` |
| `CBUG(cond, ...)` | `CBUG(cond, fmt, ...)` | If `cond` is true, logs error via `ERR()` and calls `abort()` |
| `qsyslog_set` | `void qsyslog_set(qsyslog_t logger)` | Set the global log sink callback (defaults to stderr) |
| `qsys_openlog` | `void qsys_openlog(const char *ident)` | Open system log / Windows Event Source |
| `qsys_syslog` | `void qsys_syslog(int priority, const char *fmt, ...)` | Write formatted message to system log |
| `qsys_closelog` | `void qsys_closelog(void)` | Close system log / Windows Event Source |

Severity levels: `QLOG_ERR`, `QLOG_WARNING`, `QLOG_NOTICE`, `QLOG_INFO`, `QLOG_DEBUG`.

### Sockets & Event Polling

| Function | Signature | Description |
| :--- | :--- | :--- |
| `qsys_sock_init` | `int qsys_sock_init(void)` | Initialize socket runtime (calls `WSAStartup` on Windows, no-op on POSIX) |
| `qsys_sock_create` | `qsys_socket_t qsys_sock_create(int domain, int type, int proto)` | Create a new cross-platform socket |
| `qsys_sock_close` | `int qsys_sock_close(qsys_socket_t fd)` | Close socket descriptor (`closesocket` on Windows, `close` on POSIX) |
| `qsys_sock_bind` | `int qsys_sock_bind(qsys_socket_t fd, const struct sockaddr *a, socklen_t l)` | Bind address to socket |
| `qsys_sock_listen` | `int qsys_sock_listen(qsys_socket_t fd, int backlog)` | Listen for incoming connections |
| `qsys_sock_accept` | `qsys_socket_t qsys_sock_accept(qsys_socket_t fd, struct sockaddr *a, socklen_t *l)` | Accept incoming connection |
| `qsys_sock_connect` | `int qsys_sock_connect(qsys_socket_t fd, const struct sockaddr *a, socklen_t l)` | Connect to remote address |
| `qsys_sock_recv` | `ssize_t qsys_sock_recv(qsys_socket_t fd, void *buf, size_t len, int flags)` | Receive data from socket |
| `qsys_sock_send` | `ssize_t qsys_sock_send(qsys_socket_t fd, const void *buf, size_t len, int flags)` | Send data over socket |
| `qsys_sock_set_nonblock` | `int qsys_sock_set_nonblock(qsys_socket_t fd, int nonblock)` | Set non-blocking mode (`ioctlsocket` on Windows, `O_NONBLOCK` on POSIX) |
| `qsys_sock_wouldblock` | `int qsys_sock_wouldblock(void)` | Return true if last error was `EAGAIN` / `EWOULDBLOCK` / `WSAEWOULDBLOCK` |
| `qsys_sock_strerror` | `const char *qsys_sock_strerror(void)` | Return human-readable error description for last socket error |
| `qsys_poll` | `int qsys_poll(struct pollfd *fds, nfds_t nfds, int timeout)` | Poll socket / file descriptors for I/O readiness (`WSAPoll` on Windows) |
| `qsys_ignore_pipe` | `void qsys_ignore_pipe(void)` | Ignore `SIGPIPE` so socket writes return `EPIPE` instead of aborting |
| `qsys_stop_on_signal` | `void qsys_stop_on_signal(void (*handler)(int))` | Install clean shutdown handler for `SIGINT` and `SIGTERM` |

### Process & Memory Diagnostics

| Function | Signature | Description |
| :--- | :--- | :--- |
| `qsys_pid_alive` | `int qsys_pid_alive(pid_t pid)` | Test if process is alive (returns `0` if active, `-1` if dead/inaccessible) |
| `qsys_self_rss_bytes` | `uint64_t qsys_self_rss_bytes(void)` | Query current process Resident Set Size (RSS) in bytes via `/proc`, `mach`, or PSAPI |

### Timing & Clocks

| Function | Signature | Description |
| :--- | :--- | :--- |
| `qsys_now_ms` | `uint64_t qsys_now_ms(void)` | Monotonic millisecond timestamp (`clock_gettime(CLOCK_MONOTONIC)` / `QueryPerformanceCounter`) |
| `qsys_sleep_ms` | `void qsys_sleep_ms(unsigned ms)` | Portable millisecond sleep (`nanosleep` on POSIX, `Sleep` on Windows) |

### Dynamic Library Loading

| Function / Flag | Signature | Description |
| :--- | :--- | :--- |
| `qsys_dlopen` | `void *qsys_dlopen(const char *path, int flags)` | Load dynamic library (`dlopen(RTLD_NOW\|RTLD_LOCAL)` / `LoadLibraryA`) |
| `qsys_dlsym` | `void *qsys_dlsym(void *handle, const char *symbol)` | Resolve exported symbol from library handle |
| `qsys_dlclose` | `int qsys_dlclose(void *handle)` | Unload dynamic library |
| `qsys_dlerror` | `const char *qsys_dlerror(void)` | Retrieve last dynamic loader error message |
| `QSYS_RTLD_NODELETE` | `0x01` | Flag: keep library mapped on close (`RTLD_NODELETE`, no-op on Windows) |

### Filesystem, Locking & POSIX Shims

| Function | Signature | Description |
| :--- | :--- | :--- |
| `qsys_flock` | `int qsys_flock(int fd, int op)` | Advisory file lock: `QSYS_LOCK_SH`, `QSYS_LOCK_EX`, `QSYS_LOCK_NB`, `QSYS_LOCK_UN` |
| `qsys_mkdir` | `int qsys_mkdir(const char *path)` | Create directory with default permissions (`0755` on POSIX, `_mkdir` on Windows) |
| `qsys_dprintf` | `int qsys_dprintf(int fd, const char *fmt, ...)` | Write formatted output to file descriptor |
| `qsys_ftruncate` | `int qsys_ftruncate(int fd, off_t length)` | Truncate file descriptor to specified byte length |
| `qsys_fsync` | `int qsys_fsync(int fd)` | Flush file descriptor changes to stable storage (`fsync` / `_commit`) |
| `qsys_mkstemps` | `int qsys_mkstemps(char *tmpl, int suffixlen)` | Create unique temporary file from template ending in `XXXXXX` + suffix |
| `qsys_realpath` | `char *qsys_realpath(const char *p, char *r)` | Canonicalize absolute path (`realpath` / `_fullpath`) |

### Strings, Environment & Time

| Function | Signature | Description |
| :--- | :--- | :--- |
| `qsys_strlcpy` | `size_t qsys_strlcpy(char *dst, const char *src, size_t size)` | Bounded string copy with guaranteed NUL termination |
| `qsys_gmtime_r` | `struct tm *qsys_gmtime_r(const time_t *t, struct tm *res)` | Thread-safe conversion to UTC broken-down time (`gmtime_r` / `gmtime_s`) |
| `qsys_setenv` | `int qsys_setenv(const char *name, const char *val, int overwrite)` | Set environment variable (`setenv` / `_putenv_s`) |
| `qsys_unsetenv` | `int qsys_unsetenv(const char *name)` | Remove environment variable (`unsetenv` / `_putenv_s`) |

### Compiler Macro Normalization

| Macro | GCC / Clang Implementation | MSVC Implementation | Purpose |
| :--- | :--- | :--- | :--- |
| `WEAK` | `__attribute__((weak))` / `((weak_import))` on Apple | *noop* | Allow default symbol definitions to be overridden |
| `UNUSED` | `__attribute__((unused))` | *noop* | Suppress unused variable and parameter compiler warnings |

---

## Installation & Build

### Prebuilt Packages
Prebuilt packages are distributed from tty.pt for Linux (APT / Alpine / Arch / Fedora-RHEL), macOS (Homebrew), Windows (winget / MSYS2), and OpenBSD. Follow the [installation instructions](https://github.com/tty-pt/ci/blob/main/docs/install.md) and use **libqsys** as the package name.

### Build from Source
Building `libqsys` requires a standard C99 compiler and `make`:

```sh
make                  # builds lib/libqsys.so, bin/qsys_dlopen_test, and bin/qsys_port_test
make test             # runs the full in-tree test suite
sudo make install     # installs headers, library, and qsys.pc
```

Link against `libqsys` in your project:
```sh
cc my_program.c $(pkg-config --cflags --libs qsys)
```

## License

BSD 2-Clause License. Copyright (c) 2025-2026, tty-pt. See `LICENSE` for details.
