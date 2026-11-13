const _nodeFs = typeof process !== "undefined" && typeof process.getBuiltinModule === "function" ? process.getBuiltinModule("fs") : null;
const _nodePath = typeof process !== "undefined" && typeof process.getBuiltinModule === "function" ? process.getBuiltinModule("path") : null;
const _nodeUrl = typeof process !== "undefined" && typeof process.getBuiltinModule === "function" ? process.getBuiltinModule("url") : null;

const __dirname = typeof import.meta !== "undefined" && import.meta.url && _nodePath && _nodeUrl && _nodeUrl.fileURLToPath
  ? _nodePath.dirname(_nodeUrl.fileURLToPath(import.meta.url))
  : "";

let nativeBinding = loadNativeAddon(import.meta.url, "stele");

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

  /** Dynamic TypedArray views immune to memory.grow() detachment */
  u8() {
    return new Uint8Array(this.memory.buffer);
  }

  u32() {
    return new Uint32Array(this.memory.buffer);
  }

  i32() {
    return new Int32Array(this.memory.buffer);
  }

  f32() {
    return new Float32Array(this.memory.buffer);
  }

  f64() {
    return new Float64Array(this.memory.buffer);
  }

  /**
   * Allocates null-terminated UTF-8 string in WASM memory
   * @param {string} str
   * @returns {number} Pointer to allocated string (caller must free or use withString)
   */
  allocString(str) {
    const bytes = new TextEncoder().encode(str + "\0");
    const ptr = this.malloc(bytes.length);
    new Uint8Array(this.memory.buffer, ptr, bytes.length).set(bytes);
    return ptr;
  }

  /**
   * Decodes a null-terminated UTF-8 string from WASM memory
   * @param {number} ptr
   * @param {number} [maxLen]
   * @returns {string}
   */
  readString(ptr, maxLen = 0) {
    if (!ptr) return "";
    const u8 = this.u8();
    let len = 0;
    if (maxLen > 0) {
      len = maxLen;
    } else {
      while (u8[ptr + len] !== 0) len++;
    }
    return new TextDecoder().decode(new Uint8Array(this.memory.buffer, ptr, len));
  }

  /**
   * RAII helper: Allocates string, executes callback with pointer, and guarantees memory is freed
   * @param {string} str
   * @param {(ptr: number) => any} fn
   * @returns {any}
   */
  withString(str, fn) {
    const ptr = this.allocString(str);
    try {
      return fn(ptr);
    } finally {
      this.free(ptr);
    }
  }

  /**
   * RAII helper for multiple strings
   * @param {string[]} strings
   * @param {(...ptrs: number[]) => any} fn
   * @returns {any}
   */
  withStrings(strings, fn) {
    const ptrs = strings.map((s) => this.allocString(s));
    try {
      return fn(...ptrs);
    } finally {
      for (const p of ptrs) this.free(p);
    }
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
    const bytes = new TextEncoder().encode(str);
    const ptr = this.malloc(bytes.length + 1);
    const view = new Uint8Array(this.memory.buffer, ptr, bytes.length + 1);
    view.set(bytes);
    view[bytes.length] = 0;
    return [ptr, bytes.length];
  }

  writeBytes(bytes) {
    const u8 = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes || []);
    const ptr = this.malloc(u8.length);
    new Uint8Array(this.memory.buffer, ptr, u8.length).set(u8);
    return [ptr, u8.length];
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

  kernelMountBin(slot, name, bin) {
    const mountBinFn = this.instance.exports.stl_kernel_mount_bin || this.instance.exports.stele_kernel_mount_bin;
    if (!mountBinFn) return -1;
    const [namePtr] = this.encodeString(name);
    const [binPtr, binLen] = this.writeBytes(bin);
    const res = mountBinFn(slot, namePtr, binPtr, binLen);
    this.free(namePtr);
    this.free(binPtr);
    return res;
  }

  kernelUpdate(slot) {
    const updateFn = this.instance.exports.stl_kernel_update || this.instance.exports.stele_kernel_update;
    if (updateFn) return updateFn(slot);
    return 0;
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

  eventEmit(topic, payload = "") {
    const emitFn = this.instance.exports.stl_event_emit || this.instance.exports.stele_event_emit;
    if (!emitFn) return -1;
    const payloadBytes = typeof payload === "string"
      ? new TextEncoder().encode(payload)
      : (payload instanceof Uint8Array ? payload : new TextEncoder().encode(JSON.stringify(payload)));
    const [topicPtr] = this.encodeString(topic);
    const [payloadPtr, payloadLen] = payloadBytes.length > 0 ? this.writeBytes(payloadBytes) : [0, 0];
    const res = emitFn(topicPtr, payloadPtr, payloadLen);
    this.free(topicPtr);
    if (payloadPtr) this.free(payloadPtr);
    return res;
  }

  onEvent(topic, payload = "") {
    const onEventFn = this.instance.exports.stl_on_event || this.instance.exports.stele_on_event;
    if (!onEventFn) return -1;
    const payloadBytes = typeof payload === "string"
      ? new TextEncoder().encode(payload)
      : (payload instanceof Uint8Array ? payload : new TextEncoder().encode(JSON.stringify(payload)));
    const [topicPtr, topicLen] = this.encodeString(topic);
    const [payloadPtr, payloadLen] = payloadBytes.length > 0 ? this.writeBytes(payloadBytes) : [0, 0];
    const res = onEventFn(topicPtr, topicLen, payloadPtr, payloadLen);
    this.free(topicPtr);
    if (payloadPtr) this.free(payloadPtr);
    return res;
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
    stl_host_event_emit: () => {},
    stele_host_emit_patch: () => {},
    stele_host_log: () => {},
    stele_host_fetch: () => {},
    stele_host_set_location: () => {},
    stele_host_invalidate: () => {},
    stele_host_event_emit: () => {},
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
  const fileUrlPath = typeof import.meta !== "undefined" && import.meta.url && _nodeUrl && _nodeUrl.fileURLToPath
    ? _nodeUrl.fileURLToPath(new URL("./stele.wasm", import.meta.url))
    : "";
  const candidates = [
    fileUrlPath,
    _nodePath ? _nodePath.join(__dirname, "stele.wasm") : "stele.wasm",
    _nodePath && typeof process !== "undefined" && process.cwd ? _nodePath.join(process.cwd(), "node_modules/@tty-pt/stele/dist/stele.wasm") : "",
    _nodePath && typeof process !== "undefined" && process.cwd ? _nodePath.join(process.cwd(), "external/libstele/npm/dist/stele.wasm") : ""
  ];
  for (const c of candidates) {
    if (!c) continue;
    try {
      if (_nodeFs && _nodeFs.existsSync(c)) return c;
    } catch {}
  }
  return candidates[0] || (_nodePath ? _nodePath.join(__dirname, "stele.wasm") : "stele.wasm");
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
      if (_nodeFs && _nodeFs.existsSync(wasmPath)) {
        const wasmBuf = _nodeFs.readFileSync(wasmPath);
        const wasmMod = new WebAssembly.Module(wasmBuf);
        wasmInst = new WebAssembly.Instance(wasmMod, getDefaultImports());
        if (wasmInst.exports._initialize) {
          wasmInst.exports._initialize();
        }
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
    if (_nodeFs && _nodeFs.existsSync(wasmPath)) {
      wasmBytes = _nodeFs.readFileSync(wasmPath);
    }
  } else if (typeof fetch !== "undefined") {
    const res = await fetch(new URL("stele.wasm", import.meta.url));
    wasmBytes = await res.arrayBuffer();
  }

  let wasmInst = null;
  if (wasmBytes) {
    const { instance } = await WebAssembly.instantiate(wasmBytes, getDefaultImports());
    if (instance.exports._initialize) {
      instance.exports._initialize();
    }
    wasmInst = instance;
  } else {
    const memory = new WebAssembly.Memory({ initial: 256, maximum: 4096 });
    wasmInst = { exports: { memory } };
  }

  const substrate = new WasmSubstrate(wasmInst);
  globalThis.__ttypt_substrate__ = substrate;
  return substrate;
}

/**
 * Plug-and-play module loader: Instantiates any downstream WASM module onto
 * the Stele shared-memory substrate with zero consumer boilerplate.
 *
 * @param {URL|string} wasmUrl - URL to the .wasm file (e.g. new URL("./module.wasm", import.meta.url))
 * @param {object} customImports - Optional module-specific host imports
 * @returns {WebAssembly.Instance}
 */
export function loadWasmModule(wasmUrl, customImports = {}) {
  const substrate = getSubstrate();
  const sharedMemory = substrate.memory;

  const baseImports = {
    wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 }),
    env: {
      memory: sharedMemory,
      malloc: (sz) => substrate.malloc(sz),
      free: (ptr) => substrate.free(ptr),
      abort: () => { throw new Error("WASM abort"); },
      ...(customImports.env || {})
    },
    ...customImports
  };
  baseImports.env.memory = sharedMemory;
  if (!baseImports.env.malloc) baseImports.env.malloc = (sz) => substrate.malloc(sz);
  if (!baseImports.env.free) baseImports.env.free = (ptr) => substrate.free(ptr);
  const mergedImports = new Proxy(baseImports, {
    get(target, prop) {
      if (prop in target) return target[prop];
      return new Proxy({}, { get: () => () => 0 });
    }
  });

  let wasmBytes = null;
  if (typeof process !== "undefined" && process.versions?.node) {
    try {
      const getModule = globalThis.process?.getBuiltinModule;
      const fs = getModule ? getModule("fs") : null;
      if (fs) {
        wasmBytes = fs.readFileSync(wasmUrl);
      }
    } catch {}
  } else if (typeof globalThis !== "undefined" && globalThis.__wasm_bytes_cache__?.[String(wasmUrl)]) {
    wasmBytes = globalThis.__wasm_bytes_cache__[String(wasmUrl)];
  }

  function wrapInstance(inst) {
    if (inst.exports._initialize) {
      inst.exports._initialize();
    }
    const exportsProxy = new Proxy(inst.exports, {
      get(target, prop) {
        if (prop in target) return target[prop];
        if (prop === "malloc") return (sz) => substrate.malloc(sz);
        if (prop === "free") return (ptr) => substrate.free(ptr);
        if (prop === "memory") return sharedMemory;
        return undefined;
      }
    });
    return new Proxy(inst, {
      get(target, prop) {
        if (prop === "exports") return exportsProxy;
        return target[prop];
      }
    });
  }

  if (wasmBytes) {
    const mod = new WebAssembly.Module(wasmBytes);
    const inst = new WebAssembly.Instance(mod, mergedImports);
    return wrapInstance(inst);
  }

  throw new Error(`Unable to load WebAssembly module from ${wasmUrl}`);
}

/**
 * Universal asynchronous loader for Browser / Web workers
 */
export async function loadWasmModuleAsync(wasmUrl, customImports = {}) {
  const substrate = await getSubstrateAsync();
  const sharedMemory = substrate.memory;

  const baseImports = {
    wasi_snapshot_preview1: new Proxy({}, { get: () => () => 0 }),
    env: {
      memory: sharedMemory,
      malloc: (sz) => substrate.malloc(sz),
      free: (ptr) => substrate.free(ptr),
      abort: () => { throw new Error("WASM abort"); },
      ...(customImports.env || {})
    },
    ...customImports
  };
  baseImports.env.memory = sharedMemory;
  if (!baseImports.env.malloc) baseImports.env.malloc = (sz) => substrate.malloc(sz);
  if (!baseImports.env.free) baseImports.env.free = (ptr) => substrate.free(ptr);
  const mergedImports = new Proxy(baseImports, {
    get(target, prop) {
      if (prop in target) return target[prop];
      return new Proxy({}, { get: () => () => 0 });
    }
  });

  if (typeof fetch !== "undefined") {
    const { instance } = await WebAssembly.instantiateStreaming(fetch(wasmUrl), mergedImports);
    const exportsProxy = new Proxy(instance.exports, {
      get(target, prop) {
        if (prop in target) return target[prop];
        if (prop === "malloc") return (sz) => substrate.malloc(sz);
        if (prop === "free") return (ptr) => substrate.free(ptr);
        if (prop === "memory") return sharedMemory;
        return undefined;
      }
    });
    if (instance.exports._initialize) instance.exports._initialize();
    return new Proxy(instance, {
      get(target, prop) {
        if (prop === "exports") return exportsProxy;
        return target[prop];
      }
    });
  }
  return loadWasmModule(wasmUrl, customImports);
}

/**
 * Universal Node-API native addon discoverer.
 * Automatically checks local build, installed package, and project cwd paths.
 * Returns null if running in browser or addon unavailable.
 *
 * @param {string} callerImportMetaUrl - Caller's import.meta.url
 * @param {string} name - Addon module stem (e.g. "stele", "custom")
 * @returns {object|null}
 */
export function loadNativeAddon(callerImportMetaUrl, name) {
  if (typeof process === "undefined" || !process.versions?.node) {
    return null;
  }
  try {
    const getModule = globalThis.process?.getBuiltinModule;
    const req = getModule ? getModule("module").createRequire(callerImportMetaUrl) : null;
    if (!req) return null;

    const candidates = [
      `../build/Release/${name}_node.node`,
      `./${name}_node.node`,
      `./${name}.node`,
      `../dist/${name}.node`,
      `../prebuilds/${process.platform}-${process.arch}/${name}.node`,
      `./prebuilds/${process.platform}-${process.arch}/${name}.node`,
      `@tty-pt/${name}-${process.platform}-${process.arch}`
    ];
    for (const c of candidates) {
      try {
        const mod = req(c);
        if (mod) return mod;
      } catch {}
    }
  } catch {}
  return null;
}
