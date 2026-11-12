<p align="center">
  <img src="assets/logo.png" alt="libstele logo" width="220" />
</p>

# libstele

[![npm](https://img.shields.io/badge/npm-@tty--pt/stele-CB3837?logo=npm)](npm/README.md)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue.svg)](LICENSE)
[![Standards: ISO C99 / POSIX.1-2008](https://img.shields.io/badge/Standards-C99%20%7C%20POSIX.1--2008-00599C.svg)](#)
[![Zero-Dependency](https://img.shields.io/badge/Dependencies-Zero-success.svg)](#)

> Zero-dependency system substrate for plain C: cross-platform sockets, event polling, pluggable logging, process diagnostics, monotonic timing, advisory file locking, and isomorphic C contracts. Canonical headers are `<ttypt/stele.h>` and `<ttypt/iso.h>`.

---

## 1. Quick Example: Cross-Platform Echo Server

```c
#include <stdio.h>
#include <string.h>
#include <ttypt/stele.h>

int main(void) {
    if (stl_sock_init() != 0) {
        ERR("Failed to initialize socket runtime: %s", stl_sock_strerror());
        return 1;
    }

    stl_ignore_pipe(); // Ignore SIGPIPE on client disconnects

    stl_socket_t server = stl_sock_create(AF_INET, SOCK_STREAM, 0);
    stl_sock_set_nonblock(server, 1);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    stl_sock_bind(server, (struct sockaddr *)&addr, sizeof(addr));
    stl_sock_listen(server, 128);
    printf("Echo server listening on port 8080...\n");

    struct pollfd pfd = { .fd = server, .events = POLLIN };
    if (stl_poll(&pfd, 1, 5000) > 0 && (pfd.revents & POLLIN)) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        stl_socket_t client = stl_sock_accept(server, (struct sockaddr *)&client_addr, &len);
        if (client >= 0) {
            char buf[128];
            stl_sock_send(client, "HELLO\n", 6, 0);
            stl_sock_close(client);
        }
    }

    stl_sock_close(server);
    return 0;
}
```

Compile natively on any platform:

```bash
cc server.c $(pkg-config --cflags --libs stele) -o server
```

---

## 2. Capabilities & Architecture

`libstele` serves as the bedrock Layer 0 substrate across the `tty.pt` C platform ecosystem.

| Subsystem | Header | Key Functions & Macros |
| :--- | :--- | :--- |
| **Logging & Assertions** | `<ttypt/stele.h>` | `LOG()`, `WARN()`, `ERR()`, `CBUG(cond, ...)`, `stllog_set()`, `stl_syslog()` |
| **Sockets & Multiplexing** | `<ttypt/stele.h>` | `stl_sock_create()`, `stl_sock_set_nonblock()`, `stl_poll()`, `stl_ignore_pipe()` |
| **High-Res Timing & Diagnostics** | `<ttypt/stele.h>` | `stl_now_ms()`, `stl_sleep_ms()`, `stl_pid_alive()`, `stl_self_rss_bytes()` |
| **Advisory File Locking** | `<ttypt/stele.h>` | `stl_flock(fd, STL_LOCK_EX \| STL_LOCK_NB)`, `stl_dprintf()`, `stl_ftruncate()` |
| **Dynamic Loading** | `<ttypt/stele.h>` | `stl_dlopen()`, `stl_dlsym()`, `stl_dlclose()` with `STL_RTLD_NODELETE` |
| **Isomorphic C Export ABI** | `<ttypt/iso.h>` | `ISO_EXPORT()`, `ISO_EXPORT_AS()`, `ISO_IMPORT()` |
| **WASM Substrate & Slots** | `<ttypt/stele_wasm.h>` | `stl_kernel_boot()`, `stl_kernel_mount()`, `stl_kernel_mount_bin()`, `stl_invalidate()`, `stl_slot_invalidate()`, `STL_WASM_MODULE()`, `STL_WASM_MODULE_BIN()`, `STL_WASM_MODULE_EXT()` |
| **Cross-Slot Event Bus** | `<ttypt/stele_wasm.h>` | `stl_event_on()`, `stl_event_off()`, `stl_event_emit()`, `stl_on_event()` |

---

## 3. Installation & Build

### Prebuilt Packages
Prebuilt packages for Linux (APT / Alpine / Arch / Fedora-RHEL), macOS (Homebrew), Windows (winget / MSYS2), and OpenBSD are distributed via [tty.pt packages](https://github.com/tty-pt/ci/blob/main/docs/install.md). Use **libstele** as the package name.

### Build from Source
```sh
make                  # builds lib/libstele.so
make test             # runs portability & dlopen test suite
sudo make install     # installs to /usr/local or /usr
```

---

## 4. JavaScript & WebAssembly Bindings (`@tty-pt/stele`)

For browser, Node.js, and edge runtimes, `libstele` ships an isomorphic WebAssembly kernel and slot transport runtime on npm:

```sh
npm install @tty-pt/stele
```

See [npm/README.md](npm/README.md) for the full TypeScript and WebAssembly API reference.

---

## License

BSD 2-Clause License. Copyright (c) 2026, Paulo André Azevedo Quirino. See `LICENSE`.
