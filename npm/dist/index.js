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
    nativeBinding = req("../build/Release/stele_node.node");
  } catch {}
  if (!nativeBinding) {
    try {
      nativeBinding = req(path.join(process.cwd(), "build/Release/stele_node.node"));
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
    const fn = this.instance.exports.stl_kernel_boot || this.instance.exports.stele_kernel_boot;
    if (fn) return fn();
    return 0;
  }

  kernelMount(slot, name, json = "{}") {
    const mountFn = this.instance.exports.stl_kernel_mount || this.instance.exports.stele_kernel_mount;
    if (!mountFn) return -1;
    const [namePtr] = this.encodeString(name);
    const [jsonPtr, jsonLen] = this.encodeString(json);
    const res = mountFn(slot, namePtr, jsonPtr, jsonLen);
    this.free(namePtr);
    this.free(jsonPtr);
    return res;
  }

  kernelAction(slot, actionId, data = "") {
    const actionFn = this.instance.exports.stl_kernel_action || this.instance.exports.stele_kernel_action;
    if (!actionFn) return -1;
    const [dataPtr] = this.encodeString(data);
    const res = actionFn(slot, actionId, dataPtr);
    this.free(dataPtr);
    return res;
  }

  kernelUnmount(slot) {
    const unmountFn = this.instance.exports.stl_kernel_unmount || this.instance.exports.stele_kernel_unmount;
    if (unmountFn) return unmountFn(slot);
    return 0;
  }

  kernelVersion() {
    const fn = this.instance.exports.stl_kernel_version || this.instance.exports.stele_kernel_version;
    if (fn) return Number(fn());
    return 2;
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
  const steleImports = {
    stl_host_emit_patch: () => {},
    stl_host_log: () => {},
    stl_host_fetch: () => {},
    stl_host_set_location: () => {},
    stl_host_invalidate: () => {},
    stele_host_emit_patch: () => {},
    stele_host_log: () => {},
    stele_host_fetch: () => {},
    stele_host_set_location: () => {},
    stele_host_invalidate: () => {},
  };
  return {
    stele: steleImports,
    stl: steleImports,
    audio: {
      stl_audio_play: () => 0,
      stl_audio_pause: () => 0,
      stl_audio_seek: () => 0,
      stl_audio_get_time: () => 0,
      stele_audio_play: () => 0,
      stele_audio_pause: () => 0,
      stele_audio_seek: () => 0,
      stele_audio_get_time: () => 0,
    },
    storage: {
      stl_storage_get: () => 0,
      stl_storage_set: () => 0,
      stele_storage_get: () => 0,
      stele_storage_set: () => 0,
    },
    canvas: {
      stl_canvas_blit: () => 0,
      stele_canvas_blit: () => 0,
    },
    clipboard: {
      stl_clipboard_copy: () => 0,
      stele_clipboard_copy: () => 0,
    },
    wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 })
  };
}

function resolveSteleWasmPath() {
  const candidates = [
    path.join(__dirname, "stele.wasm"),
    path.join(process.cwd(), "node_modules/@tty-pt/stele/dist/stele.wasm"),
    path.join(process.cwd(), "external/libstele/npm/dist/stele.wasm"),
    path.join(process.cwd(), "site/external/libstele/npm/dist/stele.wasm")
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
      const wasmPath = resolveSteleWasmPath();
      if (fs.existsSync(wasmPath)) {
        const wasmBuf = fs.readFileSync(wasmPath);
        const wasmMod = new WebAssembly.Module(wasmBuf);
        wasmInst = new WebAssembly.Instance(wasmMod, getDefaultImports());
      }
    } catch (err) {
      console.warn("Unable to boot stele.wasm synchronously:", err.message);
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
    const wasmPath = resolveSteleWasmPath();
    wasmBytes = fs.readFileSync(wasmPath);
  } else if (typeof fetch !== "undefined") {
    const res = await fetch(new URL("stele.wasm", import.meta.url));
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
