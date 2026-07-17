// Score a candidate autotiler rule against the user's whole review.
//
//   node tools/wall_lab/try_rule.js <patch.js> [-v]
//
// patch.js must export ONE of:
//   { patches: [[find, replace], ...] }   exact string swaps; each must hit
//                                         exactly 1 site (template is CRLF, so
//                                         a find spanning a newline matches 0)
//   { mutate(src) { return src } }        full control
//
// Prints two numbers that both matter:
//   fixes  - of the 73 cells the user marked, how many now draw what they asked
//   keeps  - of the ~3000 cells they reviewed and left alone, how many are intact
//
// A candidate is only better if it raises fixes WITHOUT dropping keeps. Chasing
// fixes alone is exactly how the round-3 rule hit 56/61 while wrecking 421 cells
// nobody asked it to touch.
"use strict";
const path = require("path");
const { loadRule } = require("../../tests/js/rule_harness.js");
const CASES = require("../../tests/js/wall_cases.json");

const VERBOSE = process.argv.includes("-v");
const patchPath = process.argv[2];
if (!patchPath) { console.error("usage: node tools/wall_lab/try_rule.js <patch.js> [-v]"); process.exit(2); }

const patch = require(path.resolve(patchPath));
const mutate = patch.mutate ? patch.mutate : src => {
  let s = src;
  for (const [find, repl] of patch.patches) {
    const n = s.split(find).length - 1;
    if (n !== 1) {
      throw new Error(`patch matched ${n} sites, expected exactly 1 (template is CRLF; ` +
                      `single-line finds only):\n  ${JSON.stringify(find.slice(0, 90))}`);
    }
    s = s.replace(find, repl);
  }
  return s;
};

function score(api) {
  let fixHit = 0, fixMiss = 0, keepOk = 0;
  const missed = [], broke = [];
  for (const r of CASES.reviewed) {
    for (const f of r.fix) {
      api.setup(r.w, r.h, r.floors);
      const got = api.roleAt(f.xy[0], f.xy[1], f.layer);
      if (api.cellOf(got) === f.cell.join(",")) fixHit++;
      else { fixMiss++; missed.push(`${r.name || r.id} (${f.xy}) ${f.layer}: want [${f.cell}], got ${got}`); }
    }
    for (const [key, was] of Object.entries(r.keep)) {
      const [xy, layer] = key.split("@");
      const [x, y] = xy.split(",").map(Number);
      api.setup(r.w, r.h, r.floors);
      const got = api.roleAt(x, y, layer) || null;
      if (got === was) keepOk++;
      else broke.push({ id: r.name || r.id, kind: r.kind, key, was, got });
    }
  }
  return { fixHit, fixMiss, keepOk, missed, broke };
}

let api;
try { api = loadRule({ mutate }); }
catch (e) { console.error(`PATCH FAILED: ${e.message}`); process.exit(2); }

const s = score(api);
const keepTot = s.keepOk + s.broke.length;
console.log(`fixes  ${s.fixHit}/${s.fixHit + s.fixMiss}`);
console.log(`keeps  ${s.keepOk}/${keepTot}${s.broke.length ? `   <-- ${s.broke.length} BROKEN` : "   (intact)"}`);

if (s.broke.length) {
  const byShape = {};
  for (const b of s.broke) {
    const k = `${b.key.split("@")[1]}: ${b.was || "none"} -> ${b.got || "none"}`;
    (byShape[k] = byShape[k] || []).push(b);
  }
  console.log("\nbroken keeps (cells the user left alone that this moves):");
  for (const [shape, list] of Object.entries(byShape).sort((a, b) => b[1].length - a[1].length).slice(0, 12)) {
    console.log(`  ${String(list.length).padStart(4)}  ${shape}`);
    if (VERBOSE) for (const b of list.slice(0, 3)) console.log(`          ${b.id} (${b.key})${b.kind === "approved" ? " [APPROVED render]" : ""}`);
  }
}
if (VERBOSE && s.missed.length) {
  console.log("\nfixes still missed:");
  for (const m of s.missed) console.log(`  - ${m}`);
}
console.log(s.broke.length === 0 && s.fixMiss === 0 ? "\nSOLVED\n" : "");
process.exit(s.broke.length ? 1 : 0);
