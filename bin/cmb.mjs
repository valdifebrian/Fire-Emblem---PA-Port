// cmb.mjs — FE9 /Scripts/*.cmb container parser + bytecode disassembler (AM-017 P1).
// Evidence (Ghidra PPC decompiles, out/ghidra/dec/):
//  FUN_80013e24 CmLoadScript: swaps BE header fields @0x18/1c/22/24/28 when block
//    flag bit0xe clear; links block chain @+0x1c.
//  FUN_800140b4: reloc — +0x24 (pool off) +0x28 (entries array off) += base;
//    entries = stride-4 LE-u32 terminated by 0; per record: first u32 byte-reversed
//    then += base, then FUN_8000d3ec interns NUL-terminated key bytes.
//  FUN_8000d3ec/8000d1d0: register/deregister in global hash (509 buckets).
//  FUN_80013a90: call script func BY NAME via FUN_8000d354 lookup (err 5 = not found).
//  FUN_800c28ec ScriptDemoMain: LoadScript("scripts/demo.cmb") → call "ScriptDemoMain".
//  FUN_800138f4 pump: frame = [PC@+0x0C, rec@+0x08, L@+0x14, SP@+0x18, argc@+0x0D,
//    nLoc@+0x12, yield@+0x1C, globOff@+0x20]; call FUN_80011edc(frame).
//  FUN_80011edc interpreter: switch(*(u8*)PC) — 64 single-byte opcodes; operands
//    u8 / u16-BIG-ENDIAN / u32-BIG-ENDIAN (CONCAT11 pairs). Locals L = u32[] at
//    frame+0x14; globals G = u32[] at *(r13-0x7c08). FUN_80011d20 = op 0x38:
//    u16-BE pool offset → NUL name → hash lookup; if native (rec+8==0) run native
//    trampoline, else push script frame. FUN_800056cc = op 0x41 native call,
//    target = top-of-stack pointer, operand = fixed arg count 0..9.
// STATUS: container + interpreter SEMANTICS DECODED (all 64 cases). Record header
//  fields +0x0d argc(i8) / +0x12 nLocals(i16) / +0x20 globOff(u16) confirmed vs pump.
import { readFile, writeFile, mkdir } from "fs/promises";
import { openRvz, readFST, walkFiles } from "./rvz.mjs";

// opcode → [mnemonic, ...operand kinds]; i8 signed, u8, i16/u16/u32 BIG-ENDIAN.
const OPS = new Map(Object.entries({
  0x01: ["PUSH_L", "u8"], 0x02: ["PUSH_L", "u16"],
  0x03: ["LOAD_L_OFF", "u8"], 0x04: ["LOAD_L_OFF", "u16"],      // pop n; L[off+n]
  0x05: ["LOADIDX_L", "u8"], 0x06: ["LOADIDX_L", "u16"],        // pop n; *(L[off]+n*4)
  0x07: ["PUSHADDR_L", "i8"], 0x08: ["PUSHADDR_L", "u16"],
  0x09: ["ADDR_L_IDX", "i8"], 0x0a: ["ADDR_L_IDX", "u16"],      // pop n; &L[off]+n*4
  0x0b: ["LOAD_L_VAL", "u8"], 0x0c: ["LOAD_L_VAL", "u16"],      // pop n; L[off]+n*4
  0x0d: ["PUSH_G", "u8"], 0x0e: ["PUSH_G", "u16"],
  0x0f: ["LOAD_G_OFF", "u8"], 0x10: ["LOAD_G_OFF", "u16"],
  0x11: ["LOADIDX_G", "u8"], 0x12: ["LOADIDX_G", "u16"],
  0x13: ["PUSHADDR_G", "i8"], 0x14: ["PUSHADDR_G", "u16"],
  0x15: ["ADDR_G_IDX", "i8"], 0x16: ["ADDR_G_IDX", "u16"],
  0x17: ["LOAD_G_VAL", "u8"], 0x18: ["LOAD_G_VAL", "u16"],
  0x19: ["PUSH8", "i8"], 0x1a: ["PUSH16", "i16"], 0x1b: ["PUSH32", "i32"],
  0x1c: ["PUSH_POOL", "i8"], 0x1d: ["PUSH_POOL", "u16"], 0x1e: ["PUSH_POOL", "u32"],
  0x1f: ["DUP"], 0x20: ["DROP"], 0x21: ["OVER"],
  0x22: ["ADD"], 0x23: ["SUB"], 0x24: ["MUL"], 0x25: ["DIV"], 0x26: ["MOD"],
  0x27: ["NEG"], 0x28: ["BNOT"], 0x29: ["NOT"],
  0x2a: ["BOR"], 0x2b: ["BAND"], 0x2c: ["BXOR"], 0x2d: ["SHL"], 0x2e: ["SHR"],
  0x2f: ["EQ"], 0x30: ["NE"], 0x31: ["CMP_S31"], 0x32: ["LE"],
  0x33: ["CMP_S33"], 0x34: ["LT"],
  0x35: ["STR_EQ"], 0x36: ["STR_NE"],
  0x37: ["CALL_LOCAL", "u8"],                                       // entry-array index
  0x38: ["CALL", "u16", "u8"],                                      // [poolOff BE][argc]
  0x39: ["RET"],
  0x3a: ["JMP", "i16"], 0x3b: ["JZ", "i16"], 0x3c: ["JZ_PUSH1", "i16"],
  0x3d: ["JNZ", "i16"], 0x3e: ["JNZ_PUSH0", "i16"],
  0x3f: ["YIELD"], 0x40: ["SKIP4"],
  0x41: ["CALLN", "u8"],                                            // fn ptr on stack
}).map(([k, v]) => [Number(k), v]));

const W = { u8: 1, i8: 1, u16: 2, i16: 2, u32: 4, i32: 4 };

// Read pool offset operand as signed (engine sign-extends i8; u16/u32 unsigned).
function poolName(poolBytes, off) {
  if (off < 0 || off >= poolBytes.length) return null;
  let j = off; while (j < poolBytes.length && poolBytes[j] !== 0) j++;
  return poolBytes.toString("latin1", off, j);
}

export function disasm(code, codeOff, poolBytes) {
  const out = [];
  let i = 0;
  while (i < code.length) {
    const op = code[i];
    const e = OPS.get(op);
    const at = (codeOff + i).toString(16).padStart(4, "0");
    if (!e) { out.push([at, op, "DB", [...code.subarray(i, i + 1)].map((b) => b.toString(16)).join(" ")]); i += 1; continue; }
    const [mn, ...kinds] = e;
    let j = i + 1; const vals = [], txt = [];
    let bad = false;
    for (const k of kinds) {
      if (j + W[k] > code.length) { bad = true; break; }
      let v;
      if (k === "u8") v = code[j];
      else if (k === "i8") v = code.readInt8(j);
      else if (k === "u16") v = code.readUInt16BE(j);
      else if (k === "i16") v = code.readInt16BE(j);
      else if (k === "u32") v = code.readUInt32BE(j);
      else v = code.readInt32BE(j);
      vals.push(v);
      if (k === "i16" || k === "u16") txt.push((v + (k === "i16" ? 0 : 0)).toString());
      j += W[k];
    }
    if (bad) { out.push([at, op, "DB", "truncated"]); i += 1; continue; }
    let cmt = "";
    if (mn === "PUSH_POOL" || (mn === "CALL")) {
      const nm = poolName(poolBytes, vals[0]);
      if (nm) cmt = '"' + nm + '"';
    }
    if (mn === "CALL_LOCAL") cmt = `entry[${vals[0]}]`;
    let label = mn;
    if (vals.length) label += " " + vals.join(" ");
    if (["JMP", "JZ", "JZ_PUSH1", "JNZ", "JNZ_PUSH0"].includes(mn)) {
      const tgt = codeOff + i + vals[0]; // rel from op start (engine: pc-2 after read)
      label += ` -> 0x${tgt.toString(16)}`;
    }
    out.push([at, op, label, cmt]);
    i = j;
  }
  return out;
}

export function parseCmb(buf, name = "?") {
  if (buf.toString("latin1", 0, 3) !== "cmb") throw new Error(name + ": not cmb");
  const u32 = (o) => buf.readUInt32LE(o), u16 = (o) => buf.readUInt16LE(o);
  const selfname = buf.toString("latin1", 4, buf.indexOf(0, 4));
  const poolOff = u32(0x24), entOff = u32(0x28);
  // pool = poolOff .. entOff (raw bytes; names resolved lazily by offset)
  const poolBytes = buf.subarray(poolOff, entOff);
  const entries = [];
  for (let e = entOff; e + 4 <= buf.length; e += 4) { const v = u32(e); if (!v) break; entries.push(v); }
  // all known pointers (name & code) bound code spans
  const ptrs = [];
  for (const off of entries) { ptrs.push(off, u32(off), u32(off + 4)); }
  ptrs.sort((a, b) => a - b);
  const funcs = [];
  for (const off of entries) {
    if (off < 0x28 || off + 0x14 > buf.length) { (funcs.bad ||= []).push(off); continue; }
    const nameOff = u32(off), codeOff = u32(off + 4);
    if (codeOff <= 0 || codeOff >= buf.length) { (funcs.bad ||= []).push(off); continue; }
    let end = buf.length;
    for (const p of ptrs) { if (p > codeOff) { end = Math.min(end, p); break; } }
    let fname = null;
    if (nameOff && nameOff < buf.length) { let j = nameOff; while (buf[j] >= 0x20 && buf[j] < 0x7f) j++; fname = buf.toString("latin1", nameOff, j); }
    const rec = {
      name: fname || `#${off.toString(16)}`, off, nameOff, codeOff,
      u8f: u16(off + 8), uAf: u16(off + 0xa),
      flagC: buf[off + 0xc], argc: buf.readInt8(off + 0xd),
      uE: buf[off + 0xe], id: u16(off + 0x10), nLoc: u16(off + 0x12),
    };
    const code = buf.subarray(codeOff, end);
    const ins = disasm(code, codeOff, poolBytes);
    for (const l of ins) { if (l[2].startsWith("CALL ") && l[3]) { (rec.calls ||= []).push(l[3].slice(1, -1)); } }
    rec.ins = ins;
    funcs.push(rec);
  }
  return { name, selfname, size: buf.length, poolOff, poolBytes: poolBytes.toString("latin1").split("\0").filter(Boolean), funcs };
}

const mode = process.argv[2];
const { pathToFileURL } = await import("url");
const isMain = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (mode === "json" && process.argv[3]) {
  for (const f of process.argv.slice(3)) {
    const c = parseCmb(await readFile(f), f.split("/").pop());
    await writeFile(f + ".json", JSON.stringify(c, null, 1));
    const calls = new Set(c.funcs.flatMap((x) => x.calls || []));
    console.log(`${c.name}: self=${c.selfname} pool=${c.poolBytes.length} funcs=${c.funcs.length} calls=${calls.size}`);
  }
} else if (mode === "dis" && process.argv[3]) {
  const c = parseCmb(await readFile(process.argv[3]), process.argv[3].split("/").pop());
  const fi = parseInt(process.argv[4] || "0", 10);
  const rec = c.funcs[fi];
  console.log(`; ${c.selfname} func[${fi}] "${rec.name}" @0x${rec.codeOff.toString(16)} argc=${rec.argc} nLoc=${rec.nLoc}`);
  for (const [a, op, l, cm] of rec.ins) console.log(`${a}: ${op.toString(16).padStart(2, "0")}  ${l}${cm ? "   ; " + cm : ""}`);
} else if (mode === "all") {
  const rvzPath = process.env.FE9_RVZ;
  if (!rvzPath) throw new Error("FE9_RVZ not set");
  const rvz = await openRvz(rvzPath);
  const gc = Buffer.from(await rvz.read(0, 0x440));
  const root = await readFST(rvz, gc.readUInt32BE(0x424), gc.readUInt32BE(0x428));
  const files = (await walkFiles(root, "", [])).filter((f) => /^\/Scripts\/.*\.cmb$/i.test(f.path)).sort();
  await mkdir("out/cmb", { recursive: true });
  const rows = ["script,size,funcs,imports,called_names_unique"];
  const allCalls = new Map();
  for (const f of files) {
    const data = Buffer.from(await rvz.read(f.pos, f.size));
    const c = parseCmb(data, f.path);
    await writeFile(`out/cmb/${c.selfname}.json`, JSON.stringify(c));
    const calls = [...new Set(c.funcs.flatMap((x) => x.calls || []))].sort();
    for (const cc of calls) allCalls.set(cc, (allCalls.get(cc) || 0) + 1);
    rows.push([c.selfname, c.size, c.funcs.length, c.poolBytes.length, `"${calls.join("|")}"`].join(","));
    console.log(`${c.selfname}: ${c.funcs.length}f ${c.poolBytes.length}imp ${calls.length}names`);
  }
  await writeFile("out/cmb/SCRIPTS_SUMMARY.csv", rows.join("\n") + "\n");
  await writeFile("out/cmb/ALL_CALLED_NAMES.csv", [...allCalls.entries()].sort((a, b) => b[1] - a[1]).map((e) => `${e[1]},${e[0]}`).join("\n") + "\n");
  console.log(`-> out/cmb/ (${files.length} scripts, ${allCalls.size} distinct called names)`);
} else if (isMain) console.error("usage: cmb json <files...> | dis <file> [funcIdx] | all (FE9_RVZ=)");
