// Loads the wall autotiler out of the lab page and makes it callable from Node.
//
// The rule is authored in tools/wall_lab/scenarios_template.html because that
// is where it gets eyeballed against the real tilesheet. Rather than keep a
// second copy here (which would drift the moment either side changed), this
// evaluates the page's own <script> in a vm with a stubbed DOM. It reads the
// TEMPLATE, not the spliced scenarios.html, so the base64 sheets are never
// needed - Image is a stub and the src it gets is a %%WALLS%% placeholder.
//
// Everything the page declares is a top-level `const`/`let` inside the vm
// script's own lexical scope, so it is NOT reachable on the context object.
// The only way in is to append a probe to the source that closes over it,
// which is what buildProbe does.
"use strict";
const fs = require("fs");
const vm = require("vm");
const path = require("path");

const TEMPLATE = path.join(__dirname, "..", "..", "tools", "wall_lab", "scenarios_template.html");

const noop = () => {};

function fakeCtx() {
  // Every 2D-context call is a no-op; nothing here inspects pixels.
  return new Proxy({}, {
    get: (_t, k) => (k === "measureText" ? () => ({ width: 10 }) : noop),
    set: () => true,
  });
}

function fakeEl(tag) {
  return {
    tagName: String(tag || "div").toUpperCase(),
    children: [], style: {}, dataset: {},
    // dunZoom/dunSize are read with parseInt/lookup, so a numeric-ish default keeps them happy.
    value: "3", checked: false, textContent: "", innerHTML: "", hidden: false,
    width: 0, height: 0,
    classList: { add: noop, remove: noop, toggle: noop, contains: () => false },
    addEventListener: noop, removeEventListener: noop, appendChild: noop, append: noop,
    remove: noop, click: noop, focus: noop, setAttribute: noop, getAttribute: () => null,
    getContext: () => fakeCtx(),
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 100, height: 100 }),
    querySelector: () => fakeEl("div"), querySelectorAll: () => [],
    scrollIntoView: noop, closest: () => null, insertAdjacentHTML: noop,
    toDataURL: () => "data:,",
  };
}

function makeContext() {
  const els = Object.create(null);
  const document = {
    getElementById: id => (els[id] || (els[id] = fakeEl(id))),
    createElement: fakeEl,
    querySelector: () => fakeEl("div"),
    querySelectorAll: () => [],
    addEventListener: noop,
    body: fakeEl("body"),
    documentElement: fakeEl("html"),
  };
  class FakeImage {
    constructor() {
      this._src = "";
      // Real sheet dims; the page divides by these to lay out its picker.
      this.naturalWidth = 576; this.naturalHeight = 576;
      this.width = 576; this.height = 576;
    }
    set src(v) { this._src = v; setImmediate(() => this.onload && this.onload()); }
    get src() { return this._src; }
    addEventListener(t, f) { if (t === "load") this.onload = f; }
  }
  const store = Object.create(null);
  const ctx = {
    document, Image: FakeImage, console,
    localStorage: {
      getItem: k => (k in store ? store[k] : null),
      setItem: (k, v) => { store[k] = String(v); },
      removeItem: k => { delete store[k]; },
      clear: () => { for (const k in store) delete store[k]; },
    },
    navigator: { clipboard: { writeText: async () => {} } },
    setTimeout, clearTimeout, setInterval, clearInterval, setImmediate,
    requestAnimationFrame: f => setImmediate(f),
    URL: { createObjectURL: () => "blob:x", revokeObjectURL: noop },
    Blob: class {}, FileReader: class { readAsText() {} },
    __API: {},
  };
  ctx.window = ctx;
  ctx.globalThis = ctx;
  return ctx;
}

// Appended to the page source so it can close over the page's private scope.
const PROBE = `
;(function () {
  __API.mapping = mapping;
  __API.setup = function (w, h, floors) {
    MAP_W = w; MAP_H = h;
    map = Array.from({ length: h }, () => Array(w).fill("#"));
    for (const [fx, fy] of floors) map[fy][fx] = ".";
  };
  __API.roleAt = function (x, y, layer) {
    if (layer === "overlay") return overlayRoleAt(x, y);
    if (!isWallCell(map[y][x])) return null;
    return wbRoleForCell(x, y, computeWallCell(x, y));
  };
  __API.setWalkBehind = function (on) { WALKBEHIND = on; };
  __API.counterpartCell = counterpartCell;
  __API.KIT_ROWS = KIT_ROWS;
})();
`;

/**
 * @param {{mutate?: (src: string) => string}} [opts]
 *   mutate rewrites the page source before evaluation - used by the mutation
 *   tests to prove the suite actually fails when the rule is broken.
 * @returns {{mapping: object, setup: Function, roleAt: Function, setWalkBehind: Function,
 *            cellOf: Function, roleForCell: Function, emits: Function}}
 */
function loadRule(opts = {}) {
  const html = fs.readFileSync(TEMPLATE, "utf8");
  const m = html.match(/<script>([\s\S]*)<\/script>/);
  if (!m) throw new Error(`no <script> block found in ${TEMPLATE}`);
  let src = m[1];
  if (opts.mutate) {
    const before = src;
    src = opts.mutate(src);
    if (src === before) throw new Error("mutate() changed nothing - the pattern did not match");
  }
  const ctx = makeContext();
  vm.createContext(ctx);
  vm.runInContext(src + PROBE, ctx, { filename: "scenarios_template.html<script>", timeout: 20000 });

  const api = ctx.__API;
  const kit = "boulder";
  // Roles alias: ksw===ccnw===[0,6,13] and kse===ccne===[0,5,13]. Compare the
  // DRAWN CELL, never the role name, or correct renders read as failures.
  api.cellOf = role => (role && api.mapping[kit][role] ? api.mapping[kit][role].join(",") : null);
  const emitted = new Set(Object.keys(api.mapping[kit]).map(r => api.mapping[kit][r].join(",")));
  api.emits = cell => emitted.has(Array.isArray(cell) ? cell.join(",") : cell);
  return api;
}

module.exports = { loadRule, TEMPLATE };
