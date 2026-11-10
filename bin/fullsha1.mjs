import { createHash } from "node:crypto";
import { openRvz } from "./rvz.mjs";
const r = await openRvz(process.argv[2]);
const h = createHash("sha1");
const t0 = Date.now();
for (let ci = 0; ci < r.numG; ci++) {
  const b = await r.read(ci * r.chunk, Math.min(r.chunk, r.isoSize - ci * r.chunk));
  h.update(b);
  if (ci % 1000 === 0) console.log(ci, "/", r.numG, ((Date.now()-t0)/1000).toFixed(0)+"s");
}
console.log("ISO_SHA1", h.digest("hex"), "bytes", h.reading ?? 0);
