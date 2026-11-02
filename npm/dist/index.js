import { createRequire } from "node:module";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = typeof import.meta !== "undefined" && import.meta.url
  ? path.dirname(fileURLToPath(import.meta.url))
  : "";

let nativeBinding = null;
try {
  const req = createRequire(import.meta.url);
  try {
    nativeBinding = req("../build/Release/qsys_node.node");
  } catch {}
  if (!nativeBinding) {
    try {
      nativeBinding = req(path.join(process.cwd(), "build/Release/qsys_node.node"));
    } catch {}
  }
} catch {
  // In-memory / browser fallback
}

/**
 * Get high-resolution monotonic timestamp in milliseconds
 */
export function nowMs() {
  if (nativeBinding && nativeBinding.nowMs) {
    return Number(nativeBinding.nowMs());
  }
  if (typeof performance !== "undefined" && performance.now) {
    return Math.floor(performance.now());
  }
  return Date.now();
}

/**
 * Get current process resident set size (RSS) in bytes
 */
export function selfRssBytes() {
  if (nativeBinding && nativeBinding.selfRssBytes) {
    return Number(nativeBinding.selfRssBytes());
  }
  if (typeof process !== "undefined" && process.memoryUsage) {
    return process.memoryUsage().rss;
  }
  return 0;
}

/**
 * Generic Plug-and-Play Capability Registry
 */
export class CapabilityRegistry {
  constructor() {
    this._capabilities = new Map();
  }

  register(name, provider) {
    this._capabilities.set(name, provider);
    return this;
  }

  get(name) {
    return this._capabilities.get(name) || null;
  }

  has(name) {
    return this._capabilities.has(name);
  }

  list() {
    return Array.from(this._capabilities.keys());
  }

  unregister(name) {
    return this._capabilities.delete(name);
  }
}

/**
 * WasmSubstrate: Universal single-memory runtime substrate for pluggable modules
 */
export class WasmSubstrate {
  constructor(instance) {
    this.instance = instance;
    this.memory = instance.exports.memory || new WebAssembly.Memory({ initial: 256, maximum: 4096 });
    this._allocOffset = 65536; // Start heap above initial page
    this._modules = new Map();
  }

  malloc(size) {
    if (this.instance.exports.malloc) {
      return this.instance.exports.malloc(size);
    }
    const aligned = (size + 7) & ~7;
    const ptr = this._allocOffset;
    this._allocOffset += aligned;
    return ptr;
  }

  free(ptr) {
    if (this.instance.exports.free) {
      this.instance.exports.free(ptr);
    }
  }

  encodeString(str) {
    const bytes = Buffer.from(str, "utf8");
    const ptr = this.malloc(bytes.length + 1);
    const view = new Uint8Array(this.memory.buffer, ptr, bytes.length + 1);
    view.set(bytes);
    view[bytes.length] = 0;
    return [ptr, bytes.length];
  }

  kernelBoot() {
    if (this.instance.exports.qsys_kernel_boot) {
      return this.instance.exports.qsys_kernel_boot();
    }
    return 0;
  }

  kernelMount(slot, name, json = "{}") {
    if (!this.instance.exports.qsys_kernel_mount) return -1;
    const [namePtr] = this.encodeString(name);
    const [jsonPtr, jsonLen] = this.encodeString(json);
    const res = this.instance.exports.qsys_kernel_mount(slot, namePtr, jsonPtr, jsonLen);
    this.free(namePtr);
    this.free(jsonPtr);
    return res;
  }

  kernelAction(slot, actionId, data = "") {
    if (!this.instance.exports.qsys_kernel_action) return -1;
    const [dataPtr] = this.encodeString(data);
    const res = this.instance.exports.qsys_kernel_action(slot, actionId, dataPtr);
    this.free(dataPtr);
    return res;
  }

  kernelUnmount(slot) {
    if (this.instance.exports.qsys_kernel_unmount) {
      return this.instance.exports.qsys_kernel_unmount(slot);
    }
    return 0;
  }

  kernelVersion() {
    if (this.instance.exports.qsys_kernel_version) {
      return Number(this.instance.exports.qsys_kernel_version());
    }
    return 1;
  }

  registerModule(name, api) {
    this._modules.set(name, api);
    return this;
  }

  getModule(name) {
    return this._modules.get(name) || null;
  }

  hasModule(name) {
    return this._modules.has(name);
  }

  listModules() {
    return Array.from(this._modules.keys());
  }
}

export function getDefaultImports() {
  return {
    bud: {
      bud_host_emit_patch: () => {},
      bud_host_log: () => {},
      bud_host_fetch: () => {},
      bud_host_set_location: () => {},
      bud_host_mark_dirty: () => {},
      bud_host_flush: () => {},
    },
    qsys: {
      qsys_host_emit_patch: () => {},
      qsys_host_log: () => {},
      qsys_host_fetch: () => {},
      qsys_host_set_location: () => {},
    },
    audio: {
      qsys_audio_play: () => 0,
      qsys_audio_pause: () => 0,
      qsys_audio_seek: () => 0,
      qsys_audio_get_time: () => 0,
    },
    storage: {
      qsys_storage_get: () => 0,
      qsys_storage_set: () => 0,
    },
    canvas: {
      qsys_canvas_blit: () => 0,
    },
    clipboard: {
      qsys_clipboard_copy: () => 0,
    },
    wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 })
  };
}

function resolveQsysWasmPath() {
  const candidates = [
    path.join(__dirname, "qsys.wasm"),
    path.join(process.cwd(), "node_modules/@ttypt/qsys/dist/qsys.wasm"),
    path.join(process.cwd(), "external/libqsys/npm/dist/qsys.wasm"),
    path.join(process.cwd(), "site/external/libqsys/npm/dist/qsys.wasm")
  ];
  for (const c of candidates) {
    try {
      if (fs.existsSync(c)) return c;
    } catch {}
  }
  return candidates[0];
}

/**
 * Retrieves the single shared WebAssembly substrate instance.
 * Guaranteed to instantiate WebAssembly strictly ONCE across the application lifecycle.
 */
export function getSubstrate() {
  if (globalThis.__ttypt_substrate__) {
    return globalThis.__ttypt_substrate__;
  }

  let wasmInst = null;
  if (typeof process !== "undefined" && process.versions?.node) {
    try {
      const wasmPath = resolveQsysWasmPath();
      if (fs.existsSync(wasmPath)) {
        const wasmBuf = fs.readFileSync(wasmPath);
        const wasmMod = new WebAssembly.Module(wasmBuf);
        wasmInst = new WebAssembly.Instance(wasmMod, getDefaultImports());
      }
    } catch (err) {
      console.warn("Unable to boot qsys.wasm synchronously:", err.message);
    }
  }

  if (!wasmInst) {
    // Pure in-memory fallback instance
    const memory = new WebAssembly.Memory({ initial: 256, maximum: 4096 });
    wasmInst = { exports: { memory } };
  }

  const substrate = new WasmSubstrate(wasmInst);
  globalThis.__ttypt_substrate__ = substrate;
  globalThis.__ttypt_shared_wasm__ = substrate.instance.exports;
  return substrate;
}

/**
 * Backward-compatible helper returning substrate WASM exports
 */
export function getSharedWasm() {
  const substrate = getSubstrate();
  return substrate.instance.exports;
}

/**
 * Instantiates strictly ONCE and caches on globalThis.__ttypt_substrate__.
 */
export async function getSubstrateAsync() {
  if (globalThis.__ttypt_substrate__) {
    return globalThis.__ttypt_substrate__;
  }

  let wasmBytes = null;
  if (typeof process !== "undefined" && process.versions?.node) {
    const wasmPath = resolveQsysWasmPath();
    wasmBytes = fs.readFileSync(wasmPath);
  } else if (typeof fetch !== "undefined") {
    const res = await fetch(new URL("qsys.wasm", import.meta.url));
    wasmBytes = await res.arrayBuffer();
  }

  let wasmInst = null;
  if (wasmBytes) {
    const { instance } = await WebAssembly.instantiate(wasmBytes, getDefaultImports());
    wasmInst = instance;
  } else {
    const memory = new WebAssembly.Memory({ initial: 256, maximum: 4096 });
    wasmInst = { exports: { memory } };
  }

  const substrate = new WasmSubstrate(wasmInst);
  globalThis.__ttypt_substrate__ = substrate;
  return substrate;
}
