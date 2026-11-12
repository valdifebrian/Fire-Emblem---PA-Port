// natcov.mjs — native semantics coverage vs governance corpus.
// Covered = impl address OR name appears (whole-word) in spec, scoreboard,
// or engine main.cpp. Prints totals + uncovered natives with freq>=MIN.
import fs from "fs";
const MIN = +(process.env.MIN ?? 8);
const rows = fs.readFileSync("out/data/native_catalog.csv", "utf8").split("\n").slice(1).filter(l => l.trim());
const corpus =
  fs.readFileSync("../fe9-port/docs/FE9_BEHAVIOR_SPEC.md", "utf8") +
  fs.readFileSync("../fe9-port/docs/SCOREBOARD.md", "utf8") +
  fs.readFileSync("../fe9-port/poc/app/src/main/cpp/main.cpp", "utf8");
const parsed = rows.map(l => { const c = l.split(","); return { name: c[0], argc: c[1], impl: c[2], calls: +c[3] }; });
parsed.sort((a, b) => b.calls - a.calls);
const word = (hay, needle) => {
  let i = -1;
  while ((i = hay.indexOf(needle, i + 1)) >= 0) {
    const a = i > 0 ? hay[i - 1] : " ", b = hay[i + needle.length] || " ";
    if (!/[A-Za-z0-9_]/.test(a) && !/[A-Za-z0-9_]/.test(b)) return true;
  }
  return false;
};
const miss = [];
let cov = 0;
for (const p of parsed)
  if ((p.impl && corpus.includes(p.impl)) || word(corpus, p.name)) cov++;
  else if (p.calls >= MIN) miss.push(p);
console.log(`total ${parsed.length} covered ${cov} uncovered>=${MIN}: ${miss.length}`);
console.log(miss.map(p => `${p.name}\t${p.calls}\t${p.argc}\t${p.impl}`).join("\n"));
process.exit(0);
