<p align="center">
  <img src="https://raw.githubusercontent.com/tty-pt/libqsys/main/logo.png" alt="@ttypt/qsys logo" width="220" />
</p>

# @ttypt/qsys

[![npm version](https://img.shields.io/badge/npm-v0.1.0-CB3837?logo=npm)](#)
[![License: BSD-2-Clause](https://img.shields.io/badge/License-BSD--2--Clause-blue)](#)
[![TypeScript Ready](https://img.shields.io/badge/TypeScript-Ready-3178C6?logo=typescript)](#)
[![Node.js Native](https://img.shields.io/badge/Node.js-N--API-339933?logo=node.js)](#)
[![WebAssembly](https://img.shields.io/badge/WebAssembly-Reactor-654FF0?logo=webassembly)](#)
[![Zero Dependencies](https://img.shields.io/badge/dependencies-0-success)](#)

> Universal Node.js and Browser bindings for **`libqsys`**, the zero-dependency system substrate for plain C and isomorphic WebAssembly. Provides high-resolution monotonic clocks, memory diagnostics, and the decoupled `CapabilityRegistry` plug-and-play contract.

---

## 1. Quickstart

```bash
npm install @ttypt/qsys
```

```typescript
import { nowMs, selfRssBytes, CapabilityRegistry } from "@ttypt/qsys";

// 1. High-resolution monotonic timestamp (milliseconds)
const start = nowMs();

// 2. Memory diagnostics
const rss = selfRssBytes();
console.log(`Resident memory: ${(rss / 1024 / 1024).toFixed(2)} MB`);

// 3. Decoupled Plug-and-Play Capability Registry
const registry = new CapabilityRegistry();
registry.register("temporal", myTemporalModule);
registry.register("spatial", mySpatialModule);

const temporal = registry.get("temporal");
```

---

## 2. Universal Isomorphic Contract (`<ttypt/iso.h>`)

`libqsys` defines the standard isomorphic C capability contract used across the entire `tty.pt` stack:
- `ISO_EXPORT(ret, name, ...)`
- `ISO_EXPORT_AS(ret, c_name, wasm_name, ...)`
- `ISO_IMPORT(ret, module_name, name, ...)`

---

## 3. C Development Headers via NPM

```makefile
CFLAGS += -I./node_modules/@ttypt/qsys/include
LDLIBS += -lqsys
```

---

## 4. Building from Source

```bash
make npm-build    # Compiles native addon and WASM reactor
make npm-test     # Runs verification test suite
```

## License

BSD 2-Clause License. Copyright (c) 2026, tty-pt.
