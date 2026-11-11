## [Unreleased] (target: v1.6.0)

### Added
- **Zero-Reload SPA Navigation & Warm-Memory Slot Swapping (`client.js`, `stele-client.js`)**:
  - Implemented seamless client-side SPA route transitions with warm-memory preservation.
  - Added event bus implementation (`src/stele_event.c`) for cross-module decoupled messaging.
  - Added known capability bridges mapping to eliminate redundant 404 network lookups.
  - Active capability bridge synchronization (`window.__active_bridges`) and batched microtask remounting (`_scheduleBatchRemount`) across patch replay boundaries and slot updates.
  - Standardized client interface definitions in `include/ttypt/stele_wasm.h`.

### Changed
- Refactored WASM kernel integration (`src/stele_wasm_kernel.c`) and updated allowed import lists.

## [1.5.2] - 2026-11-03

- **Universal System Substrate & Single Shared Allocator**:
  - `@tty-pt/stele` is the sole owner and exporter of `malloc` and `free` across the shared linear memory space.
  - `loadWasmModule()` injects `env.malloc`, `env.free`, and `env.memory` into all downstream WebAssembly modules.
  - Standardized `loadNativeAddon` for symmetrical native Node-API and WebAssembly dual dispatch across Node and browser runtimes.
  - Configured 16MB initial / 256MB max memory boundaries in `stele.wasm` to accommodate multi-module guest allocations.
  - Added detached-buffer immune typed array views (`substrate.u8()`, `substrate.u32()`, `substrate.f32()`, `substrate.i32()`) and RAII string marshalling helpers (`withString`, `readString`).
- **Parallel Streaming Hydration with Fallback (`client.js`, `stele-client.js`)**:
  - Implemented `instantiateWasmStreamingWithFallback()` prioritizing `WebAssembly.instantiateStreaming(fetch(...))` with automatic fallback to `fetch().then(r => r.arrayBuffer()).then(b => WebAssembly.instantiate(b))` on error or non-`application/wasm` MIME types.
  - Parallelized slot mounting and substrate kernel boot via `Promise.all`.
- **Domain-Agnostic WebAssembly Substrate & Isomorphic Ergonomics (`<ttypt/iso.h>`, `<ttypt/stele_wasm.h>`)**:
  - Decoupled `libstele` completely from external UI libraries; standardized generic slot operations (`stl_module_ops_t`, `stl_kernel_*`).
  - Canonized `ISO_EXPORT`, `ISO_EXPORT_AS`, and `ISO_IMPORT` in `<ttypt/iso.h>` as the canonical isomorphic export contract across native shared objects (SSR) and WebAssembly reactors (browser).
  - Centralized WASM compilation in `external/mk/include.mk` (`make wasm`) with post-link `wasm-opt -O3` optimization.

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
