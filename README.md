# libqsys

[![C99](https://img.shields.io/badge/C-C99-555?logo=c)](#)
[![BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](#)
[![Sys utilities](https://img.shields.io/badge/sys-utilities-6B7280)](#)

> A small library for system utilities like logging and useful macros.

Cross-platform helpers for logging, compiler attribute macros, and dynamic
loading — the common substrate the rest of the tty.pt stack builds on.

## Contents

- [Features](#features)
- [Install](#install)
- [Build from source](#build-from-source)
- [Quickstart](#quickstart)
- [API overview](#api-overview)
- [Documentation](#documentation)
- [Testing](#testing)
- [License](#license)

## Features

- **Logging macros** — `LOG(TYPE, ...)`, `WARN(...)`, `ERR(...)`, `CBUG(...)`
  with a pluggable `qsyslog_t` sink.
- **Compiler attribute helpers** — `WEAK`, `UNUSED` and friends, ported across
  toolchains.
- **Dynamic loading** — dlopen helpers with `QSYS_RTLD_NODELETE` (no-op on
  Windows) for module loading.
- **Shared typedefs** — `qsys.h` carries the common `qsyslog_t` callback and
  the platform veneer used by sibling libraries.

## Install

Prebuilt packages are distributed from tty.pt for Linux (APT / Alpine / Arch /
Fedora-RHEL), macOS (Homebrew), Windows (winget / MSYS2), and OpenBSD. Follow
the [installation instructions](https://github.com/tty-pt/ci/blob/main/docs/install.md)
and use **libqsys** as the package name.

## Build from source

The library builds with a plain `make` (the shared [`mk` include.mk](https://github.com/tty-pt/mk)):

```sh
make                  # builds lib/libqsys.so + bin/qsys_dlopen_test
make test             # run the in-tree test suite
sudo make install     # lib + headers + qsys.pc → $(PREFIX), default /usr/local
```

Link it from your own C code:

```sh
cc my_app.c $(pkg-config --cflags --libs qsys)
```

**Dependencies:** none beyond libc / POSIX (`-ldl` for dynamic loading).

## Quickstart

```c
#include <ttypt/qsys.h>

int main(void)
{
    WARN("welcome from libqsys");
    return 0;
}
```

## API overview

The complete surface lives in `include/ttypt/qsys.h`: the `LOG`/`WARN`/`ERR`/
`CBUG` macro family, `WEAK`/`UNUSED` attribute macros, the `qsyslog_t`
callback typedef, and the `QSYS_RTLD_NODELETE` flag for POSIX module loading.

## Documentation

The header `include/ttypt/qsys.h` is the API contract; the man pages shipped
by the package cover the logging and loading entry points.

## Testing

```sh
make test     # builds and runs bin/qsys_dlopen_test
```

## License

BSD 2-Clause License. Copyright (c) 2025, tty-pt. See `LICENSE`.