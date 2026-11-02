export declare function nowMs(): number;
export declare function selfRssBytes(): number;

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
  free(ptr: number): void;
  encodeString(str: string): [number, number];
  kernelBoot(): number;
  kernelMount(slot: number, name: string, json?: string): number;
  kernelAction(slot: number, actionId: number, data?: string): number;
  kernelUnmount(slot: number): number;
  kernelVersion(): number;
  registerModule(name: string, api: any): this;
  getModule<T = any>(name: string): T | null;
  hasModule(name: string): boolean;
  listModules(): string[];
}

export declare function getSubstrate(): WasmSubstrate;
export declare function getSubstrateAsync(): Promise<WasmSubstrate>;
export declare function getSharedWasm(): any;
