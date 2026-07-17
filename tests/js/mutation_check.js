// Proves test_wall_rule.js is a real gate and not a vacuous one.
//
//   node tests/js/mutation_check.js [-v]
//
// A green suite means nothing on its own - it has to be shown capable of going
// red. Each mutation below breaks the autotiler in a way a careless edit or a
// bad port to source/core could plausibly break it. Every one MUST be caught
// by the pass-tier or the approvals. A SURVIVOR is a hole in the gate: real
// behaviour changed and the suite shrugged.
//
// This matters most for the eventual C++ port. These are exactly the mistakes
// a hand-translation makes - a flipped bit, an off-by-one neighbour, a
// transcribed lookup table - and this file is the evidence the suite would
// notice.
"use strict";
const fs = require("fs");
const path = require("path");
const { loadRule } = require("./rule_harness.js");

const VERBOSE = process.argv.includes("-v") || process.argv.includes("--verbose");
const CASES = JSON.parse(fs.readFileSync(path.join(__dirname, "wall_cases.json"), "utf8"));
const RED = s => `\x1b[31m${s}\x1b[0m`;
const GRN = s => `\x1b[32m${s}\x1b[0m`;
const DIM = s => `\x1b[2m${s}\x1b[0m`;

// find must match EXACTLY ONE site, or the mutation is lying about what it tests.
// Single-line patterns only: the template has CRLF endings, so any find string
// spanning a newline silently matches nothing. Targets the round-4 rule.
const MUTATIONS = [
  { name: "corner test reads the row above, not below",
    why: "a corner is a side run that ENDS going down; above is a different wall",
    find: "const e = (cell.em & 2) && !isOpen(x + 1, y + 1);",
    with: "const e = (cell.em & 2) && !isOpen(x + 1, y - 1);" },

  { name: "east corner returns the west tile",
    why: "classic east/west transposition in a hand-port",
    find: "if (e) return (cell.em & 8) ? \"kne2\" : \"kne\";",
    with: "if (e) return (cell.em & 8) ? \"knw2\" : \"knw\";" },

  { name: "corner variant never chosen",
    why: "a leftover opposite bit must pick kne2/knw2, not the plain corner",
    find: "if (w) return (cell.em & 2) ? \"knw2\" : \"knw\";",
    with: "if (w) return \"knw\";" },

  { name: "double corner never fires",
    why: "kn2 collapses to a one-sided corner",
    find: "if (e && w) return \"kn2\";",
    with: "if (false) return \"kn2\";" },

  { name: "side edge keeps its N bit",
    why: "the peel is the whole point: w9 must render as w8 once N lifts off",
    find: "if (cell.em & (2 | 8)) return \"w\" + em2;",
    with: "if (cell.em & (2 | 8)) return \"w\" + cell.em;" },

  { name: "face test uses W instead of S",
    why: "a south-facing face is em&4; em&8 is a different wall entirely",
    find: "if (cell.em & 4) return cell.role;                 // south-facing face: untouched",
    with: "if (cell.em & 8) return cell.role;                 // south-facing face: untouched" },

  { name: "side junction no longer wins",
    why: "fcl/fcr/faceBoth must beat corner rounding (the user corrected us back to this)",
    find: "if (cell.role === \"fcl\" || cell.role === \"fcr\" || cell.role === \"faceBoth\")",
    with: "if (false)" },

  { name: "plain caps stop peeling",
    why: "r1/r3/r9 must drop to r0/r2w/r8 when their N floor lifts off",
    find: "return em2 === 2 ? \"r2w\" : em2 === 8 ? \"r8\" : \"r0\";",
    with: "return cell.role;" },

  { name: "ccne variant condition inverted",
    why: "picks the plain corner cap where the variant belongs and vice versa",
    find: "if (cell.role === \"ccne\" || cell.role === \"ccne2\") return em2 ? \"ccne2\" : \"ccne\";",
    with: "if (cell.role === \"ccne\" || cell.role === \"ccne2\") return em2 ? \"ccne\" : \"ccne2\";" },

  { name: "turn caps lean the wrong way",
    why: "capE2 belongs where the wall continues SE, capW2 where it continues SW",
    find: "if (se && !sw) return \"capE2\";",
    with: "if (se && !sw) return \"capW2\";" },

  // NOT listed: mutating the "|| (sc.em & 4)" face guard in wbOverlayRole.
  // Proven inert by exhaustive sweep (all 512 3x3 patterns, 50k cells, zero
  // behavioural differences): every em&1 && em&4 cell is a 1-tall run whose
  // overlay comes from overlayAt (the crown/ridge system), which wins before
  // wbOverlayRole is consulted. The guard is defensive shadowed redundancy -
  // keep it in the rule, but a mutation of it can't be "caught" by anything.

  { name: "overlay drops the N bit",
    why: "the overlay IS the peeled rim: w3 not w2, w9 not w8",
    find: "return \"w\" + sc.em;",
    with: "return \"w\" + (sc.em & ~1);" },

  { name: "turn rims lean the wrong way",
    why: "rimE2 leans toward the bleed-borrowed side; swapping it flips every turn",
    find: "if (eB && !wB) return \"rimE2\";",
    with: "if (eB && !wB) return \"rimW2\";" },

  { name: "rim variant fires on single-bit rims",
    why: "one borrowed bit is a plain 2-tall side rim (w9/w3), not a turn",
    find: "if ((sc.em & 10) === 10) {",
    with: "if (sc.em & 10) {" },
];

function evaluate(api) {
  // Every assertion the golden data makes, as a comparable signature. The gate
  // is currently RED (the landed rule changes 421 cells the user left alone), so
  // "caught" cannot mean "went from green to red" - it means the set of
  // violations MOVED. That works whether or not the rule is passing today.
  let broken = 0;
  const detail = [];
  for (const r of CASES.reviewed) {
    for (const f of r.fix) {
      if (f.status !== "pass") continue;
      api.setup(r.w, r.h, r.floors);
      const got = api.roleAt(f.xy[0], f.xy[1], f.layer);
      if (api.cellOf(got) !== f.cell.join(",")) { broken++; detail.push(`fix ${r.id} (${f.xy}) ${f.layer}`); }
    }
    for (const [key, wasRole] of Object.entries(r.keep)) {
      const [xy, layer] = key.split("@");
      const [x, y] = xy.split(",").map(Number);
      api.setup(r.w, r.h, r.floors);
      if ((api.roleAt(x, y, layer) || null) !== wasRole) { broken++; detail.push(`keep ${r.id} (${key})`); }
    }
  }
  return { broken, detail, sig: detail.join("|") };
}

// node tests/js/mutation_check.js --try '{"find":"...","with":"..."}'
// Applies one ad-hoc mutation and reports whether the gate notices. Used to
// probe for blind spots without editing this file. Exit 0 = caught, 2 = survived.
function tryOne(json) {
  let m;
  try { m = JSON.parse(json); } catch (e) { console.log(`bad --try JSON: ${e.message}`); return 3; }
  if (typeof m.find !== "string" || typeof m.with !== "string") { console.log('--try needs {"find","with"}'); return 3; }
  let api;
  try {
    api = loadRule({ mutate: src => {
      const hits = src.split(m.find).length - 1;
      if (hits !== 1) throw new Error(`pattern matched ${hits} sites, expected exactly 1 (note: template is CRLF, so patterns cannot span lines)`);
      return src.replace(m.find, m.with);
    }});
  } catch (e) { console.log(`NOT APPLIED: ${e.message}`); return 3; }
  const base = evaluate(loadRule());
  const r = evaluate(api);
  if (r.sig !== base.sig) {
    const delta = r.broken - base.broken;
    console.log(`CAUGHT: violation set moves (${base.broken} -> ${r.broken}, ${delta >= 0 ? "+" : ""}${delta})`);
    return 0;
  }
  console.log("SURVIVED: real behaviour changed and not one assertion noticed - this is a blind spot");
  return 2;
}

function main() {
  const ti = process.argv.indexOf("--try");
  if (ti !== -1) return tryOne(process.argv[ti + 1]);

  const base = evaluate(loadRule());
  if (base.broken) {
    console.log(DIM(`
  note: the rule currently violates ${base.broken} assertions (see test_wall_rule.js).`));
    console.log(DIM("  A mutation counts as caught when it MOVES the violation set, not when it creates one."));
  }

  const survivors = [];
  console.log("");
  for (const m of MUTATIONS) {
    let api, err = null;
    try {
      api = loadRule({
        mutate: src => {
          const hits = src.split(m.find).length - 1;
          if (hits !== 1) throw new Error(`pattern matched ${hits} sites, expected exactly 1`);
          return src.replace(m.find, m.with);
        },
      });
    } catch (e) { err = e; }

    if (err) {
      // A mutation that cannot be applied is a broken test, not a caught bug.
      console.log(RED(`  ?? ${m.name}`) + DIM(` - could not apply: ${err.message}`));
      survivors.push({ ...m, unapplied: true });
      continue;
    }
    const r = evaluate(api);
    if (r.sig !== base.sig) {
      const d = r.broken - base.broken;
      console.log(GRN(`  caught   `) + m.name + DIM(`  (violations ${base.broken} -> ${r.broken})`));
      if (VERBOSE) for (const d of r.detail.slice(0, 4)) console.log(DIM(`             ${d}`));
    } else {
      console.log(RED(`  SURVIVED `) + m.name);
      console.log(DIM(`             ${m.why}`));
      survivors.push(m);
    }
  }

  const caught = MUTATIONS.length - survivors.length;
  console.log(`\nmutation coverage  ${caught}/${MUTATIONS.length} caught`);
  if (survivors.length) {
    console.log(RED(`\n  ${survivors.length} mutation${survivors.length > 1 ? "s" : ""} survived - the suite has blind spots there.`));
    console.log(DIM("  Real behaviour changed and every assertion still passed.\n"));
    return 1;
  }
  console.log(GRN("\nthe gate bites\n"));
  return 0;
}

process.exit(main());
