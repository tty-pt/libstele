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
