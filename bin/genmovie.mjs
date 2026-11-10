import fs from "node:fs";
// DTM v-current: header 256B pack(1) + per-poll ControllerState 8B
// ControllerState: [0]=Start|A<<1|B<<2|X<<3|Y<<4|Z<<5|Up<<6|Down<<7
//                  [1]=Left|R<<2... => Left=bit0,Right=bit1,L=bit2,R=bit3,disc=4,reset=5,conn=6,origin=7
//                  [2]=TrigL [3]=TrigR [4]=StickX [5]=StickY [6]=CX [7]=CY
const HZ = 60, SECONDS = 300;
const polls = HZ * SECONDS;
const input = Buffer.alloc(polls * 8);
function set(i, { start = 0, a = 0, b = 0, x = 0, down = 0, up = 0, left = 0, right = 0 } = {}, first = false) {
  const o = i * 8;
  input[o] = (start ? 1 : 0) | (a ? 2 : 0) | (b ? 4 : 0) | (x ? 8 : 0) | (down ? 128 : 0) | (up ? 64 : 0);
  input[o + 1] = 0x40 | (left ? 1 : 0) | (right ? 2 : 0) | (first ? 0x80 : 0); // connected (+origin reset first frame)
  input[o + 2] = 0; input[o + 3] = 0;
  input[o + 4] = 0x80; input[o + 5] = 0x80; input[o + 6] = 0x80; input[o + 7] = 0x80;
}
for (let i = 0; i < polls; i++) set(i, {}, i === 0);
function press(i0, dur, keys) { for (let i = i0; i < Math.min(i0 + dur, polls); i++) set(i, keys, false); }
// boot+attract: idle till 95s; tap START every 2s from 95s..180s (title/attract -> menu)
for (let t = 95 * HZ; t < 180 * HZ; t += 120) press(t, 8, { start: 1 });
// from 185s: down-nav + confirm attempts (menu: Continue/New/Event)
for (let t = 185 * HZ; t < 270 * HZ; t += 300) {
  press(t, 8, { down: 1 });
  press(t + 40, 8, { start: 1 });
}
const hdr = Buffer.alloc(256);
hdr.set([0x44, 0x54, 0x4d, 0x1a], 0);           // DTM 0x1A
hdr.write("GFEE01", 4, "ascii");                 // gameID
hdr[10] = 0;                                     // bWii=false
hdr[11] = 0x01;                                  // GC pad 1 present
hdr[12] = 0;                                     // bFromSaveState=false
hdr.writeBigUInt64LE(BigInt(polls), 13);         // frameCount
hdr.writeBigUInt64LE(BigInt(polls), 21);         // inputCount
hdr.writeBigUInt64LE(0n, 29);                    // lagCount
hdr.writeBigUInt64LE(0n, 37);                    // uniqueID
hdr.writeUInt32LE(0, 45);                        // numRerecords
hdr.write("agent", 49, "ascii");                 // author
// md5[16] at 113 = zeros -> checksum check skipped
hdr.writeBigUInt64LE(BigInt(Math.floor(Date.now() / 1000)), 129); // recordingStartTime
hdr[137] = 0;                                    // bSaveConfig=false -> use current config
hdr[151] = 0x03;                                 // memcards A+B inserted (match config)
hdr.writeBigUInt64LE(1_500_000_000_000n, 237);   // tickCount huge: never auto-end
fs.writeFileSync("fe9-re/out/agent_boot.dtm", Buffer.concat([hdr, input]));
console.log("dtm:", 256 + input.length, "bytes,", polls, "polls,", SECONDS, "s");
