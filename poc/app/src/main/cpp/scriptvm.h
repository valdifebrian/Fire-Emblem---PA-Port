/* scriptvm.h — FE9 `.cmb` script VM, byte-exact port of the decompiled
 * interpreter FUN_80011edc (+ FUN_80011d20 name call, FUN_80011a5c native
 * trampoline, FUN_800138f4 entry, FUN_800056cc CALLN). [BRV-SCT-005][P3-M3.1]
 *
 * DECISION [BRV-SCT-005]: the runtime executes RAW .cmb bytes (same bytes the
 * GameCube loader consumes); the JSON view of cmb.mjs stays an offline RE
 * artifact. "chapter content = data" with zero conversion drift.
 *
 * Record layout (file LE; runtime identical — loader patches +0x04 and interns
 * the name; natives: +0x04 impl, +0x08 = 0):
 *   +0x00 u32 nameOff(pool)  +0x04 u32 codeOff   +0x08 u16  +0x0A u16
 *   +0x0C u8  +0x0D i8 argc  +0x0E u8  +0x10 u16 id  +0x12 i16 nLoc
 *   +0x20 u16 globOff        +0x24 u32 poolOff
 * Frame model (per thread): ONE u32 stack in the VM ram arena; a call places
 * the callee directly over the caller's pushed args:
 *   lbase = sp - argc; zero L[argc..nLoc); SP = lbase + nLoc; then push
 *   [pc, rec+1, gbase+1, lbase] as the save frame (these are L[nLoc..nLoc+4),
 *   i.e. SP ends at lbase+nLoc+4). RET: res=pop; read save at lbase+nLoc;
 *   sp=lbase (result overwrites args[0] slot region); push res.
 *   G = GBASE + globOff (per-cmb window in ram; u32 cells)
 * Opcodes verified case-by-case against FUN_80011edc.c (2026-11-12):
 *   jumps: target = (pc_after_operand) + disp - 2   (operand-relative)
 *   JZ/JNZ POP test; JZ_PUSH1 pops, if nonzero push1+branch; JNZ_PUSH0 pops,
 *   if nonzero push0+branch (lazy && / || pair); CMP_S31/S33 exact formula;
 *   YIELD: pause slice, resume at pc-after-operand; CALLN(u8 n): args[0]=
 *   handle (native registry index+1); CALL(0x38): u16-BE pool name + u8 argc.
 * Native trampoline (FUN_80011a5c): SP-=argc; impl(args[0..argc)); args[0]
 *   slot overwritten with return value; SP = args+1. Natives MAY yield
 *   (WaitM): engine re-invokes SAME native with saved args until it stops.
 * Memory: all data pointers are byte addresses into Vm::ram (arena). The
 * engine MUST pass arena addresses through natives, never host pointers.
 */
#ifndef FE9_SCRIPTVM_H
#define FE9_SCRIPTVM_H
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm {

static const uint32_t RAM_SIZE   = 16u << 20; /* 16 MiB script ram      */
static const uint32_t STACK_CAP  = 8192;      /* u32 slots per thread   */
static const int      MAX_DEPTH  = 64;

struct Rec {
  uint32_t off = 0;
  std::string name;
  uint32_t codeOff = 0;
  int8_t argc = 0;
  int16_t nLoc = 0;
  uint16_t globOff = 0;
  int32_t globMax = 0;
};

struct Cmb {
  std::vector<uint8_t> data;
  std::string selfname, file;
  int ix = 0; /* registration index (file tag in save frame) */
  uint32_t poolOff = 0, poolEnd = 0, entOff = 0;
  std::vector<Rec> recs;
  std::unordered_map<std::string, int> byName;
  uint32_t gRam = 0, gCells = 0; /* globals window in Vm::ram */

  uint32_t u32le(uint32_t o) const { uint32_t v; std::memcpy(&v, data.data() + o, 4); return v; }
  const char* poolStr(uint32_t relOff) const {
    uint32_t o = poolOff + relOff;
    if (o < poolOff || o >= poolEnd) return nullptr;
    return (const char*)data.data() + o;
  }
  bool load(std::vector<uint8_t>&& bytes, const std::string& what) {
    data = std::move(bytes);
    file = what;
    if (data.size() < 0x30 || std::memcmp(data.data(), "cmb", 3) != 0) return false;
    selfname = (const char*)data.data() + 4;
    poolOff = u32le(0x24); poolEnd = u32le(0x28); entOff = poolEnd;
    for (uint32_t e = entOff; e + 4 <= data.size(); e += 4) {
      uint32_t ro = u32le(e);
      if (!ro) break;
      if (ro < 0x28 || ro + 0x28 > data.size()) continue;
      Rec r;
      r.off = ro;
      r.codeOff = u32le(ro + 4);
      r.argc = (int8_t)data[ro + 0x0d];
      r.nLoc = (int16_t)(data[ro + 0x12] | (data[ro + 0x13] << 8));
      r.globOff = (uint16_t)(data[ro + 0x20] | (data[ro + 0x21] << 8));
      r.globMax = (int32_t)r.globOff + (r.nLoc > 0 ? r.nLoc : 0);
      uint32_t no = u32le(ro);
      if (no && no < data.size()) {
        uint32_t j = no;
        while (j < data.size() && data[j] >= 0x20 && data[j] < 0x7f) j++;
        r.name.assign((const char*)data.data() + no, j - no);
      }
      int idx = (int)recs.size();
      recs.push_back(r);
      byName[r.name] = idx; /* last-wins mirrors FE9 hash re-intern */
    }
    int32_t maxg = 64;
    for (auto& r : recs) if (r.globMax > maxg) maxg = r.globMax;
    gCells = (uint32_t)maxg + 0x200; /* slack [INFERENCE] */
    return true;
  }
};

struct Vm;
struct Native {
  const char* name;
  int (*fn)(Vm& vm, uint32_t* args, int argc, uint32_t* out); /* nonzero=yield */
  int argc;
};

struct Thread {
  Cmb* cmb = nullptr;
  uint32_t ramBase = 0;     /* byte address of this thread's stack in ram */
  uint32_t sp = 0;          /* u32 index  */
  uint32_t pc = 0;          /* file offset of current opcode */
  int rec = -1;
  uint32_t gbase = 0;       /* cell index into cmb globals window */
  int32_t lbase = 0;
  int depth = 0;
  bool done = false;
  int pendNative = -1;
  uint32_t pendArgs[10] = {0};
  int pendArgc = 0;
  uint32_t pendSp = 0;
};

struct LogFn { void (*fn)(void* u, const char* msg); void* u; };

struct Vm {
  std::vector<uint8_t> ram;
  uint32_t ramUsed = 0;
  std::vector<Cmb*> cmb;
  std::unordered_map<std::string, std::pair<Cmb*, int>> global;
  std::vector<Native> natives;
  std::unordered_map<std::string, int> nativeIx;
  void (*onNative)(Vm&, int ni, const uint32_t* args, int) = nullptr; /* observer */
  std::unordered_map<uint64_t, uint32_t> strCache; /* (cmbIx<<32|poolOff) -> ram addr */
  LogFn log;
  uint64_t frame = 0;
  int errors = 0;
  bool trace = false;
  struct Ev { const char* file; uint32_t pc; uint8_t op; uint32_t sp; } ring[16384];
  uint32_t ringI = 0;
  void traceEv(const char* f, uint32_t pc, uint8_t op, uint32_t sp) {
    if (!trace) return;
    ring[ringI & 16383] = {f, pc, op, sp}; ringI++;
  }
  void dumpRing() const {
    uint32_t n = ringI < 16384 ? ringI : 16384, start = ringI - n;
    for (uint32_t i = start; i < ringI; i++) {
      auto& e = ring[i & 16383];
      logf("TRACE %s @0x%x op=%02x sp=%u", e.file ? e.file : "?", e.pc, e.op, e.sp);
    }
  }

  Vm() : ram(RAM_SIZE, 0) {}

  void logf(const char* fmt, ...) const {
    if (!log.fn) return;
    char buf[512];
    va_list ap; va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    log.fn(log.u, buf);
  }
  uint32_t ramAlloc(uint32_t bytes, uint32_t align = 16) {
    ramUsed = (ramUsed + align - 1) & ~(align - 1);
    if (ramUsed + bytes > RAM_SIZE) { logf("vm ERR ram exhausted"); errors++; return 0; }
    uint32_t r = ramUsed; ramUsed += bytes; return r;
  }
  uint32_t rd32(uint32_t addr) const {
    if (addr + 4 > RAM_SIZE) return 0;
    uint32_t v; std::memcpy(&v, ram.data() + addr, 4); return v;
  }
  void wr32(uint32_t addr, uint32_t v) {
    if (addr + 4 > RAM_SIZE) { logf("vm ERR wr32 0x%x", addr); return; }
    std::memcpy(ram.data() + addr, &v, 4);
  }
  const char* rdCstr(uint32_t addr, char* tmp, uint32_t cap) const {
    if (addr >= RAM_SIZE) return nullptr;
    uint32_t i = 0;
    while (i + 1 < cap && addr + i < RAM_SIZE && ram[addr + i]) { tmp[i] = (char)ram[addr + i]; i++; }
    tmp[i] = 0;
    return tmp;
  }
  void addCmb(Cmb* c) {
    c->ix = (int)cmb.size();
    c->gRam = ramAlloc(c->gCells * 4);
    cmb.push_back(c);
    for (size_t i = 0; i < c->recs.size(); i++)
      if (!c->recs[i].name.empty()) global[c->recs[i].name] = {c, (int)i};
  }
  int regNative(const Native& n) {
    int i = (int)natives.size();
    natives.push_back(n);
    nativeIx[n.name] = i;
    return i;
  }
  int regNativeHandle(const char* name) { return regNative({name, nullptr, 0}); } /* CALLN handle */
};

enum StepRes { STEP_YIELD = 0, STEP_DONE, STEP_ERROR };

static inline uint16_t be16(const uint8_t* d, uint32_t o) { return (uint16_t)((d[o] << 8) | d[o + 1]); }
static inline uint32_t be32(const uint8_t* d, uint32_t o) {
  return ((uint32_t)d[o] << 24) | ((uint32_t)d[o + 1] << 16) | ((uint32_t)d[o + 2] << 8) | d[o + 3];
}
static inline int natives_size(Vm& V) { return (int)V.natives.size(); }

static inline uint32_t* tsp(Vm& V, Thread& t) { return (uint32_t*)(V.ram.data() + t.ramBase) + t.sp; }

static StepRes pushFrame(Vm& V, Thread& t, Cmb* c, int recIx) {
  Rec& r = c->recs[recIx];
  if (t.depth >= MAX_DEPTH) { V.logf("vm ERR depth @%s", r.name.c_str()); return STEP_ERROR; }
  int32_t argc = r.argc;
  if ((int32_t)t.sp < argc) { V.logf("vm ERR under @%s", r.name.c_str()); return STEP_ERROR; }
  int32_t lbase = (int32_t)t.sp - argc;
  int32_t nLoc = r.nLoc > 0 ? r.nLoc : 0;
  int32_t save = lbase + nLoc;                 /* save frame location */
  if (save + 4 > (int32_t)STACK_CAP) { V.logf("vm ERR stack @%s", r.name.c_str()); return STEP_ERROR; }
  uint32_t* S = (uint32_t*)(V.ram.data() + t.ramBase);
  for (int32_t i = lbase + argc; i < lbase + nLoc; i++) S[i] = 0; /* zero L[argc..nLoc) */
  S[save]     = t.pc;
  S[save + 1] = (uint32_t)(((t.cmb ? t.cmb->ix : 0) << 20) | (uint32_t)(t.rec + 1)); /* caller */
  S[save + 2] = t.gbase + 1;
  S[save + 3] = (uint32_t)t.lbase;
  t.cmb = c; t.rec = recIx; t.pc = r.codeOff;
  t.gbase = r.globOff; t.lbase = lbase; t.sp = (uint32_t)save + 4;
  t.depth++;
  return STEP_DONE;
}

static void callNative(Vm& V, int ni, uint32_t* args, int argc, uint32_t* out, bool* yielding) {
  if (V.onNative) V.onNative(V, ni, args, argc); /* optional observer hook (harness/SE) */
  *yielding = V.natives[ni].fn(V, args, argc, out) != 0;
}

/* run one slice: returns STEP_YIELD (coroutine yielded / budget), STEP_DONE
 * (thread finished), STEP_ERROR. Engine calls per active thread per frame. */
static StepRes run(Vm& V, Thread& t, uint32_t maxOps) {
  uint32_t ops = 0;
  if (t.pendNative >= 0) {
    uint32_t out = 0; bool y = false;
    callNative(V, t.pendNative, t.pendArgs, t.pendArgc, &out, &y);
    if (y) return STEP_YIELD;
    t.pendNative = -1;
    V.wr32(t.ramBase + t.pendSp * 4, out); /* result slot valid for argc==0 too */
    t.sp = t.pendSp + 1;
  }
  bool ovf = false, und = false;
  for (;;) {
    if (++ops > maxOps) return STEP_YIELD;
    const uint8_t* d = t.cmb->data.data();
    if (t.pc >= t.cmb->data.size()) { V.logf("vm ERR pc 0x%x", t.pc); return STEP_ERROR; }
    uint8_t op = d[t.pc];
    V.traceEv(t.cmb->file.c_str(), t.pc, op, t.sp);
    uint32_t p = t.pc + 1;
    int32_t o16 = (int16_t)be16(d, p);
    uint32_t* S = (uint32_t*)(V.ram.data() + t.ramBase);
    auto push = [&](uint32_t v) { if (t.sp < STACK_CAP) S[t.sp++] = v; else ovf = true; };
    auto pop = [&]() -> uint32_t { if (t.sp == 0) { und = true; return 0; } return S[--t.sp]; };
    auto Gc = [&](uint32_t off) -> uint32_t& { return ((uint32_t*)(V.ram.data() + t.cmb->gRam))[t.gbase + off]; };
    auto Lc = [&](int32_t off) -> uint32_t& { return S[t.lbase + off]; };
    auto Lptr = [&](int32_t off) -> uint32_t { return t.ramBase + (uint32_t)(t.lbase + off) * 4; };
    auto Gptr = [&](uint32_t off) -> uint32_t { return t.cmb->gRam + (t.gbase + off) * 4; };
    switch (op) {
      case 0x01: push(Lc((int8_t)d[p])); p += 1; break;
      case 0x02: push(Lc(o16)); p += 2; break;
      case 0x03: case 0x04: {
        int32_t off = (op == 0x03) ? (int32_t)d[p] : o16; p += (op == 0x03) ? 1 : 2;
        uint32_t n = pop(); push(Lc(off + (int32_t)n)); break; }
      case 0x05: case 0x06: { /* LOADIDX_L: *(L[off]+n*4) */
        int32_t off = (op == 0x05) ? (int32_t)d[p] : o16; p += (op == 0x05) ? 1 : 2;
        uint32_t n = pop(); push(V.rd32(Lc(off) + n * 4)); break; }
      case 0x07: push(Lptr((int8_t)d[p])); p += 1; break;
      case 0x08: push(Lptr(o16)); p += 2; break;
      case 0x09: case 0x0a: {
        int32_t off = (op == 0x09) ? (int8_t)d[p] : o16; p += (op == 0x09) ? 1 : 2;
        uint32_t n = pop(); push(Lptr(off) + n * 4); break; }
      case 0x0b: case 0x0c: { /* PUSHADDR_L_IND (asm @22bc/2314): push(L[off] + n*4) — lwzx L[off], add pop<<2, NO deref */
        int32_t off = (op == 0x0b) ? (int32_t)d[p] : o16; p += (op == 0x0b) ? 1 : 2;
        uint32_t n = pop(); push(Lc(off) + n * 4); break; }
      case 0x0d: push(Gc(d[p])); p += 1; break;
      case 0x0e: push(Gc(be16(d, p))); p += 2; break;
      case 0x0f: case 0x10: {
        int32_t off = (op == 0x0f) ? (int32_t)d[p] : o16; p += (op == 0x0f) ? 1 : 2;
        uint32_t n = pop(); push(Gc((uint32_t)off + n)); break; }
      case 0x11: case 0x12: {
        int32_t off = (op == 0x11) ? (int32_t)d[p] : o16; p += (op == 0x11) ? 1 : 2;
        uint32_t n = pop(); push(V.rd32(Gc(off) + n * 4)); break; }
      case 0x13: push(Gptr(d[p])); p += 1; break;
      case 0x14: push(Gptr(be16(d, p))); p += 2; break;
      case 0x15: case 0x16: {
        int32_t off = (op == 0x15) ? (int8_t)d[p] : o16; p += (op == 0x15) ? 1 : 2;
        uint32_t n = pop(); push(Gptr(off) + n * 4); break; }
      case 0x17: case 0x18: { /* PUSHADDR_G_IND (asm @270c/2764): push(G[off] + n*4) — pointer cell + index, NO deref */
        int32_t off = (op == 0x17) ? (int32_t)d[p] : o16; p += (op == 0x17) ? 1 : 2;
        uint32_t n = pop(); push(Gc(off) + n * 4); break; }
      case 0x19: push((uint32_t)(int32_t)(int8_t)d[p]); p += 1; break;
      case 0x1a: push((uint32_t)(int32_t)o16); p += 2; break;
      case 0x1b: push(be32(d, p)); p += 4; break;
      case 0x1c: case 0x1d: case 0x1e: {
        uint32_t po = op == 0x1c ? (uint32_t)(int32_t)(int8_t)d[p] : op == 0x1d ? be16(d, p) : be32(d, p);
        p += (op == 0x1c) ? 1 : (op == 0x1d) ? 2 : 4;
        const char* s = t.cmb->poolStr(po);
        uint64_t key = ((uint64_t)(uint32_t)t.cmb->ix << 32) | po;
        auto itc = V.strCache.find(key);
        uint32_t a;
        if (itc != V.strCache.end()) { a = itc->second; } /* intern once per poolOff */
        else {
          a = V.ramAlloc(64, 1);
          if (s && a) { uint32_t i = 0; while (i < 63 && s[i]) { V.ram[a + i] = (uint8_t)s[i]; i++; } V.ram[a + i] = 0; }
          V.strCache[key] = a;
        }
        push(a);
        break; }
      case 0x1f: { /* LDIND (asm @2a08): lwz top; lwz [top]; push — NOT DUP (no dup opcode exists) */
        uint32_t a0 = t.sp ? S[t.sp - 1] : (und = true, 0u); push(V.rd32(a0)); break; }
      case 0x20: pop(); break;
      case 0x21: { /* STORE_IND (mjs mislabel "OVER"): pop val, pop ptr, *ptr=val,
                      push val [case 0x21 FUN_80011edc: pop,pop-deref-write,push] */
        uint32_t val = pop(); uint32_t ptr = pop();
        if (V.trace) V.logf("[vm] ST ptr=%x val=%x @pc=%x\n", ptr, val, t.pc);
        V.wr32(ptr, val); push(val); break; }
      case 0x22: { uint32_t b = pop(); push(pop() + b); break; }
      case 0x23: { uint32_t b = pop(); push(pop() - b); break; }
      case 0x24: { uint32_t b = pop(); push(pop() * b); break; }
      case 0x25: { uint32_t b = pop(); uint32_t a = pop();
        push(b ? (uint32_t)((int32_t)a / (int32_t)b) : 0); break; } /* divw SIGNED (asm @2b4c) */
      case 0x26: { uint32_t b = pop(); uint32_t a = pop();
        push(b ? (uint32_t)((int32_t)a % (int32_t)b) : 0); break; } /* a - (a/b)*b signed (asm @2b90) */
      case 0x27: push(0 - pop()); break;
      case 0x28: push(~pop()); break;
      case 0x29: push(pop() == 0); break;
      case 0x2a: { uint32_t b = pop(); push(pop() | b); break; }
      case 0x2b: { uint32_t b = pop(); push(pop() & b); break; }
      case 0x2c: { uint32_t b = pop(); push(pop() ^ b); break; }
      case 0x2d: { uint32_t b = pop(); push(pop() << (b & 31)); break; }
      case 0x2e: { uint32_t b = pop(); uint32_t a = pop();
        push((uint32_t)((int32_t)a >> (b & 31))); break; } /* sraw ARITHMETIC (asm @2d80) */
      /* [BRV-SCT-005] CMP family pushes PPC-bit-form booleans (asm @2dc4-0x3048,
         table @0x800126?? verified 2026-11-12): JZ/JNZ test != 0 against raw word.
         Transcribed literal-from-asm — NO closed-form simplification allowed.
         Pop order: top (first pop) named t; second pop named s. */
      case 0x2f: { /* EQ-cc: subf t-s; cntlzw; rotl 27 mask24-31 */
        uint32_t t0 = pop(), s0 = pop(); uint32_t dv = t0 - s0;
        uint32_t c = dv ? (uint32_t)__builtin_clz(dv) : 32;
        push(((c << 27) | (c >> 5)) & 0xFF000000u); break; }
      case 0x30: { /* NE-cc: ((t-s)|(s-t)) << 1, bit31 */
        uint32_t t0 = pop(), s0 = pop();
        push((((t0 - s0) | (s0 - t0)) << 1) & 0x80000000u); break; }
      case 0x31: { /* LT-cc variant A: x=t^s; (x>>1)-(x&t); <<1 bit31 (asm @2e60) */
        uint32_t t0 = pop(), s0 = pop(); uint32_t x = t0 ^ s0;
        push(((uint32_t)((int32_t)(x >> 1) - (int32_t)(x & t0)) << 1) & 0x80000000u); break; }
      case 0x32: { /* LE-cc: signmask(t) + signbit(s) + C(t >=u s), top byte (asm @2eb4) */
        uint32_t t0 = pop(), s0 = pop();
        uint32_t r5 = (uint32_t)((int32_t)t0 >> 31) + (s0 & 0x80000000u) + (t0 >= s0 ? 1u : 0u);
        push(r5 & 0xFF000000u); break; }
      case 0x33: { /* variant B: (x>>1)-(x&s); <<1 bit31 (asm @2f08: and uses SECOND pop) */
        uint32_t t0 = pop(), s0 = pop(); uint32_t x = t0 ^ s0;
        push(((uint32_t)((int32_t)(x >> 1) - (int32_t)(x & s0)) << 1) & 0x80000000u); break; }
      case 0x34: { /* GE-cc: signbit(t) + signmask(s) + C(s >=u t), top byte (asm @2f5c: 0x32 mirror) */
        uint32_t t0 = pop(), s0 = pop();
        uint32_t r5 = (t0 & 0x80000000u) + (uint32_t)((int32_t)s0 >> 31) + (s0 >= t0 ? 1u : 0u);
        push(r5 & 0xFF000000u); break; }
      case 0x35: { /* asm @2fb0 byte-loop: EVERY path (match and mismatch) pushes 0.
                      Faithful constant; kept for trace-position parity. */
        pop(); pop(); push(0); break; }
      case 0x36: { /* asm @304c byte-loop (signed bytes): equal -> 0, differ -> 1 (STRNE-bool) */
        char b2[1024], a2[1024];
        uint32_t pb = pop(), pa = pop();
        const char* sb = V.rdCstr(pb, b2, 1024);
        const char* sa = V.rdCstr(pa, a2, 1024);
        bool eq = sa && sb && std::strcmp(sa, sb) == 0;
        push(eq ? 0u : 1u); break; }
      case 0x37: { /* CALL_LOCAL entries[u8] */
        uint8_t e = d[p]; p += 1;
        uint32_t ro = t.cmb->u32le(t.cmb->entOff + e * 4);
        int ri = -1;
        for (int i = 0; i < (int)t.cmb->recs.size(); i++) if (t.cmb->recs[i].off == ro) { ri = i; break; }
        if (ri < 0) { V.logf("vm ERR entry %u", e); return STEP_ERROR; }
        t.pc = p;
        if (pushFrame(V, t, t.cmb, ri) == STEP_ERROR) return STEP_ERROR;
        continue; }
      case 0x38: { /* CALL name: u16-BE pool offset, u8 argc */
        uint32_t po = be16(d, p); uint8_t na = d[p + 2]; p += 3; (void)na;
        const char* nm = t.cmb->poolStr(po);
        t.pc = p;
        if (!nm) { V.logf("vm ERR name pool 0x%x", po); return STEP_ERROR; }
        auto it = V.global.find(nm);
        if (it != V.global.end()) {
          if (pushFrame(V, t, it->second.first, it->second.second) == STEP_ERROR) return STEP_ERROR;
          continue;
        }
        auto nit = V.nativeIx.find(nm);
        if (nit == V.nativeIx.end()) { V.logf("vm ERR unknown '%s' @0x%x", nm, t.pc); return STEP_ERROR; }
        int ni = nit->second;
        int argc = V.natives[ni].argc;
        if ((int32_t)t.sp < argc) { V.logf("vm ERR under @n:%s", nm); return STEP_ERROR; }
        uint32_t args[10];
        for (int i = 0; i < argc && i < 10; i++) args[i] = S[t.sp - argc + i];
        uint32_t out = 0; bool y = false;
        callNative(V, ni, args, argc, &out, &y);
        if (y) {
          t.pendNative = ni; t.pendArgc = argc; t.pendSp = t.sp - argc;
          for (int i = 0; i < argc && i < 10; i++) t.pendArgs[i] = args[i];
          return STEP_YIELD;
        }
        t.sp = t.sp - argc + 1;
        S[t.sp - 1] = out; /* result pushed even for argc==0 natives */
        continue; }
      case 0x39: { /* RET */
        uint32_t res = pop();
        Rec& r = t.cmb->recs[t.rec];
        int32_t nLoc = r.nLoc > 0 ? r.nLoc : 0;
        int32_t save = t.lbase + nLoc;
        uint32_t pc = S[save], rec1 = S[save + 1], gb1 = S[save + 2];
        int32_t lb = (int32_t)S[save + 3];
        int cix = (int)(rec1 >> 20);
        int ri = (int32_t)(rec1 & 0xfffff) - 1;
        static int xlog = 0;
        if (xlog < 6 && (uint32_t)(t.cmb->ix << 20) != (rec1 & 0xfff00000u)) {
          V.logf("RET-X rec1=0x%x cix=%d ri=%d -> %s", rec1, cix, ri,
                 (cix >= 0 && cix < (int)V.cmb.size()) ? V.cmb[cix]->file.c_str() : "??");
          xlog++;
        }
        if (cix >= 0 && cix < (int)V.cmb.size()) t.cmb = V.cmb[cix];
        uint32_t own = t.lbase; /* real RET: sp = CALLEE's lbase (arg slots), not
                                   saved caller lbase; result overwrites arg0 */
        t.pc = pc; t.rec = ri; t.gbase = gb1 ? gb1 - 1 : 0; t.lbase = lb;
        t.sp = own; t.depth--;
        push(res);
        if (t.depth == 0) {
          if (t.pc == 0) { t.done = true; return STEP_DONE; }
          V.logf("vm ERR depth0 pc=0x%x", t.pc); return STEP_ERROR;
        }
        continue; }
      case 0x3a: t.pc = (uint32_t)((int32_t)(p + 2) + o16 - 2); continue;
      case 0x3b: { uint32_t v = pop(); t.pc = v ? (uint32_t)((int32_t)(p + 2) + o16 - 2) : p + 2; continue; }
      case 0x3c: { uint32_t v = pop();
        if (v) { push(1); t.pc = (uint32_t)((int32_t)(p + 2) + o16 - 2); } else t.pc = p + 2;
        continue; }
      case 0x3d: { uint32_t v = pop(); t.pc = v ? p + 2 : (uint32_t)((int32_t)(p + 2) + o16 - 2); continue; }
      case 0x3e: { uint32_t v = pop(); /* JZ_PUSH0: branch+push0 when zero */
        if (!v) { push(0); t.pc = (uint32_t)((int32_t)(p + 2) + o16 - 2); } else t.pc = p + 2;
        continue; }
      case 0x3f: { /* asm @355c: stb 1,0x1c(ctx) = yielded flag; pump re-enters
                      next frame at pc+1. 261 uses incl. `YIELD YIELD YIELD` runs. */
        t.pc = p; return STEP_YIELD; }
      case 0x40: t.pc = p + 4; continue;
      case 0x41: { /* LOGF u8 n: pop n args -> FUN_800056cc debug-printf (retail: stub) */
        uint8_t n = d[p]; p += 1;
        if ((int32_t)t.sp < n) { V.logf("vm ERR under @logf"); return STEP_ERROR; }
        t.sp -= n; /* retail stub prints nothing, pushes nothing (FUN_80011edc case 0x41) */
        t.pc = p;
        continue; }
      default:
        V.logf("vm ERR opcode 0x%02x @0x%x in %s [cmb=%s ix=%d rec=%d depth=%d]", op, t.pc,
               t.rec >= 0 ? t.cmb->recs[t.rec].name.c_str() : "?",
               t.cmb->file.c_str(), t.cmb->ix, t.rec, t.depth);
        V.dumpRing();
        return STEP_ERROR;
    }
    if (ovf || und) {
      V.logf("vm ERR %s sp=%u op=%02x @0x%x depth=%d rec=%s", ovf ? "OVF" : "UND", t.sp, op, t.pc,
             t.depth, t.cmb && (int)t.rec < (int)t.cmb->recs.size() ? t.cmb->recs[t.rec].name.c_str() : "?");
      return STEP_ERROR; }
    t.pc = p;
  }
}


/* start script function on a fresh thread; args pre-pushed */
static inline Thread* start(Vm& V, const char* fname, const uint32_t* args, int n) {
  auto it = V.global.find(fname);
  if (it == V.global.end()) return nullptr;
  Thread* t = new Thread();
  t->ramBase = V.ramAlloc(STACK_CAP * 4);
  if (!t->ramBase) { delete t; return nullptr; }
  uint32_t* S = (uint32_t*)(V.ram.data() + t->ramBase);
  for (int i = 0; i < n; i++) S[i] = args[i];
  t->sp = (uint32_t)n;
  if (pushFrame(V, *t, it->second.first, it->second.second) == STEP_ERROR) { delete t; return nullptr; }
  return t;
}

} // namespace vm
#endif
