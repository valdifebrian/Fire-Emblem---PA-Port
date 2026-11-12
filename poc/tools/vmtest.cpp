/* vmtest.cpp — desktop replay harness for scriptvm.h [BRV-SCT-005][P3-M3.2]
 * usage: vmtest <cmb-dir> <file.cmb ...> [--entry NAME] [--frames N]
 *   [--call NAME[:arg1,arg2...]]*  — runs after entry; default entry: first rec
 * Natives: every name from native_catalog.csv gets a log-stub; WaitM yields.
 * Proves: raw-.cmb execution + call/jump arithmetic + native dispatch +
 * coroutine yields, against REAL disc bytes.
 */
#include "scriptvm.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
using namespace vm;

static void logger(void* u, const char* msg) { (void)u; std::printf("[vm] %s\n", msg); }

struct Trace { int* n; };
static int gNatives = 0, gCalls = 0, gStrings = 0;

/* generic stub: log name+args, return 0 */
static int stub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)V; (void)args; (void)argc;
  *out = 0; gCalls++;
  return 0;
}
/* WaitM/Wait: yield N frames (arg0 = frame count; 0 = 1 frame) */
struct WaitState { uint32_t left; };
static WaitState gWait;
static int waitStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)argc;
  if (gWait.left == 0) gWait.left = args[0] ? args[0] : 1;
  if (gWait.left > 1) { gWait.left--; *out = 0; (void)V; return 1; }
  gWait.left = 0; *out = 0;
  return 0;
}
/* dialog-ish: log string arg then yield 1 (dialog stays open) */
static int dlgStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  char buf[256];
  const char* s = nullptr;
  for (int i = 0; i < argc; i++)
    if (args[i] < RAM_SIZE) { s = V.rdCstr(args[i], buf, 256); if (s && *s) break; }
  if (s && *s) { gStrings++; std::printf("[dlg] %s\n", s); }
  *out = 0;
  return 0;
}
/* UnitGetByPID: deterministic fake handle = pid+0x1000 */
static int unitStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)V; (void)argc; *out = args[0] + 0x1000; return 0; }
/* random: fixed seed LCG (golden harness overrides with recorded stream) */
static uint32_t gRng = 12345;
static int rngStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)V; (void)argc; gRng = gRng * 1103515245u + 12345u; *out = (gRng >> 16) % (args[0] ? args[0] : 1); return 0; }
/* Fade(mode u8, time u16-frames): start timed fade. isFading: 1 while fade active.
 * FadeWait = "while(isFading) yield" (0x3d JZ exits on 0) -> exits when fade completes. */
static uint64_t gFadeEnd = 0;
static int fadeStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)argc; uint64_t dur = args[1] ? args[1] : 1;
  static long k = 0; if (k++ < 4000) std::fprintf(stderr, "[fade] t=%llu a0=%u a1=%u\n", (unsigned long long)V.frame, args[0], args[1]);
  gFadeEnd = V.frame + dur; *out = 0; return 0; }
static int isFadingStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)args; (void)argc; *out = (V.frame < gFadeEnd) ? 1 : 0;
  static long k = 0;
  if (k < 4 || (k % 50000) == 0) {
    std::fprintf(stderr, "[isf] frame=%llu end=%llu out=%u\n", (unsigned long long)V.frame, (unsigned long long)gFadeEnd, *out);
  }
  k++;
  return 0; }
static int waitFStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)argc; *out = 0;
  /* latch deadline on first entry (re-entry every frame must not extend it) */
  static uint64_t wfEnd = 0;
  if (wfEnd == 0) wfEnd = V.frame + (args[0] ? args[0] : 1);
  if (V.frame < wfEnd) return 1;
  wfEnd = 0;
  return 0; }
/* existence gates (Tut* / dialog loops): report present so loops terminate */
static int existsStub(Vm& V, uint32_t* args, int argc, uint32_t* out) {
  (void)V; (void)args; (void)argc; *out = 1; return 0; }

/* native-name histogram (VMHIST=1) */
static std::map<std::string, int> gHist;
static void histHook(Vm& V, int ni, const uint32_t*, int) { gHist[V.natives[ni].name]++; }
static void histDump(Vm& V) {
  if (!V.onNative) return;
  std::vector<std::pair<int, std::string>> v;
  for (auto& kv : gHist) v.push_back({kv.second, kv.first});
  std::sort(v.begin(), v.end(), std::greater<>());
  for (auto& p : v) std::printf("[hist] %s x%d\n", p.second.c_str(), p.first);
}
int main(int argc, char** argv) {
  if (argc < 3) { std::printf("usage: vmtest <cmb-dir> file.cmb... [opts]\n"); return 2; }
  Vm V; V.log = {logger, nullptr};
  std::string dir = argv[1];
  std::vector<Cmb*> loaded;
  std::string entry; uint64_t frames = 600;
  std::vector<std::string> calls;
  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--entry") entry = argv[++i];
    else if (a == "--trace") V.trace = true;
    else if (a == "--frames") frames = std::stoull(argv[++i]);
    else if (a == "--call") calls.push_back(argv[++i]);
    else {
      std::string path = dir + "/" + a;
      std::ifstream f(path, std::ios::binary);
      std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      if (bytes.empty()) { std::printf("cannot read %s\n", path.c_str()); return 2; }
      Cmb* c = new Cmb();
      if (!c->load(std::move(bytes), a)) { std::printf("bad cmb %s\n", path.c_str()); return 2; }
      std::printf("[cmb] %s: self=%s recs=%zu entries@0x%x pool=0x%x..0x%x\n", a.c_str(),
                  c->selfname.c_str(), c->recs.size(), c->entOff, c->poolOff, c->poolEnd);
      loaded.push_back(c);
      V.addCmb(c);
    }
  }
  if (loaded.empty()) { std::printf("no cmb loaded\n"); return 2; }
  /* natives: union registry (catalog + call-site argc) [BRV-SCT-005] */
  { std::ifstream f(dir + "/../data/script_native_registry.csv"); std::string line;
    bool head = true;
    while (std::getline(f, line)) {
      if (head) { head = false; continue; }
      if (!line.empty() && line[0] == '#') continue;
      std::stringstream ss(line); std::string nm, rest;
      std::getline(ss, nm, ','); std::getline(ss, rest, ',');
      int ac = rest.empty() ? 0 : std::stoi(rest);
      Native n{strdup(nm.c_str()), stub, ac};
      if (nm == "WaitM" || nm == "Wait" || nm == "WaitS" || nm == "WaitFocus") n.fn = waitStub;
      else if (nm == "UnitGetByPID") n.fn = unitStub;
      else if (nm.find("BG") != std::string::npos) n.fn = stub;
      V.regNative(n); gNatives++;
    }
    std::printf("[nat] %d stubs from catalog\n", gNatives);
  }
  /* dialog-ish natives get string-logging too (names guessable from spec) */
  for (const char* d : {"SallySetByGroup","regist","DrawMsg","TextDraw","DispMsg"})
    if (V.nativeIx.count(d)) V.natives[V.nativeIx[d]].fn = dlgStub;
  for (const char* r : {"Random","GetRandom","rnd"})
    if (V.nativeIx.count(r)) V.natives[V.nativeIx[r]].fn = rngStub;
  for (const char* d : {"Fade","FadeIn","FadeOut","ScreenFade"})
    if (V.nativeIx.count(d)) V.natives[V.nativeIx[d]].fn = fadeStub;
  if (V.nativeIx.count("isFading")) V.natives[V.nativeIx["isFading"]].fn = isFadingStub;
if (V.nativeIx.count("WaitF")) V.natives[V.nativeIx["WaitF"]].fn = waitFStub;
  for (const char* d : {"ProcExists","TalkExist"})
    if (V.nativeIx.count(d)) V.natives[V.nativeIx[d]].fn = existsStub;

  if (getenv("VMHIST")) V.onNative = histHook;

  if (entry.empty() && !loaded[0]->recs.empty())
    for (auto& r : loaded[0]->recs) if (!r.name.empty()) { entry = r.name; break; }
  std::printf("[run] entry=%s frames=%llu\n", entry.c_str(), (unsigned long long)frames);
  Thread* t = vm::start(V, entry.c_str(), nullptr, 0);
  if (!t) { /* fallback: try first rec of first cmb by index */
    std::printf("[run] entry not found; recs: ");
    for (auto* c : loaded) for (auto& r : c->recs) { std::printf("%s ", r.name.c_str()); }
    std::printf("\n"); return 1;
  }
  uint64_t f = 0;
  for (; f < frames && !t->done; f++) {
    V.frame = f;
    StepRes r = vm::run(V, *t, 200000);
    if ((f % 20000) == 0) std::fprintf(stderr, "[pump] f=%llu r=%d pend=%d pc=0x%x sp=%d\n",
      (unsigned long long)f, (int)r, t->pendNative, t->pc, t->sp);
    if (r == STEP_ERROR) { std::printf("[stop] ERROR at frame %llu\n", (unsigned long long)f); break; }
  }
  if (V.trace) V.dumpRing();
  std::printf("[done] frames=%llu done=%d natives=%d strings=%d depth=%d\n",
              (unsigned long long)f, (int)t->done, gCalls, gStrings, t->depth);
  for (auto& call : calls) {
    std::string nm = call; std::vector<uint32_t> args;
    auto col = call.find(':');
    if (col != std::string::npos) {
      nm = call.substr(0, col);
      std::stringstream as(call.substr(col + 1)); std::string v;
      while (std::getline(as, v, ',')) args.push_back((uint32_t)std::stoul(v, nullptr, 0));
    }
    Thread* t2 = vm::start(V, nm.c_str(), args.data(), (int)args.size());
    if (!t2) { std::printf("[call] %s NOT FOUND\n", nm.c_str()); continue; }
    for (uint64_t i = 0; i < frames && !t2->done; i++)
      if (vm::run(V, *t2, 200000) == STEP_ERROR) break;
    std::printf("[call] %s done=%d\n", nm.c_str(), (int)t2->done);
  }
  histDump(V);
  return 0;
}
