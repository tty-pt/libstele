export declare function nowMs(): number;
export declare function selfRssBytes(): number;

export interface SlotBridge {
  root: HTMLElement;
  namespace?: string;
  makeImports?(): Record<string, Function>;
  mount?(instance: WebAssembly.Instance, state: any): void;
  update?(slot: number): void;
  unmount?(slot: number): void;
  emitPatch?(op: string, a: string, b: string, c: string): void;
}

export type SlotBridgeFactory = (root: HTMLElement) => SlotBridge | null;


export declare class CapabilityRegistry<T = any> {
  constructor();
  register(name: string, provider: T): this;
  get(name: string): T | null;
  has(name: string): boolean;
  list(): string[];
  unregister(name: string): boolean;
}

export interface WasmSubstrate {
  readonly memory: WebAssembly.Memory;
  readonly instance: WebAssembly.Instance;
  malloc(size: number): number;
  calloc(nmemb: number, size: number): number;
  realloc(ptr: number, size: number): number;
  free(ptr: number): void;
  encodeString(str: string): [number, number];
  writeBytes(bytes: Uint8Array | ArrayLike<number>): [number, number];
  allocString(str: string): number;
  readString(ptr: number, maxLen?: number): string;
  withString<T>(str: string, fn: (ptr: number) => T): T;
  withStrings<T>(strings: string[], fn: (...ptrs: number[]) => T): T;
  u8(): Uint8Array;
  u32(): Uint32Array;
  i32(): Int32Array;
  f32(): Float32Array;
  f64(): Float64Array;
  kernelBoot(): number;
  kernelMount(slot: number, name: string, json?: string): number;
  kernelMountBin(slot: number, name: string, bin: Uint8Array): number;
  kernelUpdate(slot: number): number;
  kernelAction(slot: number, actionId: number, data?: string): number;
  kernelUnmount(slot: number): number;
  kernelVersion(): number;
  eventEmit(topic: string, payload?: string | Uint8Array | Record<string, any>): number;
  onEvent(topic: string, payload?: string | Uint8Array | Record<string, any>): number;
  registerModule(name: string, api: any): this;
  getModule<T = any>(name: string): T | null;
  hasModule(name: string): boolean;
  listModules(): string[];
}

export declare function getSubstrate(): WasmSubstrate;
export declare function getSubstrateAsync(): Promise<WasmSubstrate>;
export declare function getSharedWasm(): any;

export declare function loadWasmModule(wasmUrl: URL | string, customImports?: Record<string, any>): WebAssembly.Instance;
export declare function loadWasmModuleAsync(wasmUrl: URL | string, customImports?: Record<string, any>): Promise<WebAssembly.Instance>;
export declare function loadNativeAddon<T = any>(callerImportMetaUrl: string, name: string): T | null;
