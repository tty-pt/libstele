<div align="center">
  <img src="https://raw.githubusercontent.com/tty-pt/libstele/main/logo.png" alt="@tty-pt/stele logo" width="220" />
</div>

# @tty-pt/stele

[![npm version](https://img.shields.io/npm/v/@tty-pt/stele.svg)](https://www.npmjs.com/package/@tty-pt/stele)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue.svg)](../LICENSE)
[![Standards: WebAssembly Reactor](https://img.shields.io/badge/WASM-Reactor--Substrate-orange.svg)](#)

> Universal Node.js and Browser bindings for **`libstele`**, the zero-dependency system substrate for plain C and isomorphic WebAssembly. Provides high-resolution monotonic clocks, memory diagnostics, and the decoupled `CapabilityRegistry` plug-and-play contract.

---

## 1. Quick Start

Install via npm:

```bash
npm install @tty-pt/stele
```

Usage in modern JavaScript / TypeScript:

```javascript
import {
  nowMs,
  selfRssBytes,
  CapabilityRegistry,
  WasmSubstrate,
  getSubstrate,
  getSharedWasm
} from "@tty-pt/stele";

// 1. High-Resolution Metrics
console.log(`Current process monotonic time: ${nowMs()} ms`);
console.log(`Resident Set Size (RSS): ${selfRssBytes()} bytes`);

// 2. Capability Registry
const caps = new CapabilityRegistry();
caps.register("audio", (root, wasm) => ({
  play: (url) => console.log("Playing audio:", url)
}));

// 3. WebAssembly Substrate
const substrate = getSubstrate();
console.log("Single-memory substrate initialized:", substrate.memory.buffer.byteLength);
```

---

## 2. JavaScript / TypeScript API Reference

### `nowMs(): number`
Returns a high-resolution monotonic timestamp in milliseconds using the fastest available native clock.

### `selfRssBytes(): number`
Returns the process Resident Set Size (RSS) in bytes.

### `CapabilityRegistry`
Decoupled plug-and-play capability registry mapping feature slots to runtime providers.
* `.register(name: string, provider: Function): this`
* `.get(name: string): Function | null`
* `.has(name: string): boolean`
* `.list(): string[]`
* `.unregister(name: string): boolean`

### `WasmSubstrate`
Universal single-memory WebAssembly runtime substrate.
* `instance: WebAssembly.Instance`: Underlying compiled WASM instance.
* `memory: WebAssembly.Memory`: Shared linear memory space across all colocated modules.
* `.malloc(size: number): number`: Allocates memory within the substrate.
* `.free(ptr: number): void`: Frees memory within the substrate.
* `.encodeString(str: string): [number, number]`: Writes UTF-8 string into WASM memory, returning `[ptr, byteLength]`.
* `.kernelBoot(): number`: Invokes `stl_kernel_boot()`.
* `.kernelMount(slot: number, name: string, json?: string): number`: Mounts module into slot.
* `.kernelAction(slot: number, actionId: number, data?: string): number`: Dispatches action to module.

### `getSubstrate(): WasmSubstrate`
Returns the globally shared `WasmSubstrate` instance, guaranteed to instantiate `stele.wasm` strictly once per lifecycle.

### `getSharedWasm(): WebAssembly.Exports`
Direct accessor returning the raw exported functions and linear memory of the shared `stele.wasm` kernel.

---

## 3. Isomorphic C Header Packaging

`libstele` defines the standard isomorphic C capability contract used across the entire `tty.pt` stack:

- `<ttypt/stele.h>`: Cross-platform POSIX/Win32 socket and system shims.
- `<ttypt/iso.h>`: Universal `ISO_EXPORT` and `ISO_IMPORT` macros for isomorphic C modules compiled natively for SSR and into `.wasm` for the browser.
- `<ttypt/stele_wasm.h>`: Universal single-instantiation WASM kernel substrate and slot orchestration ABI.

When building native Node addons or WebAssembly reactors:

```makefile
CFLAGS += -I./node_modules/@tty-pt/stele/include
LDLIBS += -lstele
```
