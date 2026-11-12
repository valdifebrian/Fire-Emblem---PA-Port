// mapbin.mjs - parse map.bin named-chunk containers (/zmap/<ch>/map.cmp -> LZ77 -> pack -> map.bin)
// and FE8Data.bin-style registry bins. Format (loader FUN_8003f87c, consumer FUN_80044450):
//   hdr:      [u32 fileSize][u32 ptrTableOff][u32 ptrCount][u32 dirCount][u32 ?] at file 0
//   data base: +0x20 (all table/chunk offsets relative to it; for FE8Data.bin extracted
//              flat, base = 0). reloc table at data+ptrTableOff: ptrCount u32 offsets whose
//              target u32 gets += data (file offsets -> RAM ptrs).
//   dir:      right after table: dirCount x [u32 dataOff][u32 nameOff] (registry order, NOT sorted)
//   pool:     at dir end; name string = pool + nameOff.
//   chunks (map.bin): mapcapacity[W u16,H u16,nInst u16,...], mapbuilddesc(nInst x 0x20:
//              f32 pos x3 + ptr model name @+0x1c), mapbuildinst, panelindex(u16 W*H idx->uniquepanel),
//              panelindexBase/uniquepanel(12B: [u16 q0..q3][u32 namePtr -> terrain-name string]),
//              uniquepanelBase, mapgrid/mapfog/maplight/mapextra(camera), maplinkAt(grid links).
//   terrain id: FUN_8005b7a4(byte*) walks string: h = (h+c)*0x10 per char (== packed ASCII u32 for
//              <=4 chars), linear-scan 77-entry hash table; miss->0; idx 9 or 33 -> 0x1c.
import {readFileSync} from 'node:fs';
import {lz77ExDecode} from './lz77.mjs';

const u32 = (b,o) => b.readUInt32BE(o);
const u16 = (b,o) => b.readUInt16BE(o);
const f32 = (b,o) => b.readFloatBE(o);
const str = (b,o) => { let s=''; for (let i=o; i<b.length && b[i]; i++) s += String.fromCharCode(b[i]); return s; };

export function parseRegistryBin(buf, base = 0x20) {
  const fileSize = u32(buf,0), ptrOff = u32(buf,4), ptrCnt = u32(buf,8), dirCnt = u32(buf,0xc);
  const table = base + ptrOff, dir = table + 4*ptrCnt, pool = dir + 8*dirCnt;
  const reloc = [];
  for (let i = 0; i < ptrCnt; i++) reloc.push(u32(buf, table + i*4));
  const chunks = [];
  for (let i = 0; i < dirCnt; i++)
    chunks.push({ name: str(buf, pool + u32(buf, dir + i*8 + 4)), dataOff: u32(buf, dir + i*8) });
  return { fileSize, base, reloc, chunks };
}

// parse LZ77-compressed map.cmp pack container, return {entries:[{name,data}]}
export function parsePack(dec) {
  if (dec.toString('latin1', 0, 4) !== 'pack') throw new Error('not pack, magic=' + dec.subarray(0,4));
  const cnt = u16(dec, 4);
  let off = 0x10;
  const names = [];
  for (let i = 0; i < cnt; i++) {
    const len = u32(dec, off); off += 4;
    const name = dec.toString('latin1', off, off + len - 1); off += len;
    names.push(name);
  }
  off = (off + 3) & ~3;
  const tableOff = [];
  for (let i = 0; i < cnt; i++) { tableOff.push(u32(dec, off)); off += 8; }
  return names.map((name, i) => ({ name, data: dec.subarray(tableOff[i], i+1<cnt ? tableOff[i+1] : dec.length) }));
}

export function parseMapBin(mb) {
  const r = parseRegistryBin(mb, 0x20);
  const byName = {}; for (const c of r.chunks) byName[c.name] = 0x20 + c.dataOff;
  const cap = byName.mapcapacity;
  const W = u16(mb, cap), H = u16(mb, cap+2), nInst = u16(mb, cap+4);
  const inst = [];
  for (let i = 0; i < nInst; i++) {
    const d = byName.mapbuilddesc + i*0x20;
    inst.push({ model: str(mb, 0x20 + u32(mb, d + 0x1c)), x: f32(mb, d+0x10), y: f32(mb, d+0x14), z: f32(mb, d+0x18) });
  }
  const grid = [];
  for (let y = 0; y < H; y++) { const row = []; for (let x = 0; x < W; x++) row.push(u16(mb, byName.panelindex + (y*W+x)*2)); grid.push(row); }
  const baseGrid = byName.panelindexBase ? [] : null;
  if (baseGrid) for (let y = 0; y < H; y++) { const row = []; for (let x = 0; x < W; x++) row.push(u16(mb, byName.panelindexBase + (y*W+x)*2)); baseGrid.push(row); }
  const offs = r.chunks.map(c => 0x20 + c.dataOff).sort((a,b) => a-b);
  const chunkEnd = (off) => offs.find(x => x > off) ?? u32(mb,0) + 0x20;
  const readPanels = (off, cnt) => {
    const out = [];
    for (let k = 0; k < cnt; k++) {
      const d = off + k*12;
      out.push({ q: [u16(mb,d),u16(mb,d+2),u16(mb,d+4),u16(mb,d+6)], namePtr: u32(mb, d+8) });
    }
    return out;
  };
  const upOff = byName.uniquepanel;
  const panelsUp = upOff ? readPanels(upOff, (chunkEnd(upOff) - upOff) / 12 | 0) : null;
  const upbOff = byName.uniquepanelBase;
  const panelsBase = upbOff ? readPanels(upbOff, (chunkEnd(upbOff) - upbOff) / 12 | 0) : null;
  return { W, H, nInst, inst, grid, baseGrid, panels: panelsUp, panelsBase, chunks: r.chunks, relocCnt: r.reloc.length };
}

export { lz77ExDecode };
