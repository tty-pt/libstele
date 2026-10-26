## [1.4.0]

<p align="center">
  <img src="https://raw.githubusercontent.com/tty-pt/libqsys/main/logo.png" alt="libqsys logo" width="220" />
</p>

- **Universal Isomorphic C Contract (`<ttypt/iso.h>`)**:
  - Introduced `<ttypt/iso.h>` providing platform-independent `ISO_EXPORT`, `ISO_EXPORT_AS`, and `ISO_IMPORT` macros.
  - Symmetrical compilation across native shared libraries (`.so` / `.dylib`) with default symbol visibility and WebAssembly reactor bundles (`.wasm`) with direct `export_name` attributes.
  - Zero preprocessor conditionals required in consuming application and component source code.

## 1.3.0

- **Consolidated socket & portability shims**: full cross-platform shims for
  sockets (`qsys_sock_*`), polling (`qsys_poll`), advisory locking
  (`qsys_flock`), filesystem (`qsys_realpath`, `qsys_mkdir`), file I/O
  (`qsys_ftruncate`, `qsys_dprintf`), timing (`qsys_now_ms`, `qsys_sleep_ms`),
  process management (`qsys_pid_alive`, `qsys_self_rss_bytes`), and signals
  (`qsys_stop_on_signal`, `qsys_ignore_pipe`).
- **Windows / MinGW link fixes**: added `LDLIBS-libqsys-Windows := -lws2_32 -lpsapi`
  for Win32 socket and PSAPI symbols.
- Added comprehensive unit tests in `src/qsys_port_test.c`.

## 1.2.2

- **Documentation-only release: no source or header behaviour changed.** Every
  declaration in `include/ttypt/qsys.h` is now documented Doxygen-style —
  parameters, return conventions and the Windows/Unix split stated per symbol
  (the old inline trailing comments on `qsys_dlopen`/`qsys_dlsym`/
  `qsys_dlclose`/`qsys_dlerror` became full blocks; prototypes are untouched).
- **README rewritten around the actual API surface**: what the library is for,
  the portable loader and POSIX-shim helpers with their platform caveats, and
  how to build and test it.
- `Doxyfile` and `.gitignore` picked up the generated docs; `CHANGELOG.md`
  opened with the 1.2.1 entry.

## [1.2.1]

- **Portable dynamic-library loading**: `qsys_dlopen` / `qsys_dlsym` / `qsys_dlclose` / `qsys_dlerror` — thin POSIX `dlopen`/`dlsym`/`dlclose`/`dlerror` wrappers on Unix, `LoadLibraryA`/`GetProcAddress`/`FreeLibrary`/`GetLastError`+`FormatMessageA` on Windows. Always resolves eagerly and locally (`RTLD_NOW|RTLD_LOCAL`); `QSYS_RTLD_NODELETE` flag (no-op on Windows). No suffix-appending or path canonicalization.
- **Portable POSIX-shim helpers**: `qsys_mkstemps`, `qsys_fsync`, `qsys_setenv`/`qsys_unsetenv`, `qsys_strlcpy`, `qsys_gmtime_r` — native thin wrappers on Unix, emulated on Windows.
- New `qsys_dlopen_test` binary (`make test`) covering the loader API.

## [0.1.0] - 2025-10-19
- Windows compatibility
- Change release strategy
- Headers in ttypt folder

## [0.0.11] - 2025-10-02
- System Logging
- Debug Macros
- Error Logging
