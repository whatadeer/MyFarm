// The wall rule EXACTLY as it rendered when the user reviewed round 3 - the
// picture they were actually looking at when they marked cells wrong and
// approved others.
//
// Recovered verbatim from this session's transcript: the Edit that replaced the
// rule recorded the previous version as its old_string. NOT reconstructed from
// memory or notes.
//
// VERIFIED: reproduces 62/62 of the roles corrections_round3.js records the user
// as having SEEN. That exactness is the whole basis for using it as truth.
//
// WHY THIS EXISTS
// A cell the user did NOT mark, in a scenario they DID review, is an implicit
// "this one is fine". Those unmarked cells outnumber the marked ones ~40:1, and
// without this baseline there is no way to tell whether a rule change moved
// something nobody asked to move. Measuring only marked cells is exactly how the
// landed rule scored 56/61 while silently changing 421 unmarked cells - it fit
// the labels and wrecked everything around them.
//
// Expressed as reverse-patches against the CURRENT template rather than a frozen
// copy of the page, so it cannot rot into a stale duplicate. If a patch stops
// matching, toBaseline throws instead of quietly producing a wrong baseline.
"use strict";
const fs = require("fs");
const path = require("path");

const read = f => fs.readFileSync(path.join(__dirname, f), "utf8").replace(/\r\n/g, "\n");

// The full wbRoleForCell + wbOverlayRole block, before and after the round-3 landing.
const REVIEWED_RULE = read("reviewed_rule.txt");
const LANDED_RULE = read("landed_rule.txt");

const PATCHES = [
  [LANDED_RULE, REVIEWED_RULE, "wbRoleForCell + wbOverlayRole"],
  ['else if (add & 2) role = variant ? "ccne2" : "ccne";',
   'else if (add & 2) role = variant ? "ccne2" : (isOpen(x + 1, y - 1) ? "fcl" : "ccne");',
   "computeWallCell: east corner-cap (thin-wall fcl case)"],
  ['else role = variant ? "ccnw2" : "ccnw";',
   'else role = variant ? "ccnw2" : (isOpen(x - 1, y - 1) ? "fcr" : "ccnw");',
   "computeWallCell: west corner-cap (thin-wall fcr case)"],
];

/**
 * Pass to loadRule({ mutate: toBaseline }) to get the rule as the user reviewed it.
 *
 * Idempotent: the template currently IS the baseline (the overfit rule was
 * reverted out of it), so every patch is already applied and this is a no-op.
 * It stays here because the moment the rule is changed again, the keeps in
 * wall_cases.json have to be regenerated from the render the user actually
 * reviewed - not from whatever we just wrote.
 */
function toBaseline(src) {
  let s = src.replace(/\r\n/g, "\n");
  for (const [from, to, what] of PATCHES) {
    const already = s.split(to).length - 1;
    const n = s.split(from).length - 1;
    if (n === 0 && already >= 1) continue;          // already at baseline
    if (n !== 1) {
      throw new Error(`baseline patch "${what}" matched ${n} sites (and the target ` +
                      `text appears ${already} times) - the template drifted in a way ` +
                      `this baseline cannot describe, so it is no longer valid`);
    }
    s = s.replace(from, to);
  }
  return s;
}

module.exports = { toBaseline, PATCHES, REVIEWED_RULE, LANDED_RULE };
