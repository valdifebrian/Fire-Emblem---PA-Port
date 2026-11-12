// flagmine.mjs - static inventory of set/get/clr/regist(name) script-flag sites across out/cmb/C*.cmb.json (BRV-SCT-005m)
import fs from "fs";
const sjis = new TextDecoder("shift_jis");
const dec = s => { try { return sjis.decode(Buffer.from(s, "latin1")); } catch { return s; } };
const dir = "out/cmb";
const out = new Map(); // flag -> {sets,get,clr,regist,cmb Set}
let sites = 0;
for (const file of fs.readdirSync(dir).filter(f => /^C\d+\.cmb\.json$/.test(f))) {
  const j = JSON.parse(fs.readFileSync(`${dir}/${file}`, "utf8"));
  for (const fn of j.funcs || []) {
    let last1 = "", last2 = "";
    for (const [, , op, str] of fn.ins || []) {
      if (op.startsWith("PUSH_POOL")) { last2 = last1; last1 = dec((str || "").replace(/^"|"$/g, "")); continue; }
      if (!op.startsWith("CALL")) continue;
      const kw = /^(set|get|clr|regist)$/.test((str || "").replace(/^"|"$/g, "")) ? str.replace(/^"|"$/g, "") : null;
      if (!kw) continue;
      const name = last1;
      if (!name || /^CALL|^JNZ|^DROP/.test(name)) continue;
      sites++;
      let e = out.get(name);
      if (!e) out.set(name, e = { set: 0, get: 0, clr: 0, regist: 0, cmb: new Set() });
      e[kw]++; e.cmb.add(file.slice(0, 4));
    }
  }
}
const rows = [...out.entries()].sort((a, b) => b[1].regist - a[1].regist || a[0].localeCompare(b[0]));
console.log(`sites=${sites} distinct=${rows.length}`);
for (const [n, e] of rows.slice(0, 60))
  console.log(`${n}\tset=${e.set} get=${e.get} clr=${e.clr} reg=${e.regist}\t${[...e.cmb].join(" ")}`);
fs.writeFileSync("out/data/script_flags.txt",
  "flag\tset\tget\tclr\tregist\tcmb_count\n" + rows.map(([n, e]) =>
    `${n}\t${e.set}\t${e.get}\t${e.clr}\t${e.regist}\t${e.cmb.size}`).join("\n") + "\n");
console.log("wrote out/data/script_flags.txt");
process.exit(0);
