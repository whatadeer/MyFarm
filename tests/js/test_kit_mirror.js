// The two dungeon kits are the same layout twice: brick sits exactly 10 rows
// above boulder, same sheet, same column (boulder rows 12-19, brick rows 2-9).
//
//   node tests/js/test_kit_mirror.js
//
// The editor's Sync button is built on that invariant - pick a tile for one kit
// and it derives the other. So if a mapping edit ever breaks the offset, Sync
// would quietly write the WRONG tile into corrections, and corrections are the
// spec everything else is derived from. Hence this test.
"use strict";
const { loadRule } = require("./rule_harness.js");

const GRN = s => `\x1b[32m${s}\x1b[0m`;
const RED = s => `\x1b[31m${s}\x1b[0m`;

function main() {
  const api = loadRule();
  const { boulder, brick } = api.mapping;
  const cc = api.counterpartCell;
  const fails = [];
  let shared = 0, trips = 0;

  // 1. Every role the kits share obeys the offset.
  for (const role of Object.keys(boulder)) {
    const b = boulder[role], k = brick[role];
    if (!k) continue;
    shared++;
    if (b[0] !== k[0] || b[1] !== k[1] || b[2] - k[2] !== 10) {
      fails.push(`${role}: boulder [${b}] vs brick [${k}] - offset is ${b[2] - k[2]}, expected 10`);
    }
  }

  // 2. counterpartCell agrees with the mapping, both directions, and round-trips.
  for (const role of Object.keys(boulder)) {
    const b = boulder[role], k = brick[role];
    if (!k) continue;
    const toBrick = cc(b, "boulder"), toBoulder = cc(k, "brick");
    trips += 2;
    if (!toBrick || toBrick.join() !== k.join()) fails.push(`${role}: boulder->brick gave [${toBrick}], mapping says [${k}]`);
    if (!toBoulder || toBoulder.join() !== b.join()) fails.push(`${role}: brick->boulder gave [${toBoulder}], mapping says [${b}]`);
    const back = toBrick && cc(toBrick, "brick");
    if (!back || back.join() !== b.join()) fails.push(`${role}: round trip lost it - [${b}] -> [${toBrick}] -> [${back}]`);
  }

  // 3. It must REFUSE where there is no twin, rather than invent one. Sync
  //    offering a bogus tile is worse than offering nothing.
  const refusals = [
    [[2, 1, 2], "boulder", "a cell on another sheet (Rails) - only the Walls sheet has two kits"],
    [[0, 5, 21], "boulder", "a row below the boulder band"],
    [[0, 5, 0], "brick", "a row above the brick band"],
    [[0, 5, 3], "boulder", "a brick-band row claimed as boulder (would land on row -7)"],
  ];
  for (const [cell, kit, why] of refusals) {
    const got = cc(cell, kit);
    if (got !== null) fails.push(`[${cell}] as ${kit} should have no twin (${why}), got [${got}]`);
  }

  console.log(`\nkit mirror  ${shared} shared roles, ${trips} conversions, ${refusals.length} refusals`);
  if (fails.length) {
    console.log(RED(`\n  ${fails.length} failure${fails.length > 1 ? "s" : ""}:`));
    for (const f of fails) console.log(RED(`    x ${f}`));
    console.log(RED("\n  Sync would write wrong tiles into corrections.\n"));
    return 1;
  }
  console.log(GRN("ok - brick is boulder shifted 10 rows, without exception\n"));
  return 0;
}

process.exit(main());
