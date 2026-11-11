// cmbflow.mjs — linearize disassembled .cmb funcs into native CALL flows.
// Input: out/cmb/<X>.cmb.json (from cmb.mjs). Output: docs-style CSV
//   file,func,id,argc,seq  with seq = ";"-joined tokens:
//   CALL name(argc) / i:N / s:str / DROP / RET / other-opcode.
import fs from "node:fs";
import path from "node:path";

const dir = process.argv[2] || "out/cmb";
const outFile = process.argv[3] || "out/cmb/FLOWS.csv";
const OP_CALL = 56, OP_CALLV = 58; // per cmb.mjs opcode table
const rows = [];
for (const f of fs.readdirSync(dir).filter((x) => x.endsWith(".cmb.json"))) {
  const j = JSON.parse(fs.readFileSync(path.join(dir, f), "latin1"));
  for (const fn of j.funcs) {
    const toks = [];
    for (const ins of fn.ins || []) {
      const [, op, txt, arg] = ins;
      if (op === OP_CALL || op === OP_CALLV) {
        const nm = (arg || "").replace(/^"|"$/g, "");
        const m = txt.match(/CALL\s+\d+\s+(\d+)/);
        toks.push(`${nm}/${m ? m[1] : "?"}`);
      } else if (txt.startsWith("PUSH8")) toks.push("i" + txt.slice(6).trim());
      else if (txt.startsWith("PUSH16")) toks.push("i" + txt.slice(7).trim());
      else if (txt.startsWith("PUSHD") || txt.startsWith("PUSH32") || txt.startsWith("PUSHF")) toks.push("i" + (arg || "").trim());
      else if (txt.startsWith("PUSH") && arg && /^"/.test(arg)) toks.push("s:" + arg.replace(/^"|"$/g, ""));
      else if (txt.startsWith("DROP")) toks.push("_");
      else if (txt.startsWith("RET")) toks.push("RET");
      else if (!txt.startsWith("DB")) toks.push(txt.split(" ")[0].toLowerCase());
    }
    rows.push([j.name, fn.name || ("f" + fn.id), fn.id, fn.argc, toks.join(";")]);
  }
}
const esc = (s) => /[",\n]/.test(s) ? '"' + s.replace(/"/g, '""') + '"' : s;
fs.writeFileSync(outFile, "file,func,id,argc,seq\n" + rows.map((r) => r.map(esc).join(",")).join("\n") + "\n");
console.log("rows", rows.length, "->", outFile);
