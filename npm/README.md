<div align="center">
  <img src="https://raw.githubusercontent.com/tty-pt/libstele/main/logo.png" alt="@tty-pt/stele logo" width="220" />
</div>

# @tty-pt/stele

[![npm version](https://img.shields.io/npm/v/@tty-pt/stele.svg)](https://www.npmjs.com/package/@tty-pt/stele)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue.svg)](LICENSE)
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
import { nowMs, selfRssBytes, CapabilityRegistry } from "@tty-pt/stele";

console.log(`Current process monotonic time: ${nowMs()} ms`);
console.log(`Resident Set Size (RSS): ${selfRssBytes()} bytes`);

const caps = new CapabilityRegistry();
caps.register("audio", (root, wasm) => ({
  play: (url) => console.log("Playing audio:", url)
}));
```

---

## 2. Isomorphic C Header Packaging

`libstele` defines the standard isomorphic C capability contract used across the entire `tty.pt` stack:

- `<ttypt/stele.h>`: Cross-platform POSIX/Win32 socket and system shims.
- `<ttypt/iso.h>`: Universal `ISO_EXPORT` and `ISO_IMPORT` macros for isomorphic C modules compiled natively for SSR and into `.wasm` for the browser.
- `<ttypt/stele_wasm.h>`: Universal single-instantiation WASM kernel substrate and slot orchestration ABI.

When building native Node addons or WebAssembly reactors:

```makefile
CFLAGS += -I./node_modules/@tty-pt/stele/include
LDLIBS += -lstele
```
