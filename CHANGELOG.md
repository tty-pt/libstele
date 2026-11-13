## [1.6.1] - 2026-11-13

### Changed
- **Unbounded 4 GB Shared Linear Memory & 2 MB Global Base (`Makefile`, `stele.wasm`, `client.js`, `npm/dist/index.js`)**:
  - Removed the 256 MB (`max=4096` pages) ceiling on `WebAssembly.Memory` so linear memory starts at 16 MB (`initial=256`) and expands on demand up to the full `wasm32` 4 GB limit for in-browser LLM weights, KV caches, and recall indices.
  - Placed `stele.wasm` at `--global-base=2097152` (2 MB), reserving `[0 .. 2 MB)` for downstream reactor static data and stack bands while `stele`'s `dlmalloc` heap grows upward from `~2.07 MB` to 4 GB without colliding with guest modules.
- **Full Single-Allocator Exports (`calloc` & `realloc`)**:
  - Exported `calloc` and `realloc` alongside `malloc` and `free` from `stele.wasm` and `WasmSubstrate`, injecting all four into `env` in `loadWasmModule` / `loadWasmModuleAsync` so downstream C modules linked with `--wrap` share a single heap.

## [1.6.0] - 2026-11-12

### Added
- **In-WASM & Cross-Slot Event Bus (`src/stele_event.c`, `<ttypt/stele_wasm.h>`)**:
  - Added `stl_event_on()`, `stl_event_off()`, `stl_event_emit()`, and `stl_on_event()` (with `stele_*` aliases) to `libstele.so` and `stele.wasm` for decoupled in-instance and cross-slot messaging.
  - Added `stl_host_event_emit` host import and `SteleWasmClient` event bus methods (`on`, `off`, `emit`) bridging WASM slots and JavaScript listeners.
- **Zero-Copy Binary State Hydration (`src/stele_wasm_kernel.c`, `<ttypt/stele_wasm.h>`, `client.js`)**:
  - Added `init_bin` hook to `stl_module_ops_t`, `STL_WASM_MODULE_BIN` registration macro, and `stl_kernel_mount_bin()` / `stele_kernel_mount_bin()` kernel exports.
  - Added binary state extraction in `SteleWasmClient` supporting `application/octet-stream`, `data-encoding="base64"`, `data-state-bin`, and `meta[name="state-src"]` / `meta[name="page-state-src"]` preloads.
- **On-Demand Lazy WASM Slot Mounting (`client.js`, `stele-client.js`)**:
  - Added `lazyMountSlot()` and declarative `[data-lazy-module]` / `[data-lazy-trigger]` hydration supporting `interaction`, `visible` (`IntersectionObserver`), and `manual` triggers.
- **Optimistic Form Mutations & Automatic Rollback (`client.js`, `stele-client.js`)**:
  - Added GET filter form delegation and POST optimistic mutation handling (`stl_optimistic_update` / `stl_optimistic_rollback`) with `stl:mutate`, `stl:saved`, and `stl:rollback` lifecycle events.
- **Zero-Reload SPA Navigation & Batched Capability Remounting (`client.js`, `stele-client.js`)**:
  - Updated `stl_host_set_location` to use `history.pushState` / `replaceState` for same-origin route transitions while preserving warm WASM memory.
  - Added active capability bridge synchronization (`window.__active_bridges`) and microtask-batched remounting (`_scheduleBatchRemount`) across DOM patch replay boundaries and slot updates.
  - Exported `SlotBridge` and `SlotBridgeFactory` TypeScript interfaces and `./client` package subpath in `@tty-pt/stele`.

### Changed
- **Strict Substrate Encapsulation & Packaging Parity**:
  - Standardized capability script resolution in `stele-client.js` onto the unified `/<thing>-cap.js` convention (`/<cap>-cap.js`, `/<prefix>-cap.js`) with `data-capability-*` / `capabilityUrls` overrides, removing hardcoded downstream library maps.
  - Removed project-specific addon path references from `loadNativeAddon()` and restored `npm/native/stele_napi.c` for clean Node-API addon compilation.
  - Synchronized `include/ttypt/*.h` with `npm/include/ttypt/*.h` and `htdocs/stele-client.js` with `npm/dist/client.js` during `make npm-build`.
  - Added `wasm-allowed-imports.lst` declaring sanctioned `stele.*` and `clipboard.*` host imports.

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
