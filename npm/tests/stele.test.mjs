import test from "node:test";
import assert from "node:assert/strict";
import { nowMs, selfRssBytes, CapabilityRegistry, getSubstrate } from "../dist/index.js";

test("nowMs and selfRssBytes return valid positive metrics", () => {
  const t1 = nowMs();
  assert.ok(typeof t1 === "number" && t1 > 0, "nowMs must return a positive timestamp");

  const rss = selfRssBytes();
  assert.ok(typeof rss === "number" && rss > 0, "selfRssBytes must return positive memory usage");
});

test("CapabilityRegistry provides decoupled plug-and-play module registration", () => {
  const reg = new CapabilityRegistry();

  reg.register("temporal", { kind: "time-axis", version: 1 });
  reg.register("spatial", { kind: "space-axis", version: 1 });

  assert.equal(reg.has("temporal"), true);
  assert.equal(reg.has("spatial"), true);
  assert.equal(reg.has("vector"), false);

  const temporal = reg.get("temporal");
  assert.equal(temporal?.kind, "time-axis");

  assert.deepEqual(reg.list().sort(), ["spatial", "temporal"]);

  reg.unregister("temporal");
  assert.equal(reg.has("temporal"), false);
  assert.equal(reg.get("temporal"), null);
});

test("WasmSubstrate provides single-memory allocation and dynamic module registration", () => {
  const subA = getSubstrate();
  assert.ok(subA.memory instanceof WebAssembly.Memory);

  const ptr1 = subA.malloc(32);
  const ptr2 = subA.malloc(64);
  assert.ok(ptr1 > 0);
  assert.ok(ptr2 >= ptr1 + 32);

  // Dynamic module registration on the substrate
  subA.registerModule("custom_plugin", { run: () => 42 });
  assert.equal(subA.hasModule("custom_plugin"), true);
  assert.equal(subA.getModule("custom_plugin")?.run(), 42);

  // Singleton caching: second getSubstrate call returns exact same instance
  const subB = getSubstrate();
  assert.equal(subA, subB);

  // Kernel C ABI operations
  assert.equal(subA.kernelBoot(), 0);
  assert.equal(subA.kernelVersion(), 2);
  assert.equal(subA.kernelMount(0, "nonexistent_module", "{}"), -2);
  assert.equal(subA.kernelMountBin(0, "nonexistent_module", new Uint8Array([1, 2, 3, 4])), -2);
  assert.equal(subA.kernelAction(0, 1, "test"), -1);
  assert.equal(subA.kernelUnmount(0), 0);

  // In-WASM event bus ABI operations
  assert.equal(subA.eventEmit("test:event", { ok: true }), 0);
  assert.equal(subA.onEvent("test:event", "payload"), 0);
});
