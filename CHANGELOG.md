## [1.5.1] - 2026-11-03

- **Unified Linear Memory Substrate & Single Allocator Architecture**:
  - `@tty-pt/stele` is the sole owner and exporter of `malloc` and `free` across the shared linear memory space.
  - `loadWasmModule()` automatically injects `env.malloc`, `env.free`, and `env.memory` into all downstream WebAssembly modules with dynamic export proxies.
  - Configured `-Wl,--initial-memory=16777216 -Wl,--max-memory=268435456` in `stele.wasm` to accommodate multi-module guest allocations.
  - Added detached-buffer immune typed array views (`substrate.u8()`, `substrate.u32()`, `substrate.f32()`, `substrate.i32()`) and RAII string marshalling helpers (`withString`, `readString`).
- **Parallel Streaming Hydration with Fallback (`client.js`, `stele-client.js`)**:
  - Implemented `instantiateWasmStreamingWithFallback()` prioritizing `WebAssembly.instantiateStreaming(fetch(...))` with automatic fallback to `fetch().then(r => r.arrayBuffer()).then(b => WebAssembly.instantiate(b))` on error or non-`application/wasm` MIME types.
  - Parallelized slot mounting and substrate kernel boot via `Promise.all`.
- **Centralized Build & Binary Optimization**:
  - Delegated `build:wasm` to `make wasm` in `include.mk` with `-ffreestanding -flto -O3` and post-link `wasm-opt -O3`.

## [1.5.0]

- **Domain-Agnostic WebAssembly Substrate (`libstele`)**:
  - Decoupled `libstele` completely from external UI libraries (zero references to `libbud` or `BUD_*` identifiers).
  - Standardized generic module and slot operations (`stl_module_ops_t`, `stl_kernel_boot`, `stl_kernel_mount`, `stl_kernel_unmount`, `stl_kernel_update`, `stl_kernel_action`, `stl_kernel_dispatch_event`).
  - Added prefix-aware dynamic module matching (`stl_mod_matches`) supporting aliased and sub-module discovery (e.g. `list` matching `list_fe`, `song` matching `song_detail`).
- **Universal Isomorphic Ergonomics (`<ttypt/iso.h>`, `<ttypt/stele_wasm.h>`)**:
  - Canonized `ISO_EXPORT`, `ISO_EXPORT_AS`, and `ISO_IMPORT` in `<ttypt/iso.h>` as the canonical isomorphic export contract across native shared objects (SSR) and WebAssembly reactors (browser), deprecating `WASM_EXPORT` aliases.
  - Introduced universal state invalidation (`stl_invalidate()` and `stl_slot_invalidate(slot)`) in `<ttypt/stele_wasm.h>`: pure inline no-ops under native SSR, triggers `stl_host_invalidate` in browser WebAssembly.
- **Dynamic Component Loading & Linear Memory Architecture (`npm/dist/client.js`)**:
  - Implemented dynamic on-demand component WASM instantiation with isolated linear memory instances per component binary in `stele-client.js`.
  - Clean JS-mediated slot bridge orchestration preventing heap/data segment collisions at fixed static offsets (`--global-base=1024`).

## [1.4.0]

- **Universal Isomorphic C Contract (`<ttypt/iso.h>`)**:
  - Introduced `<ttypt/iso.h>` providing platform-independent `ISO_EXPORT`, `ISO_EXPORT_AS`, and `ISO_IMPORT` macros.
  - Symmetrical compilation across native shared libraries (`.so` / `.dylib`) with default symbol visibility and WebAssembly reactor bundles (`.wasm`) with direct `export_name` attributes.
  - Zero preprocessor conditionals required in consuming application and component source code.

## [1.3.0]

- **Consolidated socket & portability shims**: full cross-platform shims for
  sockets (`stl_sock_*`), polling (`stl_poll`), advisory locking
  (`stl_flock`), filesystem (`stl_realpath`, `stl_mkdir`), file I/O
  (`stl_ftruncate`, `stl_dprintf`), timing (`stl_now_ms`, `stl_sleep_ms`),
  process management (`stl_pid_alive`, `stl_self_rss_bytes`), and signals
  (`stl_stop_on_signal`, `stl_ignore_pipe`).
- **Windows / MinGW link fixes**: added `LDLIBS-libstele-Windows := -lws2_32 -lpsapi`
  to support Win32 socket runtime (`ws2_32`) and process RSS queries (`psapi`).
- Added comprehensive unit tests in `src/stele_port_test.c`.

## [1.2.0]

- **API Documentation**: every public function, macro, and type
  declaration in `include/ttypt/stele.h` is now documented Doxygen-style.
- Added comprehensive unit tests in `src/stele_dlopen_test.c`.

## [1.1.0]

- **Portable dynamic-library loading**: `stl_dlopen` / `stl_dlsym` / `stl_dlclose` / `stl_dlerror` — thin POSIX `dlopen`/`dlsym`/`dlclose`/`dlerror` wrappers on Unix, `LoadLibraryA`/`GetProcAddress`/`FreeLibrary`/`GetLastError`+`FormatMessageA` on Windows. Always resolves eagerly and locally (`RTLD_NOW|RTLD_LOCAL`); `STL_RTLD_NODELETE` flag (no-op on Windows). No suffix-appending or path canonicalization.
- **Portable POSIX-shim helpers**: `stl_mkstemps`, `stl_fsync`, `stl_setenv`/`stl_unsetenv`, `stl_strlcpy`, `stl_gmtime_r` — native thin wrappers on Unix, emulated on Windows.
- New `stele_dlopen_test` binary (`make test`) covering the loader API.
