// Host test for the dungeon wall autotiler, run against the rule as authored in
// tools/wall_lab/scenarios_template.html. Mirrors what tests/ does for core/*:
// no framework, its own main, non-zero exit on failure.
//
//   node tests/js/test_wall_rule.js [-v]
//
// Two questions, and the second one is the one that bites:
//
//   1. Did we fix what the user marked?    (72 fix cells: 56 gate + 16 xfail)
//   2. Did we change anything else?        (2796 keep cells)
//
// A cell the user did not mark in a scenario they did review is an implicit
// "this one is fine". Those outnumber the marked cells ~39:1. A rule can satisfy
// every correction and still be wrong, by wrecking everything around them - that
// is exactly what happened when this suite only measured question 1.
//
// Scoring is by DRAWN CELL for fixes, never role name: ksw===ccnw===[0,6,13] and
// kse===ccne===[0,5,13], so a name compare invents failures on tiles that render
// perfectly. Keeps compare role names, since the baseline and the current rule
// are the same codebase and a role change there is worth knowing about.
"use strict";
const fs = require("fs");
const path = require("path");
const { loadRule } = require("./rule_harness.js");

const VERBOSE = process.argv.includes("-v") || process.argv.includes("--verbose");
const CASES = JSON.parse(fs.readFileSync(path.join(__dirname, "wall_cases.json"), "utf8"));

const RED = s => `\x1b[31m${s}\x1b[0m`;
const GRN = s => `\x1b[32m${s}\x1b[0m`;
const YEL = s => `\x1b[33m${s}\x1b[0m`;
const DIM = s => `\x1b[2m${s}\x1b[0m`;

function main() {
  const api = loadRule();
  const regressions = [], solved = [], changed = [];
  let pass = 0, xfail = 0, kept = 0;

  for (const r of CASES.reviewed) {
    for (const f of r.fix) {
      api.setup(r.w, r.h, r.floors);
      const got = api.roleAt(f.xy[0], f.xy[1], f.layer);
      const hit = api.cellOf(got) === f.cell.join(",");
      const where = `${r.id} (${f.xy}) ${f.layer}`;
      if (f.status === "pass") {
        if (hit) pass++;
        else regressions.push(`${where}: want [${f.cell}], got ${got} [${api.cellOf(got)}]`);
      } else if (hit) {
        solved.push(`${where}: now draws [${f.cell}]`);
      } else {
        xfail++;
        if (VERBOSE) console.log(DIM(`    xfail ${where} - ${f.reason}`));
      }
    }
    for (const [key, wasRole] of Object.entries(r.keep)) {
      const [xy, layer] = key.split("@");
      const [x, y] = xy.split(",").map(Number);
      api.setup(r.w, r.h, r.floors);
      const got = api.roleAt(x, y, layer) || null;
      if (got === wasRole) { kept++; continue; }
      changed.push({ id: r.id, kind: r.kind, key, was: wasRole, now: got });
    }
  }

  const keepTotal = kept + changed.length;
  console.log(`\nfixes the user asked for   ${GRN(pass + " pass")}  ${YEL(xfail + " xfail")}  of ${pass + xfail + solved.length + regressions.length}`);
  console.log(`cells the user left alone  ${changed.length ? RED(kept + "/" + keepTotal + " kept") : GRN(kept + "/" + keepTotal + " kept")}`);

  if (solved.length) {
    console.log(YEL(`\n  ${solved.length} known-failing case${solved.length > 1 ? "s" : ""} now PASSES:`));
    for (const s of solved) console.log(YEL(`    + ${s}`));
    console.log(DIM("    (rerun tools/wall_lab/gen_cases.js to promote them into the gate)"));
  }
  if (regressions.length) {
    console.log(RED(`\n  ${regressions.length} regression${regressions.length > 1 ? "s" : ""}:`));
    for (const f of regressions) console.log(RED(`    x ${f}`));
  }
  if (changed.length) {
    const inApproved = changed.filter(c => c.kind === "approved").length;
    console.log(RED(`\n  ${changed.length} UNREQUESTED CHANGES - cells the user reviewed and did not mark:`));
    if (inApproved) console.log(RED(`    ${inApproved} of them inside renders the user explicitly approved.`));
    // Group by transition: the shape of the damage says more than 400 lines do.
    const byShape = {};
    for (const c of changed) {
      const k = `${c.key.split("@")[1]}: ${c.was || "none"} -> ${c.now || "none"}`;
      (byShape[k] = byShape[k] || []).push(c);
    }
    for (const [shape, list] of Object.entries(byShape).sort((a, b) => b[1].length - a[1].length)) {
      console.log(RED(`    ${String(list.length).padStart(4)}  ${shape}`));
      if (VERBOSE) for (const c of list.slice(0, 3)) console.log(DIM(`            ${c.id} (${c.key})`));
    }
    console.log(DIM("\n    Each of these is a wall the user did not ask to change. Either the rule is\n" +
                    "    overfitting the marked cells, or these cells need reviewing so the data can\n" +
                    "    say they are allowed to move. Run with -v to see which scenarios."));
  }

  const bad = regressions.length + changed.length;
  console.log(bad ? RED(`\nFAILED (${bad})\n`) : GRN("\nok\n"));
  return bad ? 1 : 0;
}

process.exit(main());
