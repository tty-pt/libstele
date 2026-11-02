<p align="center">
  <img src="logo.png" alt="libstele logo" width="220" />
</p>

# libstele (libqsys)

[![npm](https://img.shields.io/badge/npm-@ttypt/qsys-CB3837?logo=npm)](npm/README.md)
[![C99](https://img.shields.io/badge/C-C99-555?logo=c)](#)
[![BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](#)
[![Dependencies](https://img.shields.io/badge/dependencies-zero-success)](#)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20BSDs-blueviolet)](#)

> Zero-dependency system substrate for plain C: cross-platform sockets, event polling, pluggable logging, process diagnostics, monotonic timing, advisory file locking, and isomorphic C contracts. Canonical headers are `<ttypt/qsys.h>` and `<ttypt/iso.h>`.

---

## 1. Quickstart

### Cross-Platform Non-Blocking Sockets & Polling
Set up a non-blocking TCP server and poll for connections without writing a single `#ifdef _WIN32`:

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
    qsys_ignore_pipe(); // Ignore SIGPIPE on client disconnects

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

    struct pollfd pfd = { .fd = server, .events = POLLIN };
    if (qsys_poll(&pfd, 1, 5000) > 0 && (pfd.revents & POLLIN)) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        qsys_socket_t client = qsys_sock_accept(server, (struct sockaddr *)&client_addr, &len);
        if (client >= 0) {
            LOG(INFO, "Client connected!");
            qsys_sock_send(client, "HELLO\n", 6, 0);
            qsys_sock_close(client);
        }
    }
    qsys_sock_close(server);
    return 0;
}
```

Compile and link:
```sh
cc server.c $(pkg-config --cflags --libs qsys) -o server
```

### Isomorphic C Export (`<ttypt/iso.h>`)
Write C modules that compile symmetrically into native `.so` libraries and browser `.wasm` binaries:

```c
#include <ttypt/iso.h>

ISO_EXPORT int calculate_sum(int a, int b) {
    return a + b;
}
```

---

## 2. Core Capabilities

| Subsystem | Primary Header | Key Functions & Macros |
| :--- | :--- | :--- |
| **Logging & Assertions** | `<ttypt/qsys.h>` | `LOG()`, `WARN()`, `ERR()`, `CBUG(cond, ...)`, `qsyslog_set()`, `qsys_syslog()` |
| **Sockets & Multiplexing** | `<ttypt/qsys.h>` | `qsys_sock_create()`, `qsys_sock_set_nonblock()`, `qsys_poll()`, `qsys_ignore_pipe()` |
| **High-Res Timing & Diagnostics** | `<ttypt/qsys.h>` | `qsys_now_ms()`, `qsys_sleep_ms()`, `qsys_pid_alive()`, `qsys_self_rss_bytes()` |
| **Advisory File Locking** | `<ttypt/qsys.h>` | `qsys_flock(fd, QSYS_LOCK_EX \| QSYS_LOCK_NB)`, `qsys_dprintf()`, `qsys_ftruncate()` |
| **Dynamic Loading** | `<ttypt/qsys.h>` | `qsys_dlopen()`, `qsys_dlsym()`, `qsys_dlclose()` with `QSYS_RTLD_NODELETE` |
| **Isomorphic C Standard** | `<ttypt/iso.h>` | `ISO_EXPORT`, `ISO_EXPORT_AS()`, `ISO_IMPORT` for dual `.so`/`.wasm` compilation |

---

## 3. Core API Reference

| Function / Macro | Signature | Description |
| :--- | :--- | :--- |
| `LOG(LEVEL, fmt, ...)` | `LOG(TYPE, fmt, ...)` | Tagged logging with caller function name (`__func__`) |
| `CBUG(cond, fmt, ...)` | `CBUG(cond, fmt, ...)` | Logs error and calls `abort()` if `cond` is true |
| `qsys_sock_init` | `int qsys_sock_init(void)` | Initialize network subsystem (Winsock on Win32, no-op on POSIX) |
| `qsys_sock_set_nonblock` | `int qsys_sock_set_nonblock(sock, nonblock)` | Set non-blocking mode portably |
| `qsys_poll` | `int qsys_poll(fds, nfds, timeout_ms)` | Event multiplexing (`poll` on POSIX, `WSAPoll` on Windows) |
| `qsys_now_ms` | `uint64_t qsys_now_ms(void)` | Monotonic timestamp in milliseconds |
| `qsys_self_rss_bytes` | `size_t qsys_self_rss_bytes(void)` | Process resident memory usage in bytes |
| `qsys_flock` | `int qsys_flock(fd, operation)` | Cross-platform advisory file locking (`QSYS_LOCK_SH/EX/NB/UN`) |
| `qsys_dprintf` | `int qsys_dprintf(fd, fmt, ...)` | Formatted printing directly to file descriptor |
| `qsys_stop_on_signal` | `void qsys_stop_on_signal(volatile int *flag)` | Register SIGINT/SIGTERM handlers to set flag to 1 |

---

## 4. Installation & Build

### Prebuilt Packages
Prebuilt packages for Linux (APT / Alpine / Arch / Fedora-RHEL), macOS (Homebrew), Windows (winget / MSYS2), and OpenBSD are distributed via [tty.pt packages](https://github.com/tty-pt/ci/blob/main/docs/install.md). Use **qsys** as the package name.

### Build from Source
```sh
make                  # builds lib/libqsys.so
make test             # run comprehensive cross-platform test suite
sudo make install     # install headers and library to system
```

---

## 5. JavaScript & WebAssembly Bindings (@ttypt/qsys)

The system substrate and isomorphic C runtime contracts are colocated in [`npm/`](npm/README.md):
- **Universal Substrate**: Monotonic timestamps, memory RSS diagnostics, and the plug-and-play `CapabilityRegistry` contract.
- **Isomorphic C ABI**: `<ttypt/iso.h>` defines universal export and import attributes for dual C shared object and WebAssembly reactor builds.
- **C Development Headers**: Install via `npm install @ttypt/qsys` and consume headers from `node_modules/@ttypt/qsys/include`.

👉 *For complete TypeScript usage, capability guides, and API reference, see [npm/README.md](npm/README.md).*

## License

BSD 2-Clause License. Copyright (c) 2025, tty-pt. See `LICENSE`.
