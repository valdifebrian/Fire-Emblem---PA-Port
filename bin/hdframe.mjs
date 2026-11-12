// hdframe.mjs — harvest frame video HD FE9 utk behavior-mining.
// Pakai (coarse scan, background):
//   node bin/hdframe.mjs scan --step 60 [--start 0] [--end SEC] [--dir out/vid/coarse] [--w 320]
//   node bin/hdframe.mjs shot TSec OUT.png [--w 1280]     # frame tunggal res penuh
//   node bin/hdframe.mjs sheet --dir out/vid/coarse [--cols 10] [--rows 9] [--out out/vid/sheets]
// Sheet: cell (r*cols+c) pada sheet s => ts = (s*cols*rows + r*cols + c)*step (step ditulis di stderr manifest)
import { execFileSync } from "child_process";
import fs from "fs";
import path from "path";

const VID = "C:/Kodingan/Fire Emblem： Path of Radiance - Full Game in HD! [Vf53jqrC3L8] 720p.mp4";
const argv = process.argv.slice(2);
const mode = argv[0];
const opt = (k, d) => { const i = argv.indexOf(k); return i >= 0 ? argv[i + 1] : d; };

if (mode === "scan") {
  const step = parseInt(opt("--step", "60")), start = parseInt(opt("--start", "0")), end = parseInt(opt("--end", "78890"));
  const dir = opt("--dir", "out/vid/coarse"), w = parseInt(opt("--w", "320"));
  fs.mkdirSync(dir, { recursive: true });
  let n = 0;
  for (let t = start; t <= end; t += step) {
    const outP = path.join(dir, `f${String(n).padStart(4, "0")}.jpg`);
    if (!fs.existsSync(outP)) {
      try {
        execFileSync("ffmpeg", ["-v", "error", "-ss", String(t), "-i", VID, "-frames:v","1", "-q:v", "6", "-vf", `scale=${w}:-1`, "-y", outP], { stdio: ["ignore", "ignore", "pipe"] });
      } catch (e) { console.error("fail@" + t, String(e.stderr || e).slice(0, 120)); }
    }
    n++;
    if (n % 50 === 0) console.error(`scan ${n} frames (t=${t})`);
  }
  fs.writeFileSync(path.join(dir, "manifest.txt"), `step=${step}\nstart=${start}\nframes=${n}\n`);
  console.log("scan done", n);
} else if (mode === "shot") {
  const t = argv[1], outP = argv[2], w = parseInt(opt("--w", "1280"));
  execFileSync("ffmpeg", ["-v", "error", "-ss", t, "-i", VID, "-frames:v","1", "-vf", `scale=${w}:-1`, "-y", outP], { stdio: ["inherit", "inherit", "inherit"] });
  console.log("shot", t, outP);
} else if (mode === "sheet") {
  const dir = opt("--dir", "out/vid/coarse"), cols = parseInt(opt("--cols", "10")), rows = parseInt(opt("--rows", "9"));
  const outDir = opt("--out", "out/vid/sheets");
  fs.mkdirSync(outDir, { recursive: true });
  const files = fs.readdirSync(dir).filter((f) => /^f\d+\.jpg$/.test(f)).sort();
  const listP = path.join(dir, "list.txt");
  fs.writeFileSync(listP, files.map((f) => `file '${f.replace(/'/g, "'\\''")}'`).join("\n"));
  execFileSync("ffmpeg", ["-v", "error", "-f", "concat", "-safe", "0", "-i", listP, "-vf", `tile=${cols}x${rows}`, "-fps_mode", "passthrough", "-q:v", "5", "-y", path.join(outDir, "s%02d.jpg")], { stdio: ["ignore", "ignore", "pipe"] });
  const sheets = fs.readdirSync(outDir).filter((f) => f.endsWith(".jpg")).length;
  console.log("sheets", sheets, outDir, `per-sheet=${cols * rows}`);
} else console.log("mode? scan|shot|sheet");
process.exit(0);
