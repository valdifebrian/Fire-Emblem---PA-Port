#!/usr/bin/env node
// dbx.mjs — FE9 /zdbx.cmp *.dbx (SJIS key-value text, records separated by ¥) -> UTF-8 JSON.
// Format (BRV-ANM-003): '#' comments; blank lines; record terminator = '¥' (fullwidth yen).
// Records: { \n key<TAB/SP>value \n ... }  — value: raw string (trim trailing WS), lists comma-sep.
// Usage: dbx.mjs convert <file.dbx ...> [--out dir] [--num]   (default out: alongside as .json)
//        dbx.mjs show <file.dbx>
import fs from "node:fs";
import path from "node:path";

export function sjisDecode(buf) {
  const dec = new TextDecoder("shift_jis");
  return dec.decode(buf);
}

export function dbxParse(text) {
  const recs = [];
  let cur = null;
  for (let raw of text.split("\n")) {
    const line = raw.replace(/\r$/, "");
    const t = line.trim();
    if (t.startsWith("#") || t === "") continue;
    if (t.includes("¥") || t === "¥") {
      // '¥' can appear mid-line only as terminator; split just in case
      const parts = t.split("¥");
      for (let i = 0; i < parts.length; i++) {
        const p = parts[i].trim();
        if (p === "" ) { if (cur) { recs.push(cur); cur = null; } continue; }
        // content before ¥ on same line is treated as a line in cur (defensive)
        const m = p.match(/^(\S+)[ \t]+(.*)$/);
        if (m && cur) cur[m[1]] = castVal(m[2]);
        else if (cur && p !== "{" && p !== "}") cur._extra = (cur._extra || "") + p;
      }
      continue;
    }
    if (t === "{") { cur = {}; continue; }
    if (t === "}") { if (cur) { recs.push(cur); cur = null; } continue; }
    const m = t.match(/^(\S+)[ \t]+(.*)$/);
    if (m && cur) cur[m[1]] = castVal(m[2].replace(/[ \t]+$/, ""));
    else if (cur) cur[t] = true;
  }
  if (cur) recs.push(cur);
  return recs;
}

function castVal(v) {
  if (/^-?\d+$/.test(v)) return parseInt(v, 10);
  if (/^-?\d*\.\d+(e[-+]?\d+)?$/i.test(v)) return parseFloat(v);
  if (/^-?0[xX][0-9a-fA-F]+$/.test(v)) return parseInt(v, 16);
  if (v.includes(",")) {
    const parts = v.split(",").map((s) => s.trim()).filter((s) => s !== "");
    if (parts.length && parts.every((s) => /^-?\d+(\.\d+)?$/.test(s)))
      return parts.map(Number);
    return v;
  }
  return v;
}

const args = process.argv.slice(2);
const mode = args[0] ?? "show";
const files = [];
let optName = null;
const opts = {};
for (const a of args.slice(1)) {
  if (optName) { opts[optName] = a; optName = null; continue; }
  if (a.startsWith("--")) optName = a.slice(2);
  else files.push(a);
}
for (const f of files) {
  const text = sjisDecode(fs.readFileSync(f));
  const recs = dbxParse(text);
  if (mode === "show") {
    console.log(`== ${f}: ${recs.length} records`);
    for (const r of recs.slice(0, 3)) console.log(JSON.stringify(r));
  } else if (mode === "convert") {
    const outRoot = opts.out ? opts.out : path.dirname(f);
    const sub = path.dirname(f).replace(/^.*?out[/\\]cmp[/\\]?/, "");
    const out = path.join(outRoot, sub);
    fs.mkdirSync(out, { recursive: true });
    const dest = path.join(out, path.basename(f).replace(/\.dbx$/i, "") + ".json");
    fs.writeFileSync(dest, JSON.stringify(recs, null, opts.compact ? 0 : 1));
    console.log(`${f} -> ${dest} (${recs.length} recs)`);
  }
}
