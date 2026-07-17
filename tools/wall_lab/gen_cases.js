// Regenerates tests/js/wall_cases.json from the user's review.
//
//   node tools/wall_lab/gen_cases.js
//
// Source: corrections_round4.json, pasted verbatim from the lab's "Copy
// corrections". Round 3 was hand-transcribed into a .js file; don't do that
// again - transcription is where silent errors get in.
//
// A review produces TWO kinds of evidence, and only one is visible if you look
// at the corrections list alone:
//
//   fix  - a cell the user MARKED, with the tile that should be there.
//   keep - every OTHER cell in a scenario they reviewed. An implicit "this one
//          is fine". These outnumber the fixes ~39:1.
//
// Scoring only the fixes is how the round-3 rule scored 56/61 while changing 421
// cells the user never asked to change. The keeps are the guardrail.
//
// WHERE KEEPS COME FROM: the rule as the user reviewed it. Right now the live
// rule IS that rule (the overfit one was reverted out of the template), and
// that isn't assumed - it's checked below against the export's own current_cell
// records, which say what the page drew at review time. If that check fails, the
// rule has moved and the unmarked cells are no longer evidence of anything.
"use strict";
const fs = require("fs");
const path = require("path");
const { loadRule } = require("../../tests/js/rule_harness.js");

const SRC = path.join(__dirname, "corrections_round4.json");
const DEST = path.join(__dirname, "..", "..", "tests", "js", "wall_cases.json");
const DATA = JSON.parse(fs.readFileSync(SRC, "utf8"));
const api = loadRule();

const j = c => (c ? c.join(",") : null);
const LAYERS = ["wall", "overlay"];
const die = msg => { console.error(`REFUSING: ${msg}`); process.exit(1); };

// ---- Gate 1: the live rule must be the rule the user reviewed ---------------
{
  let ok = 0; const bad = [];
  for (const s of DATA.corrections) for (const t of s.tiles) {
    if (t.layer === "floor") continue;
    api.setup(s.grid.w, s.grid.h, s.grid.floors);
    const drew = api.cellOf(api.roleAt(t.xy[0], t.xy[1], t.layer));
    const said = t.current_cell ? j(t.current_cell.boulder) : null;
    if (drew === said) ok++; else bad.push(`${s.id} (${t.xy}) ${t.layer}: export saw [${said}], rule draws [${drew}]`);
  }
  if (bad.length) {
    console.error(`baseline check FAILED on ${bad.length} of ${ok + bad.length} marked cells:`);
    bad.slice(0, 8).forEach(b => console.error(`  ${b}`));
    die("the rule has changed since this review, so its unmarked cells are not evidence.");
  }
  console.log(`baseline validated: the live rule reproduces all ${ok} cells the export recorded seeing`);
}

// ---- Gate 2: the user's two kit picks must agree with the kit offset --------
// The Sync button derives one from the other, but picks can also be made by hand.
{
  const bad = [];
  for (const s of DATA.corrections) for (const t of s.tiles) {
    const c = t.correct_cell;
    if (!c || !c.boulder || !c.brick) continue;
    const derived = [c.boulder[0], c.boulder[1], c.boulder[2] - 10];
    if (j(derived) !== j(c.brick)) bad.push(`${s.id} (${t.xy}) ${t.layer}: boulder [${c.boulder}] implies brick [${derived}], export says [${c.brick}]`);
  }
  if (bad.length) {
    bad.forEach(b => console.error(`  ${b}`));
    die("a correction's two kits disagree - one of them is a mispick.");
  }
  console.log("kit pairs validated: every correction's brick matches its boulder at the 10-row offset");
}

const reviewed = [];
const noops = [];

for (const s of DATA.corrections) {
  const { w, h, floors } = s.grid;
  const marked = new Map();
  for (const t of s.tiles) {
    if (t.layer === "floor") continue;   // the user reviewed walls + overlays, not floors
    // A "correction" whose correct tile IS the current tile isn't a fix - it's
    // the user confirming that cell. Treat it as a keep, not as a demand.
    if (t.current_cell && t.correct_cell && j(t.current_cell.boulder) === j(t.correct_cell.boulder)) {
      noops.push(`${s.id} (${t.xy}) ${t.layer} = ${t.role}`);
      continue;
    }
    marked.set(`${t.xy[0]},${t.xy[1]}@${t.layer}`, t);
  }

  const fix = [], keep = {};
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) for (const layer of LAYERS) {
    const key = `${x},${y}@${layer}`;
    api.setup(w, h, floors);
    const saw = api.roleAt(x, y, layer) || null;
    const t = marked.get(key);
    if (!t) { keep[key] = saw; continue; }
    const cell = t.correct_cell.boulder;
    let status = "xfail", reason;
    if (!api.emits(cell)) reason = `no role emits cell ${cell[1]},${cell[2]}`;
    else if (api.cellOf(saw) === j(cell)) status = "pass";
    else reason = `draws ${saw} [${api.cellOf(saw)}] instead`;
    fix.push({ xy: t.xy, layer, saw, cell, status, ...(reason ? { reason } : {}) });
  }
  reviewed.push({ id: s.id, name: s.name, kind: "corrected", w, h, floors, fix, keep });
}

// Approved: the user hit "Looks right" on the WHOLE render, so every cell is a
// keep. The export's own role/cell annotation is ignored on purpose - it named
// the pre-walk-behind role for 10 of 25 entries (a bug now fixed in the page),
// and the user approved what was DRAWN, not what the JSON called it.
for (const a of DATA.approved) {
  const { w, h, floors } = a.grid;
  const keep = {};
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) for (const layer of LAYERS) {
    api.setup(w, h, floors);
    keep[`${x},${y}@${layer}`] = api.roleAt(x, y, layer) || null;
  }
  reviewed.push({ id: a.id, name: a.name, kind: "approved", w, h, floors, fix: [], keep });
}

const out = {
  _comment: [
    "Golden data for the dungeon wall autotiler, generated from the user's round-4",
    "review. Regenerate: node tools/wall_lab/gen_cases.js  (source: corrections_round4.json)",
    "",
    "mask bits: N=1 E=2 S=4 W=8, set = FLOOR on that side.",
    "floors[] lists the OPEN cells of a w*h grid; every other cell is wall.",
    "cell is [sheet, col, row] on the Walls sheet (sheet 0 = boulder kit);",
    "brick is the same tile 10 rows up, so only boulder is pinned here.",
    "",
    "  fix[]  - cells the user MARKED. `saw` is what the rule drew at review time,",
    "           `cell` is the tile they say belongs there.",
    "             pass  = satisfied today. Regression gate.",
    "             xfail = known-unsolved. Tracked. If one starts passing the runner",
    "                     says so; rerun gen_cases.js to promote it.",
    "",
    "  keep{} - every OTHER cell in a reviewed scenario, mapped to what the user",
    "           saw and left alone. null = nothing drawn there, and that was right.",
    "           Any rule change that moves one of these changed something nobody",
    "           asked to change. This is the guardrail the round-3 rule slipped.",
    "",
    "kind: 'corrected' = user marked something here. 'approved' = user blessed the",
    "whole render, so every cell is a keep.",
  ].join("\n"),
  reviewed,
};
fs.writeFileSync(DEST, JSON.stringify(out, null, 1) + "\n");

const fixes = reviewed.flatMap(r => r.fix);
const keeps = reviewed.reduce((n, r) => n + Object.keys(r.keep).length, 0);
console.log(`\nwrote ${DEST}`);
console.log(`  ${reviewed.length} reviewed scenarios (${reviewed.filter(r => r.kind === "corrected").length} corrected, ${reviewed.filter(r => r.kind === "approved").length} approved)`);
console.log(`  fix   ${fixes.length}  (pass ${fixes.filter(f => f.status === "pass").length}, xfail ${fixes.filter(f => f.status === "xfail").length})`);
console.log(`  keep  ${keeps}  cells the user saw and left alone`);
if (noops.length) {
  console.log(`\n  ${noops.length} marked cell(s) had the SAME tile as correct - read as confirmations, kept as keeps:`);
  noops.forEach(n => console.log(`    ~ ${n}`));
}
