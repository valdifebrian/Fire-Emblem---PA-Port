// DOL (GameCube) -> ELF32 BE PPC converter. ET_EXEC, one PT_LOAD per active DOL section.
// Usage: bun bin/dol2elf.mjs <in.dol> <out.elf>
import { readFileSync, writeFileSync } from "fs";

const [,, inPath, outPath] = process.argv;
if (!inPath || !outPath) { console.error("usage: dol2elf.mjs <in.dol> <out.elf>"); process.exit(2); }
const dol = readFileSync(inPath);
const u = (o) => dol.readUInt32BE(o);
const secs = [];
for (let i = 0; i < 17; i++) {
  const foff = u(i * 4), addr = u(0x48 + i * 4), size = u(0x90 + i * 4);
  if (size > 0) secs.push({ i, foff, addr, size, text: i < 7 });
}
if (!secs.length) throw new Error("no active sections");
const entry = u(0xe0), bssAddr = u(0xd8), bssSize = u(0xdc);
secs.sort((a, b) => a.addr - b.addr);
for (let k = 1; k < secs.length; k++) {
  const p = secs[k - 1], c = secs[k];
  if (c.addr < p.addr + p.size) throw new Error(`overlap ${p.i}@0x${p.addr.toString(16)} vs ${c.i}@0x${c.addr.toString(16)}`);
}
console.log(`sections=${secs.length} entry=0x${entry.toString(16)} bss=0x${bssAddr.toString(16)}+0x${bssSize.toString(16)}`);

// ELF layout: [ehdr 52][phdr*32][payload (DOL bytes copied per section)]
const ehdrSize = 52, phent = 32;
let payload = ehdrSize + secs.length * phent;
payload = (payload + 15) & ~15;
const plan = [];
let cur = payload;
for (const s of secs) { plan.push({ ...s, off: cur }); cur += s.size; cur = (cur + 15) & ~15; }
const total = cur;
const buf = Buffer.alloc(total);

// ELF header (BE32)
buf[0] = 0x7f; buf[1] = 0x45; buf[2] = 0x4c; buf[3] = 0x46; // \x7fELF
buf[4] = 1; buf[5] = 2; buf[6] = 1; buf[7] = 0;             // ELFCLASS32, big-endian, EV_CURRENT
buf.writeUInt16BE(2, 16);          // e_type = ET_EXEC
buf.writeUInt16BE(17, 18);         // e_machine = EM_PPC
buf.writeUInt32BE(1, 20);          // e_version
buf.writeUInt32BE(entry, 24);      // e_entry
buf.writeUInt32BE(ehdrSize, 28);   // e_phoff
buf.writeUInt32BE(0, 32);          // e_shoff (no sections)
buf.writeUInt32BE(0, 36);          // e_flags
buf.writeUInt16BE(ehdrSize, 40);   // e_ehsize
buf.writeUInt16BE(phent, 42);      // e_phentsize
buf.writeUInt16BE(secs.length, 44);// e_phnum
for (let k = 0; k < secs.length; k++) {
  const p = plan[k], o = ehdrSize + k * phent;
  buf.writeUInt32BE(1, o);                 // PT_LOAD
  buf.writeUInt32BE(p.off, o + 4);         // p_offset
  buf.writeUInt32BE(p.addr, o + 8);        // p_vaddr
  buf.writeUInt32BE(p.addr, o + 12);       // p_paddr
  buf.writeUInt32BE(p.size, o + 16);       // p_filesz
  buf.writeUInt32BE(p.size, o + 20);       // p_memsz
  buf.writeUInt32BE(p.text ? 5 : 6, o + 24); // flags: text=R+X, data=R+W
  buf.writeUInt32BE(0x10000, o + 28);      // p_align
  dol.copy(buf, p.off, p.foff, p.foff + p.size);
}
writeFileSync(outPath, buf);
console.log(`wrote ${outPath} (${total} B)`);
