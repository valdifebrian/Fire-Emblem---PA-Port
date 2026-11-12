// main.cpp — FE9 P2 Architecture PoC harness (NativeActivity + EGL/GLES3 + Oboe).
// Proves 5 capabilities on-device with real FE9 data via poc1.fpk (docs/FPK_FORMAT.md):
//  (1) native texture load: ANNA.cms LZ77+TPL+RGB565 → GLES3 texture
//  (2) native model: archer skeleton.g + body.gs skinned render + orbit camera
//  (3) native anim: move_n.ga frames/events evaluated each vsync; event codes logged
//  (4) audio: boss1.stm looped + boss2.stm mixed (gain toggle) via Oboe
//  (5) input: touch overlay events from Kotlin via JNI + PHYSICAL GAMEPAD
//      (MotionEvent generic-motion + key events -> nativePadButton/nativePadAxis;
//      when no pad is present the pad path is inert = touch behavior identical)
//      + keyboard path (desktop TODO)
// Plus fps log + thermal read = REQ-TIME/PERF hooks.
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <oboe/Oboe.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <deque>
#include <unordered_map>

#define FPK_IMPL
#define CMSDEC_IMPL
#include "fpk.h"
#include "cmsdec.h"
#include "gload.h"
#include "gsload.h"
#include "gaanim.h"
#include "scriptvm.h"
#include "texfile.h"
#include "ui.h"
#include "stmdec.h"
#include "resmgr.h"
#include "input.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FE9PoC", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FE9PoC", __VA_ARGS__)

static ActQueue g_acts;  // [P3-INP-001] one edge queue, three producers (touch/pad/keyboard)
static std::atomic<int> g_tapsIn100ms{0};
static std::atomic<float> g_mixB{0.f};    // gain of stream B 0..1

/* ---------- physical gamepad state (Kotlin dispatchGenericMotionEvent /
 * onKeyDown|Up -> nativePadButton/nativePadAxis). Enum ints are ABI with the
 * Kotlin PADB/PADAX constants in MainActivity.kt — NEVER reorder. ---------- */
enum PadButton : int {
  PADB_A = 0, PADB_B, PADB_X, PADB_Y, PADB_L1, PADB_R1, PADB_L2, PADB_R2,
  PADB_SELECT, PADB_START, PADB_L3, PADB_R3, PADB_GUIDE,
  PADB_DPUP, PADB_DPDOWN, PADB_DPLEFT, PADB_DPRIGHT, PADB_COUNT
};
enum PadAxis : int {
  PADAX_LX = 0, PADAX_LY, PADAX_RX, PADAX_RY, PADAX_LT, PADAX_RT,
  PADAX_HATX, PADAX_HATY, PADAX_COUNT
};
static std::atomic<int> g_padBtn[PADB_COUNT];
static std::atomic<float> g_padAxis[PADAX_COUNT];   // normalized -1..1 (LT/RT 0..1)
static std::atomic<int> g_padConnected{0};           // sticky once any pad input seen
static std::atomic<unsigned> g_padEdge{0};           // bit i = fresh press of PADB_i
static std::atomic<int> g_paused{0};                 // START overlay toggle (P3 menu stub)

struct TouchState { float tapNX, tapNY; bool tapReq; int taps100ms; };
struct PadState    { bool btn[PADB_COUNT]; float ax[PADAX_COUNT]; bool connected; };
struct InputState  { TouchState touch; PadState pad; };

// ---------- audio mixer [P3-AUD-001] ----------
/* N-channel mixer: slots 0/1 = BGM crossfade pair, slots 2..3 = SE one-shots.
 * SE payloads rotate through the SE slots; one-shots end when consumed (p freed
 * for the next hit). .samp bank (P2-blocked) will feed the same playSe() later —
 * today's SE payload is a real decoded STM as a documented stand-in. */
struct Mixer : oboe::AudioStreamDataCallback, oboe::AudioStreamErrorCallback {
  static const int NBGM = 2, NSLOT = 4;
  struct S { const int16_t* p = nullptr; int n = 0, pos = 0, loop = 0;
             std::atomic<float> tgt{0.f}; std::atomic<float> ramp{0.02f}; float g = 0.f; } s[NSLOT];
  std::atomic<float> master{1.f}, duck{1.f};
  int seNext = NBGM;
  oboe::AudioStream* stream = nullptr;
  void bindBgm(int i, const stm_file& d) { /* call on engine thread before start */
    s[i].p = d.pcm; s[i].n = d.pcmFrames; s[i].pos = 0; s[i].loop = d.loopStart >= 0 ? d.loopStart : 0;
    s[i].tgt.store(i == 0 ? 0.8f : 0.f);
    s[i].ramp.store(0.02f);
  }
  /* [BRV-SCT-005k] BGMFadeOut(slot,dur_ms): slot×0x30 table semantics -> our
   * 2-slot BGM pair; exponential ramp sized ~95% in dur (k=3/T). */
  void bgmFadeOut(int slot, float ms) {
    if (slot < 0 || slot >= NBGM) return;
    float T = std::max(8.f, std::min(240.f, ms / 16.667f));
    s[slot].ramp.store(3.f / T);
    s[slot].tgt.store(0.f);
  }
  void bgmMix(float x) { s[1].tgt.store(x); }
  void setDuck(float v) { duck.store(v); }             /* menu/dialog duck for BGM */
  void setMaster(float v) { master.store(v); }
  bool playSe(const int16_t* p, int n) {               /* rotate SE slots; returns slot busy-drop as false */
    if (!p || n <= 0) return false;
    int i = seNext++; if (seNext >= NSLOT) seNext = NBGM;
    s[i].pos = 0; s[i].n = n; s[i].loop = 0;           /* n before p: callback reads p last */
    s[i].p = p; s[i].tgt.store(1.f);
    return true;
  }
  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* st, void* audioData, int32_t numFrames) override {
    auto* out = static_cast<int16_t*>(audioData);
    float dk = duck.load(), mst = master.load();
    for (int32_t f = 0; f < numFrames; f++) {
      float mL = 0.f, mR = 0.f;
      for (int i = 0; i < NSLOT; i++) {
        S& c = s[i];
        if (!c.p) continue;
        float tgt = c.tgt.load() * (i < NBGM ? dk : 1.f);
        c.g += (tgt - c.g) * (i < NBGM ? c.ramp.load() : 0.3f); /* BGM ramp (bind/fade), SE fast attack */
        mL += c.p[c.pos * 2] * c.g;
        mR += c.p[c.pos * 2 + 1] * c.g;
        if (++c.pos >= c.n) {
          if (c.loop) c.pos = c.loop;
          else { c.tgt.store(0.f); if (c.g < 0.001f) { c.p = nullptr; c.n = 0; } else c.pos = c.n - 1; }
        }
      }
      int rl = (int)(mL * mst), rr = (int)(mR * mst);
      if (rl > 32767) rl = 32767; if (rl < -32768) rl = -32768;
      if (rr > 32767) rr = 32767; if (rr < -32768) rr = -32768;
      out[f * 2] = (int16_t)rl; out[f * 2 + 1] = (int16_t)rr;
    }
    return oboe::DataCallbackResult::Continue;
  }
  void onErrorBeforeClose(oboe::AudioStream*, oboe::Result) override {}
  void onErrorAfterClose(oboe::AudioStream*, oboe::Result) override {}
};
static Mixer g_mixer;

// ---------- scenes [P3-SCN-001] ----------
#include "scene.h"
#include "savemgr.h"
static SaveMgr g_saves;            // [P3-SAV-001] engine-thread only
static uint32_t g_rng = 0xC0FFEEu; // xorshift state; reseeded from loaded-state hash
static uint64_t g_playFrames = 0;  // cumulative engine frames (save payload field)
static SceneStack g_sm;                 // root = ATTRACT; cold boot lands here (product rule)
static FadeState g_fd = FD_IN;          // boot itself fades in from black
static float g_fDim = 1.f;              // fade amount: 1=black, 0=full (FS gets (1-g_fDim)*sceneDim as brightness)
static inline float scr_fade(void);     /* [P4-M0] script Fade overlay, defined in script module */
static int g_fdOp = 0;                  // pending op applied at black: 1 replace 2 push 3 pop
static SceneId g_fdId = SC_ATTRACT;
static float g_sceneDim = 1.f;          // dialog/battle keep a 0.5 world dim outside transitions
static float g_camDist = 55.f;          // eased per-scene camera distance

// ---------- GL ----------
static const char* VS = R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec2 aUV;
uniform mat4 uMVP; out vec3 vN; out vec2 vUV;
void main(){ vN=aNrm; vUV=aUV; gl_Position=uMVP*vec4(aPos,1.0); })";
static const char* FS = R"(#version 300 es
precision mediump float; in vec3 vN; in vec2 vUV; uniform sampler2D uTex; uniform float uDim;
uniform vec3 uTint; uniform float uAlpha; out vec4 o;
void main(){ vec3 c=texture(uTex,vUV).rgb; float l=0.35+0.65*max(dot(normalize(vN),normalize(vec3(0.4,0.8,0.6))),0.0); o=vec4(c*l*uDim*uTint,uAlpha); })";
static const char* VS2 = R"(#version 300 es
layout(location=0) in vec2 aPos; layout(location=1) in float aIdx;
uniform vec2 uScr; uniform float uHeld[8]; out float vH;
void main(){ vH=uHeld[int(aIdx)]; gl_Position=vec4(aPos.x/uScr.x*2.0-1.0, 1.0-aPos.y/uScr.y*2.0, 0.0, 1.0); })";
static const char* FS2 = R"(#version 300 es
precision mediump float; in float vH; out vec4 o;
void main(){ o=mix(vec4(0.45,0.55,0.65,0.30),vec4(0.95,0.75,0.30,0.60),vH); })";
/* UI quad batch [P3-UI-001]: screen px y-down -> clip; tex.a * uColor */
static const char* VS3 = R"(#version 300 es
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV;
uniform vec2 uScr; out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos.x/uScr.x*2.0-1.0, 1.0-aPos.y/uScr.y*2.0, 0.0, 1.0); })";
static const char* FS3 = R"(#version 300 es
precision mediump float; in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor; out vec4 o;
void main(){ o=texture(uTex,vUV)*uColor; })";

static GLuint makeProgram(const char* vs, const char* fs) {
  GLuint p = glCreateProgram();
  for (int i = 0; i < 2; i++) {
    GLuint s = glCreateShader(i ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
    const char* src = i ? fs : vs;
    glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, sizeof log, nullptr, log); LOGI("shader fail: %s", log); }
    glAttachShader(p, s);
  }
  glLinkProgram(p);
  GLint link = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &link);
  if (!link) {
    char log[1024];
    glGetProgramInfoLog(p, sizeof log, nullptr, log);
    LOGI("LINK FAIL prog=%u: %s", p, log);
  }
  return p;
}

static EGLDisplay dpy = EGL_NO_DISPLAY;
static EGLSurface surf = EGL_NO_SURFACE;
static EGLContext ctx = EGL_NO_CONTEXT;

typedef struct { float m[16]; } mat4; /* column-major GL */

static bool egl_create(ANativeWindow* win, int* w, int* h) {
  dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!eglInitialize(dpy, nullptr, nullptr)) return false;
  const EGLint ca[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                       EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                       EGL_NONE};
  EGLConfig cfg; EGLint nc = 0;
  if (!eglChooseConfig(dpy, ca, &cfg, 1, &nc) || !nc) return false;
  surf = eglCreateWindowSurface(dpy, cfg, win, nullptr);
  const EGLint cg[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cg);
  if (!eglMakeCurrent(dpy, surf, surf, ctx)) return false;
  eglSwapInterval(dpy, 1);
  EGLint ww, hh; eglQuerySurface(dpy, surf, EGL_WIDTH, &ww); eglQuerySurface(dpy, surf, EGL_HEIGHT, &hh);
  *w = ww; *h = hh;
  return true;
}

static mat4 m4mul(const mat4& a, const mat4& b) {
  mat4 r;
  for (int c = 0; c < 4; c++) for (int rw = 0; rw < 4; rw++) {
    float s = 0;
    for (int k = 0; k < 4; k++) s += a.m[k * 4 + rw] * b.m[c * 4 + k];
    r.m[c * 4 + rw] = s;
  }
  return r;
}

// ---------- engine state ----------
static std::vector<uint8_t> g_fpkg;
static fpk_file g_fpak;

/* ---------- external asset override (commercial IP-swap path, docs/FPK_FORMAT.md)
 * Checked BEFORE the FPK lookup: drop <root>/<fpk-entry-name> (e.g.
 * archer/tex_4.tpl) and the engine loads the replacement with zero code change
 * and zero FPK rebuild. Roots in order: env FE9_OVERRIDE_DIR, else BOTH
 * /storage/emulated/0/Android/data/<pkg>/files/assets_override (explorer-visible)
 * AND /data/user/0/<pkg>/files/assets_override (run-as-pipe deployable — plain
 * `adb push` into Android/data is SELinux-blocked to the app since API 29).
 * No dir / no file = every lookup falls through to the FPK path untouched. ---------- */
static std::vector<std::string> g_overrideRoots;
static void override_init() {
  const char* e = getenv("FE9_OVERRIDE_DIR");
  if (e && *e) { g_overrideRoots.push_back(e); }
  else {
    /* files adb-push'd into Android/data carry shell_data_file SELinux label and
     * are unreadable to the app since API 29; the internal dir via run-as pipe
     * works everywhere incl. non-root physical devices. */
    g_overrideRoots.push_back(
        "/storage/emulated/0/Android/data/com.fe9.poc/files/assets_override");
    g_overrideRoots.push_back("/data/user/0/com.fe9.poc/files/assets_override");
  }
  for (auto& r : g_overrideRoots) LOGI("override: root=%s", r.c_str());
}
static bool override_read(const char* name, std::vector<uint8_t>& v) {
  if (g_overrideRoots.empty() || !name || !*name) return false;
  while (*name == '/') name++;             /* accept /Face/x.cms and Face/x.cms */
  for (auto& root : g_overrideRoots) {
    std::string p = root;
    if (p.back() != '/') p += '/';
    p += name;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) continue;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    bool ok = sz > 0;
    if (ok) {
      v.resize((size_t)sz);
      ok = fread(v.data(), 1, (size_t)sz, f) == (size_t)sz;
    }
    fclose(f);
    if (ok) { LOGI("OVERRIDE HIT %s <- %s (%ld B; FPK entry ignored)", name, p.c_str(), sz); return true; }
    v.clear();
  }
  return false;
}
/* Raw provider for ResMgr [P3-RES-001]: override dir first (IP-swap path),
 * then FPK entry copy. resmgr.h owns caching/LRU/prefetch on top of this. */
static bool raw_provider(const char* name, std::vector<uint8_t>& v) {
  if (override_read(name, v)) return true;
  const fpk_entry* e = fpk_find(&g_fpak, name);
  if (e) v.assign(g_fpak.data + e->off, g_fpak.data + e->off + e->len);
  return e != nullptr;
}
static ResMgr g_res;

static g_model g_g; static gs_body g_gs; static ga_anim g_ga;
static ga_anim g_gas[4]; static int g_gaSel = 0;
static tpl_file g_tex; static tpl_file g_texA; static uint32_t g_texId = 0;
/* M2.2 terrain [BRV-MAP-001]: bmap01 static mesh + texpack img0 atlas */
static gs_body g_mapgs; static tpl_file g_maptex; static GLuint g_mapTex = 0;
static GLuint g_mapVao = 0, g_mapVbo = 0; static int g_mapTris = 0;
static g_model g_mapnobones; /* no .ga: gs_vertex falls through to raw pos */
static GLuint g_vao = 0, g_vbo = 0; static int g_triCount = 0;
static GLuint g_prog = 0, g_prog2 = 0, g_prog3 = 0;
/* ---------- UI engine [P3-UI-001] ---------- */
static UiFont g_font;                       /* talk.gcf atlas: real FE9 dialog font */
static tpl_file g_fuki, g_arrow;            /* real window chrome: bubble + cursor anim */
static GLuint g_fukiTex = 0, g_arrowTex = 0, g_whiteTex = 0, g_annaTex = 0;
static GLuint g_uiVao = 0, g_uiVbo = 0;
static UiBatch g_uiB;
static int g_dlgPage = 0; static long long g_dlgT0 = 0; static int g_dlgAll = 0;
static std::vector<uint8_t> g_gcf;
static std::vector<uint8_t> g_stmA, g_stmB;
static stm_file g_sA, g_sB;
static std::atomic<float> g_camYaw{45.f}; /* deg; FIXED isometric default, pad-orbit writable */
static std::atomic<float> g_camPitch{24.f}; /* deg; RIGHT_STICK look, L3/R3 refit resets it */
static int g_gx = 14, g_gy = 14; /* mirror of the active unit tile for save/cursor [P4-M1] */
static float g_ux = 0.f, g_uz = 0.f; /* mirror world pos of the active unit (camera focus) */
static float g_camFx = 0.f, g_camFz = 0.f; /* smoothed camera focus */
static std::atomic<int> g_tapReq{0};
static std::atomic<float> g_tapNX{0.f}, g_tapNY{0.f};
static float g_scrW = 1.f, g_scrH = 1.f;
static int g_frame = 0; static long long g_frameMs = 0;

/* ---------- grid movement ----------
 * M2.2 [BRV-MAP-001]: tiles are REAL bmap01 cells: 29x24 of 10 units, world
 * x -145..145, z -120..120; tile (gx,gy) center = (-140+10gx, -140+10gy).
 * Script tile coords (e.g. UnitSetPosDirByPID 15,13) map 1:1 onto this grid.
 * Path = FIFO of tile coords. Unit yaw = atan2(dx,dz)
 * (model front assumed +Z at identity; flip +180 if vision shows otherwise). */
#define TILE_W 10.f
#define MAP_TW 29
#define MAP_TH 24
static float tile_cx(int g) { return -140.f + 10.f * g; }
/* Z rows start at world -120 (build_grid / mesh p[2]-120 / tap_to_tile hz+120);
 * tile center = -115+10g. Units must land INSIDE their tile, not on the line. */
static float tile_cz(int g) { return -115.f + 10.f * g; }
/* ============================================================================
 * [P4-M1] WORLD SIMULATION — Ch1 taktis loop pasca-opening (P4-M1.1).
 * Unit table (script natives fill it: UnitSetPosDirByPID / UnitGetByPID ...),
 * real terrain cost baked from bmap01 panelindex water panels
 * (maps/bmap01_cost.bin; water=99 blocks movement), maplinkAt exits
 * (maps/bmap01_exits.bin) as enemy-wave spawn pool.
 * Combat = BRV-BTL-009 mirror: RNG = 3x u16 LFSR @8032F3B0 exactly as decoded;
 * hit% = HIT-AVO clamp[0,100] with TWO-ROLL average (FUN_8000cd88), crit via
 * single roll (FUN_8000ce68), crit x3, double strike when def.AS+4 <= atk.AS,
 * dmg = max(0, ATK-DEF). [INFERENCE flagged] weapon-item column mapping
 * (practice sword mt5/hit90/crit0) and per-unit stat semantics stay P4-CHR
 * placeholders — formulas are the verified gate, numbers are v1 constants.
 * Wave schedule T5/T10/T15 x4 = placeholder until dispos_c record grammar
 * closes (never claim it as extracted data).
 * ==========================================================================*/
#define MAXU 24
#define U_TEAM_PLAYER 0
#define U_TEAM_ENEMY 1
struct WUnit {
  char pid[24];
  int gx, gy;
  float ux_i, uz_i;              /* smooth world pos (tile arrival snaps exact) */
  float yaw, yawT;               /* eased facing */
  int team, hp, mhp, pow_, skl, spd, lck, def_, res, mov;
  int wmt, whit, wcrit;          /* weapon (practice-grade v1) */
  int waited, dead, hide;
  int path[64]; int pathN;       /* flat (gx,gy) FIFO per unit */
};
static WUnit g_u[MAXU]; static int g_nu = 0;
/* [BRV-SCT-005p] FE9 analogs: 0x80330EAC lastDied + UnitsPop/Push snapshot
 * stack (FE9 buffer 0xA000 subset; we snapshot the full mirror). */
static int g_lastDied = -1;
static WUnit g_snap[6][MAXU]; static int g_snapNu[6], g_snapN = 0;
static int g_cur = 0;            /* active unit index (script moves / camera) */
static uint8_t g_cost[MAP_TW * MAP_TH]; /* baked: 1=land, 99=water */
static uint8_t g_exits[64][2]; static int g_nExits = 0;
static void scene_req(int op, SceneId id); /* defined with scene stack (below) */

/* RNG mirror [BRV-RNG-010 CLOSED]: state 3xu16; w2'=(w2<<1|w1>>15);
 * u=((w1<<11|w0>>5)^w2'); (w2,w1,w0):=(w1,w0,u) — verbatim from spec. */
static struct { uint16_t w0, w1, w2; } g_lfsr = {1, 0xC0FF, 0xEEu};
static void lfsr_seed(uint32_t s) {
  g_lfsr.w0 = (uint16_t)(s | 1u); g_lfsr.w1 = (uint16_t)((s >> 16) | 1u);
  g_lfsr.w2 = (uint16_t)(((s * 2654435761u) >> 16) | 1u);
}
static uint16_t lfsr_u(void) {
  uint16_t w0 = g_lfsr.w0, w1 = g_lfsr.w1, w2 = g_lfsr.w2;
  uint16_t nw2 = (uint16_t)(((w2 << 1) | (w1 >> 15)) & 0xFFFFu);
  uint16_t u = (uint16_t)((((w1 << 11) | (w0 >> 5)) & 0xFFFFu) ^ nw2);
  g_lfsr.w2 = w1; g_lfsr.w1 = w0; g_lfsr.w0 = u; /* (w2,w1,w0):=(w1,w0,u) */
  return u;
}
static int rng_pct(int p) { int v = ((int)lfsr_u() * 100) >> 16; return v < p; }       /* FUN_8000ce68 */
static int rng_pct2(int p) { /* FUN_8000cd88: two rolls averaged (hit rolls) */
  int v1 = ((int)lfsr_u() * 100) >> 16, v2 = ((int)lfsr_u() * 100) >> 16;
  return (v1 + v2) / 2 < p;
}

static int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
/* createTeam: -1 = lookup only; >=0 = create absent unit on that team.
 * [P4-M1-REG] team 0 (PLAYER) is a valid creation team — must not double as
 * the "don't create" flag, or every ally PID (PID_IKE) silently fails to
 * spawn and world_init dereferences g_u[-1]. */
static int wu_find(const char* pid, int createTeam) {
  for (int i = 0; i < g_nu; i++) if (!g_u[i].dead && !strcmp(g_u[i].pid, pid)) return i;
  if (createTeam < 0 || g_nu >= MAXU) return -1;
  WUnit* u = &g_u[g_nu++];
  memset(u, 0, sizeof *u);
  snprintf(u->pid, sizeof u->pid, "%s", pid ? pid : "UNIT");
  u->team = createTeam > 0 ? U_TEAM_ENEMY : U_TEAM_PLAYER;
  u->yaw = u->yawT = 45.f; u->mov = 5; u->hp = u->mhp = 1;
  return g_nu - 1;
}
static int wu_find_create(const char* pid) {
  int i = wu_find(pid, -1);
  if (i >= 0) return i;
  int enemy = !!(pid && (strstr(pid, "THIEF") || strstr(pid, "BANDIT") || strstr(pid, "ENEMY")));
  return wu_find(pid, enemy ? U_TEAM_ENEMY : U_TEAM_PLAYER);
}
static int wu_at(int gx, int gy, int team /*-1 any*/) {
  for (int i = 0; i < g_nu; i++)
    if (!g_u[i].dead && !g_u[i].hide && g_u[i].gx == gx && g_u[i].gy == gy &&
        (team < 0 || g_u[i].team == team)) return i;
  return -1;
}
static int path_push_u(WUnit* u, int gx, int gy) {
  if (u->pathN >= 32) return 0;
  u->path[u->pathN * 2] = gx; u->path[u->pathN * 2 + 1] = gy; u->pathN++; return 1;
}
static void step_tile_u(WUnit* u, int dgx, int dgy) { /* enqueue 1 tile step from tail */
  int sx = u->gx, sy = u->gy;
  if (u->pathN > 0) { sx = u->path[2 * (u->pathN - 1)]; sy = u->path[2 * (u->pathN - 1) + 1]; }
  int nx = sx + dgx, ny = sy + dgy;
  if (nx < 0 || nx >= MAP_TW || ny < 0 || ny >= MAP_TH) return;
  if (g_cost[ny * MAP_TW + nx] >= 99) return; /* baked water: no path across [BRV-MAP-001] */
  u->yawT = atan2f((float)dgx, (float)dgy) * 57.29578f;
  path_push_u(u, nx, ny);
}
/* dpad button (1 L,2 R,3 U,4 D) -> best world axis so motion matches screen
   direction under any camera yaw (iso 45 included) — used for unit AND cursor */
static int dpad_axis(int code, int* dx, int* dy) {
  float yr = g_camYaw.load() * 0.017453292f;
  float rx = cosf(yr), rz = -sinf(yr);            /* screen right in world xz */
  float ux = -sinf(yr), uz = -cosf(yr);           /* screen up (view dir, xz) */
  float s[4][2] = { {-rx, -rz}, {rx, rz}, {ux, uz}, {-ux, -uz} }; /* L R U D */
  float* sv = s[code - 1];
  int axes[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
  int best = 0; float bestDot = -1e9f;
  for (int i = 0; i < 4; i++) {
    float d = sv[0] * axes[i][0] + sv[1] * axes[i][1];
    if (d > bestDot) { bestDot = d; best = i; }
  }
  *dx = axes[best][0]; *dy = axes[best][1];
  return code >= 1 && code <= 4;
}

/* ---- world state machine ---- */
enum { WP_SELECT = 0, WP_RANGE, WP_ANIM, WP_ENEMY, WP_WON, WP_LOSE };
struct World {
  int phase;            /* WP_* */
  int cx, cy;           /* cursor tile */
  int sel;              /* selected unit index or -1 */
  int turn, spawned;    /* player turn counter + enemies spawned this chapter */
  int aiIdx, aiT;       /* enemy phase cursor + pacing countdown */
  int waveMask;         /* bit0 T5, bit1 T10, bit2 T15 already spawned */
} static g_w = { WP_SELECT, 14, 14, -1, 1, 0, 0, 0, 0 };
static int g_worldReady = 0;

/* BFS movement field for one unit. dist[tile] = cost spent to ENTER tile
 * (start excluded); blocked: water, enemy-occupied (through yes ally, stop-on
 * blocked at endpoint check). v1 all land cost 1 -> plain BFS. */
static uint8_t g_dist[MAP_TW * MAP_TH];
static int16_t g_par[MAP_TW * MAP_TH];
static void bfs_field(WUnit* u) {
  for (int i = 0; i < MAP_TW * MAP_TH; i++) { g_dist[i] = 0xFF; g_par[i] = -1; }
  uint8_t q[MAP_TW * MAP_TH]; int qh = 0, qt = 0;
  g_dist[u->gy * MAP_TW + u->gx] = 0; q[qt++] = (uint8_t)(u->gy * MAP_TW + u->gx);
  const int dxy[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
  while (qh < qt) {
    int t = q[qh++]; int tx = t % MAP_TW, ty = t / MAP_TW;
    int spent = g_dist[t];
    if (spent >= u->mov) continue;
    for (int k = 0; k < 4; k++) {
      int nx = tx + dxy[k][0], ny = ty + dxy[k][1];
      if (nx < 0 || nx >= MAP_TW || ny < 0 || ny >= MAP_TH) continue;
      int nt = ny * MAP_TW + nx;
      if (g_dist[nt] != 0xFF) continue;
      if (g_cost[nt] >= 99) continue; /* water */
      int occ = wu_at(nx, ny, -1);
      if (occ >= 0 && g_u[occ].team != u->team) continue; /* enemies block path */
      g_dist[nt] = (uint8_t)(spent + 1); g_par[nt] = (int16_t)t; q[qt++] = (uint8_t)nt;
    }
  }
}
static int in_range(WUnit* u, int gx, int gy) {
  if (gx < 0 || gx >= MAP_TW || gy < 0 || gy >= MAP_TH) return 0;
  return g_dist[gy * MAP_TW + gx] <= u->mov;
}
static void queue_bfs_path(WUnit* u, int tx, int ty) { /* walk parents -> FIFO */
  u->pathN = 0;
  int chain[64], n = 0;
  int t = ty * MAP_TW + tx;
  while (t >= 0 && g_par[t] >= 0 && n < 60) { chain[n++] = t; t = g_par[t]; }
  for (int i = n - 1; i >= 0; i--) {
    u->yawT = 45.f;
    path_push_u(u, chain[i] % MAP_TW, chain[i] / MAP_TW);
  }
}

/* combat exchange [BRV-BTL-009 mirror] */
static void strike_seq(WUnit* A, WUnit* D) {
  int hit = clampi(A->whit + A->skl + A->lck / 2 - (2 * D->spd + D->lck / 2), 0, 100);
  int crt = clampi(A->wcrit + A->skl / 2 - D->lck / 2, 0, 100);
  int strikes = 1 + (A->spd >= D->spd + 4 ? 1 : 0); /* AS+4 double rule [BRV-BTL-009] */
  for (int i = 0; i < strikes && !D->dead; i++) {
    if (!rng_pct2(hit)) { LOGI("btl: %s misses %s (hit %d%%)", A->pid, D->pid, hit); continue; }
    int dmg = clampi(A->pow_ + A->wmt - D->def_, 0, 999);
    int crit = rng_pct(crt);
    if (crit) dmg *= 3;
    D->hp -= dmg;
    LOGI("btl: %s->%s dmg=%d%s hp=%d/%d", A->pid, D->pid, dmg, crit ? " CRIT" : "",
         D->hp > 0 ? D->hp : 0, D->mhp);
    if (D->hp <= 0) { D->dead = 1; g_lastDied = (int)(D - g_u); /* [BRV-SCT-005p] 0x80330EAC */ LOGI("btl: %s DOWN", D->pid); }
  }
}
static void battle_exchange(WUnit* A, WUnit* D) {
  strike_seq(A, D);
  if (!D->dead) strike_seq(D, A); /* counter if survivor adjacent (both range 1 v1) */
}

static void world_stats(WUnit* u) { /* v1 stat bake; semantics gate = P4-CHR */
  if (u->mhp > 1) return; /* already configured (e.g. enemy wave stats) */
  if (!strcmp(u->pid, "PID_IKE")) { u->mhp = u->hp = 24; u->pow_ = 6; u->skl = 6; u->spd = 6;
    u->lck = 4; u->def_ = 4; u->res = 1; u->mov = 5; u->wmt = 5; u->whit = 90; u->wcrit = 0; }
  else { u->mhp = u->hp = 14; u->pow_ = 4; u->skl = 3; u->spd = 5; u->lck = 2;
    u->def_ = 2; u->res = 0; u->mov = 5; u->wmt = 4; u->whit = 80; u->wcrit = 5; }
}
static void spawn_wave(int turn) { /* [BRV-SCT-005j] real Ch1 enemy feed = DisposContinueSetMode
 * ("bmap01_first_n/h/m") -> generic-class tables; exact coords need runtime capture
 * (T-RE-CAP1). Until then: deterministic pool over baked maplinkAt exits. */
  if (g_nExits <= 0) { LOGE("wloop: no exits baked — wave skipped"); return; }
  int made = 0;
  for (int k = 0; k < 4; k++) {
    int e = (turn * 5 + k * 3 + turn) % g_nExits;
    int gx = g_exits[e][0], gy = g_exits[e][1];
    if (g_cost[gy * MAP_TW + gx] >= 99 || wu_at(gx, gy, -1) >= 0) continue;
    WUnit* u = nullptr; int idx = -1;
    for (int i = 0; i < MAXU; i++) if (g_u[i].dead) { idx = i; break; }
    if (idx < 0) { if (g_nu < MAXU) idx = g_nu++; else break; }
    u = &g_u[idx]; memset(u, 0, sizeof *u);
    snprintf(u->pid, sizeof u->pid, "THIEF_T%d_%d", turn, k);
    u->team = U_TEAM_ENEMY; u->gx = gx; u->gy = gy;
    u->ux_i = tile_cx(gx); u->uz_i = tile_cz(gy);
    u->yaw = u->yawT = 45.f;
    int lv = turn / 5; /* T5=1 ... T15=3 [INFERENCE] */
    u->mhp = u->hp = 6 + 3 * lv; u->pow_ = 3 + lv; u->skl = 2 + lv; u->spd = 6 + lv;
    u->lck = 1; u->def_ = 1; u->res = 0; u->mov = 5;
    u->wmt = 4; u->whit = 85; u->wcrit = 5;
    g_w.spawned++; made++;
  }
  LOGI("wloop: wave T%d spawned=%d (placeholder schedule pending dispos grammar)", turn, made);
}
static void world_init(int force) {
  if (force) { g_nu = 0; g_cur = 0; }
  int ike = -1;
  for (int i = 0; i < g_nu; i++) if (!strcmp(g_u[i].pid, "PID_IKE")) { ike = i; break; }
  if (ike < 0) {
    ike = wu_find_create("PID_IKE");
    if (ike < 0) { LOGE("wloop: cannot create PID_IKE (MAXU full)"); g_nu = 0; ike = wu_find_create("PID_IKE"); }
    g_u[ike].gx = 14; g_u[ike].gy = 14;
    g_u[ike].ux_i = tile_cx(14); g_u[ike].uz_i = tile_cz(14);
  }
  for (int i = 0; i < g_nu; i++) { world_stats(&g_u[i]); g_u[i].waited = 0; }
  g_cur = ike;
  g_w.phase = WP_SELECT; g_w.sel = -1; g_w.turn = 1; g_w.spawned = 0; g_w.aiIdx = 0;
  g_w.aiT = 0; g_w.waveMask = 0; g_w.cx = g_u[ike].gx; g_w.cy = g_u[ike].gy;
  lfsr_seed(g_rng);
  g_worldReady = 1;
  LOGI("wloop: world live units=%d turn=1 (T5/10/15 waves x4 PLACEHOLDER; win=survive T15",
       g_nu);
}
static int wu_player_alive(int team) {
  int n = 0;
  for (int i = 0; i < g_nu; i++) if (g_u[i].team == team && !g_u[i].dead && !g_u[i].hide) n++;
  return n;
}
static int all_waited(void) {
  for (int i = 0; i < g_nu; i++)
    if (g_u[i].team == U_TEAM_PLAYER && !g_u[i].dead && !g_u[i].hide && !g_u[i].waited) return 0;
  return 1;
}
static void world_end_phase(void) { /* player -> enemy */
  g_w.sel = -1; g_w.phase = WP_ENEMY; g_w.aiIdx = 0; g_w.aiT = 12;
  LOGI("wloop: turn %d enemy phase", g_w.turn);
}
static int any_moving(void) {
  for (int i = 0; i < g_nu; i++) if (!g_u[i].dead && g_u[i].pathN > 0) return 1;
  return 0;
}
/* per-frame stepper for ALL units (script-driven paths included): drains under
 * dialogs too, exactly like FE9 animating units during talk [P4-M0 invariant] */
static void world_step(void) {
  for (int i = 0; i < g_nu; i++) {
    WUnit* u = &g_u[i];
    { float d = u->yawT - u->yaw; /* FE9-style eased turns, every unit every frame */
      while (d > 180.f) d -= 360.f; while (d < -180.f) d += 360.f;
      u->yaw += d * 0.25f; }
    if (u->pathN <= 0 || u->dead) continue;
    float tx = tile_cx(u->path[0]), tz = tile_cz(u->path[1]);
    float dx = tx - u->ux_i, dz = tz - u->uz_i;
    float dl = sqrtf(dx * dx + dz * dz);
    if (dl < 0.9f) {
      u->gx = u->path[0]; u->gy = u->path[1];
      u->ux_i = tx; u->uz_i = tz; /* snap exact on tile arrival */
      for (int k = 1; k < u->pathN; k++) { u->path[2*k-2] = u->path[2*k]; u->path[2*k-1] = u->path[2*k+1]; }
      u->pathN--;
    } else { u->ux_i += dx / dl * 0.85f; u->uz_i += dz / dl * 0.85f; } /* same cadence as old single-unit stepper */
  }
}
static void world_mirror(void) {
  WUnit* u = &g_u[clampi(g_cur, 0, g_nu > 0 ? g_nu - 1 : 0)];
  g_gx = u->gx; g_gy = u->gy; g_ux = u->ux_i; g_uz = u->uz_i;
}
/* enemy AI: nearest player, greedy BFS tile minimizing manhattan, attack when
 * adjacent. SEQ_*-grammar conditions stay dispos_c P2; default aggression v1. */
static void ai_take_turn(WUnit* e) {
  int bestT = -1, bestD = 1 << 30;
  for (int i = 0; i < g_nu; i++) {
    WUnit* p = &g_u[i];
    if (p->team != U_TEAM_PLAYER || p->dead || p->hide) continue;
    int d = abs(p->gx - e->gx) + abs(p->gy - e->gy);
    if (d == 1) { battle_exchange(e, p); return; } /* adjacent now: attack, no move */
    if (d < bestD) { bestD = d; bestT = i; }
  }
  if (bestT < 0) return;
  WUnit* tgt = &g_u[bestT];
  bfs_field(e);
  int bt = e->gy * MAP_TW + e->gx, bd = bestD;
  for (int t = 0; t < MAP_TW * MAP_TH; t++) {
    if (g_dist[t] == 0xFF || g_dist[t] > e->mov) continue;
    int tx = t % MAP_TW, ty = t / MAP_TW;
    if (wu_at(tx, ty, U_TEAM_PLAYER) >= 0) continue; /* can't stop on player */
    int d = abs(tgt->gx - tx) + abs(tgt->gy - ty);
    if (d < bd) { bd = d; bt = t; }
  }
  if (bt != e->gy * MAP_TW + e->gx) queue_bfs_path(e, bt % MAP_TW, bt / MAP_TW);
  /* attack resolves next AI visit (or same tile adjacency check above) */
}
static void world_tick(void) {
  if (!g_worldReady) return;
  if (g_w.phase == WP_ANIM && !any_moving()) { /* arrival combat: adjacent enemy = auto-strike */
    WUnit* u = &g_u[g_w.sel >= 0 ? g_w.sel : g_cur];
    g_w.sel = -1; g_w.phase = WP_SELECT;
    for (int k = 0; k < 4; k++) {
      static const int dxy[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
      int e = wu_at(u->gx + dxy[k][0], u->gy + dxy[k][1], U_TEAM_ENEMY);
      if (e >= 0) { LOGI("wloop: auto-engage on arrival"); u->waited = 1; battle_exchange(u, &g_u[e]); break; }
    }
  }
  if (g_w.phase == WP_SELECT && !any_moving() && wu_player_alive(U_TEAM_PLAYER) > 0 && all_waited()) {
    g_w.aiT = 8; world_end_phase(); /* FE9 auto-switches once every unit has acted */
    return;
  }
  if (g_w.phase == WP_ENEMY) {
    if (any_moving()) return;
    if (--g_w.aiT > 0) return;
    int done = 1;
    for (int i = 0; i < g_nu; i++) {
      if (g_u[i].team != U_TEAM_ENEMY || g_u[i].dead) continue;
      if (i < g_w.aiIdx) continue;
      ai_take_turn(&g_u[i]); g_w.aiIdx = i + 1; g_w.aiT = 14; done = 0; break;
    }
    if (done) { /* enemy phase over -> new player turn + waves */
      g_w.turn++;
      int wi = g_w.turn == 5 ? 0 : g_w.turn == 10 ? 1 : g_w.turn == 15 ? 2 : -1;
      if (wi >= 0 && !(g_w.waveMask & (1 << wi))) { g_w.waveMask |= 1 << wi; spawn_wave(g_w.turn); }
      for (int i = 0; i < g_nu; i++) if (g_u[i].team == U_TEAM_PLAYER) g_u[i].waited = 0;
      g_w.phase = WP_SELECT; g_w.sel = -1;
      LOGI("wloop: turn %d player phase", g_w.turn);
    }
  }
  /* win/lose [BRV-MAP-004 MW_REACH_15 reading = survive-to-15, role-map P4] */
  int ike = -1;
  for (int i = 0; i < g_nu; i++) if (!strcmp(g_u[i].pid, "PID_IKE")) { ike = i; break; }
  if (ike >= 0 && g_u[ike].dead) { g_w.phase = WP_LOSE; g_w.sel = -1; LOGI("wloop: DEFEAT"); }
  else if (g_w.phase != WP_WON && g_w.waveMask == 7 && wu_player_alive(U_TEAM_ENEMY) == 0)
    { g_w.phase = WP_WON; g_w.sel = -1; LOGI("wloop: VICTORY (all waves cleared)"); }
  else if (g_w.phase != WP_LOSE && g_w.turn > 15 && wu_player_alive(U_TEAM_PLAYER) > 0)
    { g_w.phase = WP_WON; g_w.sel = -1; LOGI("wloop: VICTORY (survived past turn 15)"); }
}
/* A-button semantics by phase; cursor already at g_w.cx/cy */
static void world_confirm(void) {
  if (g_w.phase == WP_WON || g_w.phase == WP_LOSE) { g_worldReady = 0; g_nu = 0; scene_req(1, SC_ATTRACT); return; }
  if (g_w.phase == WP_SELECT || g_w.phase == WP_ANIM) {
    int i = wu_at(g_w.cx, g_w.cy, U_TEAM_PLAYER);
    if (i >= 0 && !g_u[i].waited) {
      g_w.sel = i; g_cur = i; bfs_field(&g_u[i]); g_w.phase = WP_RANGE;
      LOGI("wloop: select %s mov=%d", g_u[i].pid, g_u[i].mov);
    }
    return;
  }
  if (g_w.phase == WP_RANGE) {
    WUnit* u = &g_u[g_w.sel];
    if (g_w.cx == u->gx && g_w.cy == u->gy) { g_w.sel = -1; g_w.phase = WP_SELECT; return; } /* deselect */
    int e = wu_at(g_w.cx, g_w.cy, U_TEAM_ENEMY);
    if (e >= 0) { /* attack without moving (already adjacent) */
      int d = abs(u->gx - g_w.cx) + abs(u->gy - g_w.cy);
      if (d == 1) { battle_exchange(u, &g_u[e]); u->waited = 1; g_w.sel = -1; g_w.phase = WP_SELECT; }
      return;
    }
    if (in_range(u, g_w.cx, g_w.cy) && wu_at(g_w.cx, g_w.cy, -1) < 0) {
      queue_bfs_path(u, g_w.cx, g_w.cy);
      g_w.phase = WP_ANIM;
      LOGI("wloop: %s moves %d,%d (%d tiles)", u->pid, g_w.cx, g_w.cy, u->pathN);
    }
  }
}
static void world_cancel(void) {
  if (g_w.phase == WP_RANGE) { g_w.sel = -1; g_w.phase = WP_SELECT; }
  else if (g_w.phase == WP_SELECT && !any_moving()) world_end_phase(); /* B idle = end phase */
}
static void world_input(Act a) {
  if (g_paused.load()) return;
  int dx, dy;
  switch (a) {
    case ACT_LEFT: case ACT_RIGHT: case ACT_UP: case ACT_DOWN:
      if (dpad_axis((int)a, &dx, &dy)) {
        g_w.cx = clampi(g_w.cx + dx, 0, MAP_TW - 1); g_w.cy = clampi(g_w.cy + dy, 0, MAP_TH - 1);
      }
      break;
    case ACT_CONFIRM: world_confirm(); break;
    case ACT_CANCEL: world_cancel(); break;
    default: break;
  }
  if (g_w.phase == WP_RANGE) { WUnit* u = &g_u[g_w.sel]; bfs_field(u); }
}
static void tap_to_tile(float nx, float ny, int* tx, int* ty); /* below */
static void world_tap(float nx, float ny) {
  int tx = -1, ty = -1;
  tap_to_tile(nx, ny, &tx, &ty);
  if (tx < 0) return;
  g_w.cx = tx; g_w.cy = ty;
  world_confirm();
  if (g_w.phase == WP_RANGE) bfs_field(&g_u[g_w.sel >= 0 ? g_w.sel : 0]);
}
/* script-driven unit ops land on the table: natives call wu_find_create() directly */
/* screen-normalized tap (y-down) -> ground tile via analytic ray (true pitched f) */
static void tap_to_tile(float nx, float ny, int* tx, int* ty) {
  float yr = g_camYaw.load() * 0.017453292f, pit = g_camPitch.load() * 0.017453292f, dist = 55.f;
  float eye[3] = { g_camFx + sinf(yr) * cosf(pit) * dist, sinf(pit) * dist,
                   g_camFz + cosf(yr) * cosf(pit) * dist };
  float tgt[3] = { g_camFx, 8.f, g_camFz };
  float f[3] = { tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2] };
  float fl = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]); f[0] /= fl; f[1] /= fl; f[2] /= fl;
  float r[3] = { -f[2], 0.f, f[0] }; /* f x (0,1,0) = horizontal right */
  float rl = sqrtf(r[0]*r[0] + r[2]*r[2]); r[0] /= rl; r[2] /= rl;
  float up[3] = { r[1]*f[2] - r[2]*f[1], r[2]*f[0] - r[0]*f[2], r[0]*f[1] - r[1]*f[0] }; /* r x f */
  float tanh = 0.41421f, asp = g_scrW / g_scrH;
  float sx = (nx * 2.f - 1.f) * tanh * asp, sy = (1.f - ny * 2.f) * tanh;
  float d[3] = { f[0] + r[0] * sx + up[0] * sy, f[1] + r[1] * sx + up[1] * sy, f[2] + r[2] * sx + up[2] * sy };
  float dl = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]); d[0] /= dl; d[1] /= dl; d[2] /= dl;
  if (fabsf(d[1]) < 1e-3f) return;
  float t = -eye[1] / d[1];
  if (t <= 0.f) return;
  float hx = eye[0] + d[0] * t, hz = eye[2] + d[2] * t;
  int gx = (int)floorf((hx + 145.f) / TILE_W), gy = (int)floorf((hz + 120.f) / TILE_W);
  if (gx < 0) gx = 0; if (gx > MAP_TW - 1) gx = MAP_TW - 1; if (gy < 0) gy = 0; if (gy > MAP_TH - 1) gy = MAP_TH - 1;
  *tx = gx; *ty = gy;
}

static void load_asset(AAssetManager* am) {
  /* Persistent storage: loaders keep raw-data pointers (posT/name/etc);
   * a local vector here would dangle at upload time. */
  static std::vector<uint8_t> buf_g, buf_gs, buf_cms, buf_tex;
  static std::vector<uint8_t> buf_mapgs, buf_maptex;
  static std::vector<uint8_t> buf_ga[4];
  static const char* const GA_ENTRY[4] = {"archer/move_n.ga", "archer/move_bw.ga",
                                          "archer/wait_n.ga", "archer/wait_bw.ga"};
  AAsset* a = AAssetManager_open(am, "poc1.fpk", AASSET_MODE_BUFFER);
  if (!a) { LOGI("FATAL: poc1.fpk missing"); return; }
  const uint8_t* buf = (const uint8_t*)AAsset_getBuffer(a);
  off64_t sz = AAsset_getLength64(a);
  g_fpkg.assign(buf, buf + sz);
  AAsset_close(a);
  int r = fpk_open(&g_fpak, g_fpkg.data(), g_fpkg.size());
  LOGI("fpk_open=%d (0=ok)", r);
  if (r) return;
  override_init();
  g_res.init(raw_provider, 32u << 20); /* 32 MB cache budget [P3-RES-001] */
  auto rd = [&](const char* name, std::vector<uint8_t>& v) -> bool {
    const uint8_t* d; int n;
    if (!g_res.read(name, &d, &n)) { LOGI("entry %s -> MISSING", name); return false; }
    v.assign(d, d + n);
    LOGI("entry %s -> ok len=%d (resmgr)", name, n);
    return true;
  };
  rd("archer/skeleton.g", buf_g); rd("archer/body.gs", buf_gs);
  for (int i = 0; i < 4; i++) rd(GA_ENTRY[i], buf_ga[i]);
  if (!buf_g.empty() && !buf_gs.empty()) {
    if (g_load(&g_g, buf_g.data(), (int)buf_g.size()) == 0) LOGI("g: bones=%d root=%s", g_g.n, g_g.bones[0].name);
    if (gs_load(&g_gs, buf_gs.data(), (int)buf_gs.size()) == 0) {
      int tris = 0; for (int i = 0; i < g_gs.nsub; i++) tris += g_gs.subs[i].triCount;
      LOGI("gs: subs=%d tris=%d", g_gs.nsub, tris);
    }
  }
  if (!buf_ga[0].empty() && ga_load(&g_ga, buf_ga[0].data(), (int)buf_ga[0].size()) == 0) {
    LOGI("ga: bones=%d curves=%d frames=%d..%d events=%d", g_ga.nBones, g_ga.nCurves,
         g_ga.startF, g_ga.endF, g_ga.nEvents);
    for (int i = 0; i < g_ga.nEvents; i++)
      LOGI("  event f=%d codes=%04x,%04x,%04x", g_ga.events[i].frame, g_ga.events[i].codes[0],
           g_ga.events[i].codes[1], g_ga.events[i].codes[2]);
  }
  for (int i = 1; i < 4; i++)
    if (!buf_ga[i].empty()) ga_load(&g_gas[i], buf_ga[i].data(), (int)buf_ga[i].size());
  if (rd("archer/tex_4.tpl", buf_tex) && texfile_decode(buf_tex.data(), (int)buf_tex.size(), &g_texA) == 0)
    LOGI("tex_4.tpl: %d images", g_texA.count);
  if (rd("face/anna.cms", buf_cms) && texfile_decode(buf_cms.data(), (int)buf_cms.size(), &g_tex) == 0) {
    for (int i = 0; i < g_tex.count; i++)
      LOGI("tex[%d] %dx%d fmt=%d ok=%d", i, g_tex.imgs[i].w, g_tex.imgs[i].h, g_tex.imgs[i].fmt, g_tex.imgs[i].ok);
  }
  rd("bgm/boss1.stm", g_stmA); rd("bgm/boss2.stm", g_stmB);
  /* UI engine assets [P3-UI-001]: real .gcf font + window chrome tpls */
  rd("ui/talk.gcf", g_gcf);
  { std::vector<uint8_t> tb;
    if (rd("ui/fukidasi.tpl", tb) && texfile_decode(tb.data(), (int)tb.size(), &g_fuki) == 0)
      LOGI("ui: fukidasi.tpl %d imgs (%dx%d)", g_fuki.count, g_fuki.imgs[0].w, g_fuki.imgs[0].h);
    if (rd("ui/arrow.tpl", tb) && texfile_decode(tb.data(), (int)tb.size(), &g_arrow) == 0)
      LOGI("ui: arrow.tpl %d imgs (%dx%d)", g_arrow.count, g_arrow.imgs[0].w, g_arrow.imgs[0].h); }
  /* M2.2 [BRV-MAP-001]: real Chapter-1 terrain (bmap01.gs + texpack img0).
   * STATIC buffers: g_mapgs.posT/uvT alias this memory for the engine's life. */
  {
    if (rd("map/bmap01.gs", buf_mapgs) && gs_load(&g_mapgs, buf_mapgs.data(), (int)buf_mapgs.size()) == 0) {
      int tris = 0; for (int i = 0; i < g_mapgs.nsub; i++) tris += g_mapgs.subs[i].triCount;
      LOGI("map: subs=%d tris=%d", g_mapgs.nsub, tris);
    } else LOGE("map: bmap01.gs load FAIL");
    if (rd("map/texpack.tpl", buf_maptex) && texfile_decode(buf_maptex.data(), (int)buf_maptex.size(), &g_maptex) == 0)
      LOGI("map: texpack %d imgs img0=%dx%d fmt%d ok%d", g_maptex.count, g_maptex.imgs[0].w,
           g_maptex.imgs[0].h, g_maptex.imgs[0].fmt, g_maptex.imgs[0].ok);
    else LOGE("map: texpack decode FAIL");
    /* [P4-M1] movement cost grid + enemy-exit pool baked from bmap01 map.bin
     * (panelindex water panels + maplinkAt triples); see fe9-re tmp_wloop pass. */
    { std::vector<uint8_t> cb, xb;
      if (rd("maps/bmap01_cost.bin", cb) && (int)cb.size() >= MAP_TW * MAP_TH) {
        memcpy(g_cost, cb.data(), MAP_TW * MAP_TH);
        int nWater = 0; for (int i = 0; i < MAP_TW * MAP_TH; i++) if (g_cost[i] >= 99) nWater++;
        LOGI("wloop: cost grid loaded water tiles=%d/%d", nWater, MAP_TW * MAP_TH);
      } else LOGE("wloop: cost grid MISSING -> all-land fallback (water block disabled)");
      if (rd("maps/bmap01_exits.bin", xb) && xb.size() >= 2) {
        int n = (int)((xb.data()[0] | (xb.data()[1] << 8)));
        if (n > 64) n = 64;
        g_nExits = 0;
        for (int i = 0; i < n && 2 + i * 2 + 1 < (int)xb.size(); i++) {
          g_exits[g_nExits][0] = xb[2 + i * 2]; g_exits[g_nExits][1] = xb[3 + i * 2];
          if (g_exits[g_nExits][0] < MAP_TW && g_exits[g_nExits][1] < MAP_TH) g_nExits++;
        }
        LOGI("wloop: exits=%d (spawn pool, TYPE 0x0f maplinkAt)", g_nExits);
      } else LOGE("wloop: exits MISSING -> waves disabled"); }
  }
  /* P3-RES-001 prefetch demo: next-scene asset loads on the worker while the
   * engine runs; later g_res.read() must hit cache (miss stays 11). */
  g_res.prefetch("archer/tex_5.tpl");
  if (!g_stmA.empty()) { if (stm_decode(&g_sA, g_stmA.data(), (int)g_stmA.size()) == 0) LOGI("stm A sr=%d frames=%d loop=%d", g_sA.sr, g_sA.pcmFrames, g_sA.loopStart); }
  if (!g_stmB.empty()) { if (stm_decode(&g_sB, g_stmB.data(), (int)g_stmB.size()) == 0) LOGI("stm B sr=%d frames=%d", g_sB.sr, g_sB.pcmFrames); }
}

static void upload_texture() {
  tpl_file* srcs[2] = {&g_texA, &g_tex};
  for (int si = 0; si < 2; si++)
    for (int i = 0; i < srcs[si]->count; i++)
      if (srcs[si]->imgs[i].ok) {
        glGenTextures(1, &g_texId);
        glBindTexture(GL_TEXTURE_2D, g_texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, srcs[si]->imgs[i].w, srcs[si]->imgs[i].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, srcs[si]->imgs[i].rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        LOGI("texture uploaded src=%d #%d %dx%d", si, i, srcs[si]->imgs[i].w, srcs[si]->imgs[i].h);
        return;
      }
}

/* ---------- UI engine impl [P3-UI-001] ---------- */
static void ui_upload_tpl(tpl_file* t, GLuint* dst, int pick) {
  for (int i = 0; i < t->count; i++)
    if (t->imgs[i].ok && t->imgs[i].rgba) {
      if (pick > 0) { if (i < pick) continue; if (i > pick) break; }
      glGenTextures(1, dst);
      glBindTexture(GL_TEXTURE_2D, *dst);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t->imgs[i].w, t->imgs[i].h, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, t->imgs[i].rgba);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      return;
    }
}
static void ui_init(void) {
  const uint8_t white[4] = {255, 255, 255, 255};
  glGenTextures(1, &g_whiteTex);
  glBindTexture(GL_TEXTURE_2D, g_whiteTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  if (!g_gcf.empty() && ui_font_build(&g_font, g_gcf.data(), (int)g_gcf.size())) {
    int ng = 0;
    for (int i = 0; i < UI_CPN; i++) ng += g_font.has[i];
    LOGI("ui: talk.gcf atlas built (glyphs=%d lineH=%d ascent=%d)", ng, g_font.lineH, g_font.ascent);
  } else {
    LOGE("ui: FONT BUILD FAILED (gcf=%zu B) — dialog renders chrome only", g_gcf.size());
  }
  ui_upload_tpl(&g_fuki, &g_fukiTex, 1);   /* img1 = 160x24 name banner */
  ui_upload_tpl(&g_arrow, &g_arrowTex, 0); /* img0 = 12-frame cursor strip */
  if (g_tex.count > 0) ui_upload_tpl(&g_tex, &g_annaTex, 0); /* [M2.3] anna.cms portrait */
  glGenVertexArrays(1, &g_uiVao);
  glGenBuffers(1, &g_uiVbo);
  glBindVertexArray(g_uiVao);
  glBindBuffer(GL_ARRAY_BUFFER, g_uiVbo);
  glBufferData(GL_ARRAY_BUFFER, UI_MAXQ * 24 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
}
static void ui_flush(GLuint tex, const float rgba[4], int w, int h) {
  if (!g_uiB.n || !tex) { g_uiB.n = 0; return; }
  glUseProgram(g_prog3);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUniform2f(glGetUniformLocation(g_prog3, "uScr"), (float)w, (float)h);
  glUniform4fv(glGetUniformLocation(g_prog3, "uColor"), 1, rgba);
  glUniform1i(glGetUniformLocation(g_prog3, "uTex"), 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glBindVertexArray(g_uiVao);
  glBindBuffer(GL_ARRAY_BUFFER, g_uiVbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, g_uiB.n * sizeof(float), g_uiB.v);
  glDrawArrays(GL_TRIANGLES, 0, g_uiB.n / 4);
  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  g_uiB.n = 0;
}

/* dialog sample copy: LATIN placeholder text is acceptable until SJIS talk
 * tables are wired (M2.3 real strings); FONT + metrics are the real font. */
static void scene_req(int op, SceneId id);
static const char* const DLG_NAME[3] = {"ANNA", "IAN", "ANNA"};
static const char* const DLG_TEXT[3] = {
  "THE BANDIT PICKET IS CLOSING ON THE VILLAGE, CAPTAIN. SHALL WE FORM A LINE AND HOLD THEM OFF?",
  "FROM THE WESTERN CAMP I COUNT TWELVE LANCES. THREE OF THEM MAY BE ARCHERS.",
  "GIVE THE ORDERS WHEN YOU ARE READY. THE DEMON BLOOD WILL NOT WAIT FOR US.",
};
/* script-driven pages [P4-M0 / T-C1-EVT]: non-empty override the placeholder set;
 * TalkEvent fills them with the real pooled sequence string read via VM ram. */
static std::vector<std::string> g_scrDlgN, g_scrDlgT;
static inline int dlg_pages(void) { return g_scrDlgT.empty() ? 3 : (int)g_scrDlgT.size(); }
static inline const char* dlg_name(int i) { return g_scrDlgT.empty() ? DLG_NAME[i % 3] : g_scrDlgN[i].c_str(); }
static inline const char* dlg_text(int i) { return g_scrDlgT.empty() ? DLG_TEXT[i] : g_scrDlgT[i].c_str(); }
static void dialog_advance(void) {
  int shown = (int)((g_playFrames - g_dlgT0) / 2);
  int len = (int)strlen(dlg_text(g_dlgPage));
  if (!g_dlgAll && shown < len) { g_dlgAll = 1; return; } /* A = reveal all first */
  if (g_dlgPage + 1 < dlg_pages()) { g_dlgPage++; g_dlgT0 = g_playFrames; g_dlgAll = 0; }
  else scene_req(3, SC_ATTRACT);                          /* end of page stack: pop */
}
static void draw_dialog(int w, int h) {
  /* [M2.3] FE9-accurate layout (GC 480x272 ratios): translucent dark box
   * y .713.. .96, cream nameplate fukidasi[1] straddling top-left, 192x128
   * portrait (face/anna.cms img0) anchored on box top edge, white text. */
  static const float C_BODY[4] = {0.02f, 0.02f, 0.05f, 0.82f};   /* dark glass */
  static const float C_BORD[4] = {0.55f, 0.55f, 0.62f, 0.95f};   /* grey frame */
  static const float C_NAME[4] = {0.16f, 0.15f, 0.09f, 1.f};     /* olive on cream */
  static const float C_WHITE[4] = {1.f, 1.f, 1.f, 1.f};
  float bx = w * 0.033f, by = h * 0.713f, bw = w * 0.934f, bh = h * 0.247f;
  float sc = h / 540.f; /* glyph units are GC-native ~19px lines: 2x at 1080p */
  /* box: fill then 1.5%-inset double frame (FE9 double-line edge) */
  ui_batch_reset(&g_uiB);
  ui_fill(&g_uiB, bx, by, bx + bw, by + bh);
  ui_flush(g_whiteTex, C_BODY, w, h);
  ui_batch_reset(&g_uiB);
  float t = h * 0.0035f;
  ui_fill(&g_uiB, bx, by, bx + bw, by + t); ui_fill(&g_uiB, bx, by + bh - t, bx + bw, by + bh);
  ui_fill(&g_uiB, bx, by, bx + t, by + bh); ui_fill(&g_uiB, bx + bw - t, by, bx + bw, by + bh);
  ui_flush(g_whiteTex, C_BORD, w, h);
  /* portrait FIRST: face/anna.cms img0 (192x128) anchored on box top-left,
   * half above the window; nameplate (next) then overlaps its top edge = FE9 */
  float txtx = bx + bw * 0.025f, txtw = bw * 0.93f;
  float plateX = bx + bw * 0.018f;
  float plateH = bw * 0.19f * 24.f / 160.f;
  float plateY = by - plateH * 0.55f;
  bool anna = g_annaTex != 0 && strstr(dlg_name(g_dlgPage), "ANNA") != nullptr;
  if (anna) {
    float ph = bh * 0.92f, pw = ph * 192.f / 128.f;
    float px = bx + bw * 0.012f, py = by - ph * 0.52f;
    ui_batch_reset(&g_uiB);
    ui_fill(&g_uiB, px - t, py - t, px + pw + t, py + ph + t);
    ui_flush(g_whiteTex, C_BORD, w, h);
    ui_batch_reset(&g_uiB);
    ui_quad(&g_uiB, px, py, px + pw, py + ph, 0, 0, 1, 1);
    ui_flush(g_annaTex, C_WHITE, w, h);
    txtx = px + pw + bw * 0.018f; txtw = bx + bw - bw * 0.02f - txtx;
    plateX = px; plateY = py + t; /* name sits at portrait's top edge, face clear */
  }
  /* nameplate LAST-chrome: REAL fukidasi[1] 160x24 cream banner, centered name */
  if (g_fukiTex && g_fuki.count > 1 && g_fuki.imgs[1].ok) {
    float pw = bw * 0.19f, ph = plateH;
    float px = plateX, py = plateY;
    ui_batch_reset(&g_uiB);
    ui_quad(&g_uiB, px, py, px + pw, py + ph, 0, 0, 1, 1);
    ui_flush(g_fukiTex, C_WHITE, w, h);
    float ns = sc * 1.1f;
    float tw = ui_text_w(&g_font, dlg_name(g_dlgPage), ns);
    ui_batch_reset(&g_uiB);
    ui_text(&g_uiB, &g_font, px + (pw - tw) * 0.5f, py + ph * 0.32f, ns,
            dlg_name(g_dlgPage), pw, 1, 1 << 30);
    ui_flush(g_font.tex, C_NAME, w, h);
  }
  /* body: word-wrap + typewriter (2 chars/frame, A to reveal); FE9 = light text */
  int visible = g_dlgAll ? (1 << 30) : (int)((g_playFrames - g_dlgT0) / 2);
  ui_batch_reset(&g_uiB);
  ui_text(&g_uiB, &g_font, txtx, by + bh * 0.14f, sc,
          dlg_text(g_dlgPage), txtw, 4, visible);
  ui_flush(g_font.tex, C_WHITE, w, h);
  /* next-page cursor: real arrow.tpl 12-frame anim at 8fps */
  if (g_arrowTex && g_arrow.imgs[0].ok) {
    int fr = (int)(g_playFrames / 8) % 12;
    float aw = h * 0.014f, ah = aw * g_arrow.imgs[0].h * 12.f / g_arrow.imgs[0].w;
    float ax = bx + bw - aw * 2.2f, ay = by + bh - ah * 1.8f;
    ui_batch_reset(&g_uiB);
    ui_quad(&g_uiB, ax, ay, ax + aw, ay + ah, fr / 12.f, 0, (fr + 1) / 12.f, 1);
    ui_flush(g_arrowTex, C_WHITE, w, h);
  }
}

/* ground grid: white 2x2 tex + line VBO reusing the mesh attribute layout */
static GLuint g_gridVao = 0, g_gridVbo = 0, g_texWhite = 0; static int g_gridN = 0;

/* [P4-M1] world HUD: turn/phase strip + enemy count + selected-unit stat card.
 * Same ui_* batch + real FE9 font as the dialog layer (no new texture). */
static void draw_world_hud(int w, int h) {
  if (!g_font.ok) return;
  static const float C_STRIP[4] = { .02f, .02f, .06f, .62f };
  static const float C_TEXT[4] = { .92f, .90f, .80f, 1.f };
  static const float C_ENEMY[4] = { .95f, .45f, .40f, 1.f };
  /* sc = font-native multiplier (glyphs ~19px); h*0.024 (=26) here rendered
   * glyphs ~17x too big so TURN/ENEMIES ballooned and overlapped mid-screen.
   * Dialog uses h/540 (=2.0); HUD uses h/720 (=1.5) for a slightly smaller strip. */
  float sc = h / 720.f;
  float lineH = (float)(g_font.lineH ? g_font.lineH : 20) * sc;
  float strip = 4.f + 2.f * lineH + 6.f; /* two full line boxes + pad */
  const char* ph = g_w.phase == WP_SELECT ? "PLAYER PHASE" : g_w.phase == WP_RANGE ? "SELECT DEST"
               : g_w.phase == WP_ANIM ? "MOVING" : g_w.phase == WP_ENEMY ? "ENEMY PHASE"
               : g_w.phase == WP_WON ? "VICTORY" : "DEFEAT";
  char buf[128];
  ui_batch_reset(&g_uiB);
  ui_fill(&g_uiB, 0.f, 0.f, (float)w, strip);
  ui_flush(g_texWhite, C_STRIP, w, h);
  snprintf(buf, sizeof buf, "TURN %d  %s", g_w.turn, ph);
  ui_batch_reset(&g_uiB);
  ui_text(&g_uiB, &g_font, 8.f, 4.f, sc, buf, (float)w, 1, 1 << 30);
  ui_flush(g_font.tex, C_TEXT, w, h);
  snprintf(buf, sizeof buf, "ENEMIES %d", wu_player_alive(U_TEAM_ENEMY));
  ui_batch_reset(&g_uiB);
  ui_text(&g_uiB, &g_font, 8.f, 4.f + lineH, sc, buf, (float)w, 1, 1 << 30);
  ui_flush(g_font.tex, C_ENEMY, w, h);
  WUnit* u = (g_w.sel >= 0 ? &g_u[g_w.sel] : (g_cur < g_nu ? &g_u[g_cur] : nullptr));
  if (u && !u->dead) {
    snprintf(buf, sizeof buf, "%s HP %d/%d  P%d K%d S%d L%d D%d R%d M%d",
             u->pid, u->hp, u->mhp, u->pow_, u->skl, u->spd, u->lck, u->def_, u->res, u->mov);
    float tw = ui_text_w(&g_font, buf, sc);
    ui_batch_reset(&g_uiB);
    ui_fill(&g_uiB, w - tw - 16.f, 2.f, (float)w, lineH + 10.f);
    ui_flush(g_texWhite, C_STRIP, w, h);
    ui_batch_reset(&g_uiB);
    ui_text(&g_uiB, &g_font, w - tw - 8.f, 4.f, sc, buf, (float)w, 1, 1 << 30);
    ui_flush(g_font.tex, C_TEXT, w, h);
  }
  if (g_w.phase == WP_SELECT || g_w.phase == WP_RANGE) {
    snprintf(buf, sizeof buf, "A %s  B END PHASE",
             g_w.phase == WP_RANGE ? "GO" : "SELECT UNIT");
    ui_batch_reset(&g_uiB);
    ui_text(&g_uiB, &g_font, 8.f, h - lineH - 6.f, sc, buf, (float)w, 1, 1 << 30);
    ui_flush(g_font.tex, C_TEXT, w, h);
  }
}
static void build_grid() {
  glGenTextures(1, &g_texWhite);
  glBindTexture(GL_TEXTURE_2D, g_texWhite);
  const unsigned char w4[16] = {220, 220, 230, 255, 220, 220, 230, 255,
                                220, 220, 230, 255, 220, 220, 230, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, w4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); /* no mips: default mipmap filter = incomplete = black */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  std::vector<float> V;
  /* M2.2 [BRV-MAP-001]: grid aligned to map model: 29x24 tiles of 10 units,
   * world = mapX-145, mapZ-120 (x -145..145, z -120..120). */
  for (int k = 0; k <= 29; k++) {
    float c = -145.f + 10.f * k;
    V.insert(V.end(), {c, 0, -120.f, 0, 1, 0, 0, 0, c, 0, 120.f, 0, 1, 0, 0, 0});
  }
  for (int m = 0; m <= 24; m++) {
    float c = -120.f + 10.f * m;
    V.insert(V.end(), {-145.f, 0, c, 0, 1, 0, 0, 0, 145.f, 0, c, 0, 1, 0, 0, 0});
  }
  g_gridN = (int)(V.size() / 8);
  glGenVertexArrays(1, &g_gridVao); glBindVertexArray(g_gridVao);
  glGenBuffers(1, &g_gridVbo); glBindBuffer(GL_ARRAY_BUFFER, g_gridVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(V.size() * sizeof(float)), V.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
  LOGI("grid built verts=%d", g_gridN);
}

/* M2.2 [BRV-MAP-001]: static terrain VBO. bmap01 model coords (0..290, y,
 * 0..240) shifted to center; tile pitch 10 units == engine TILE_W. No .ga ->
 * gs_vertex early-outs to raw positions (n=(0,1,0), flat terrain lighting). */
static void upload_map() {
  std::vector<float> V;
  LOGI("map dbg: fbPos=%d fbNrm=%d fbUv=%d npos=%d posToff=%d uvToff=%d sub0 bone=%d flags=%x attr=%d tris=%d",
       g_mapgs.fbPos, g_mapgs.fbNrm, g_mapgs.fbUv, g_mapgs.npos,
       (int)(g_mapgs.posT - g_mapgs.d), (int)(g_mapgs.uvT - g_mapgs.d),
       g_mapgs.subs[0].bone, g_mapgs.subs[0].flags, g_mapgs.subs[0].attr, g_mapgs.subs[0].triCount);
  {
    const gs_submesh* s = &g_mapgs.subs[0];
    float p[3], n[3];
    gs_vertex(&g_mapgs, s, s->tris[0].v[0], nullptr, &g_mapnobones, p, n);
    LOGI("map dbg v0: pidx=%d nidx=%d uidx=%d raw=%d,%d,%d out=%.2f,%.2f,%.2f",
         s->tris[0].v[0].p, s->tris[0].v[0].n, s->tris[0].v[0].u,
         gs_bei16(g_mapgs.posT, s->tris[0].v[0].p * 6), gs_bei16(g_mapgs.posT, s->tris[0].v[0].p * 6 + 2),
         gs_bei16(g_mapgs.posT, s->tris[0].v[0].p * 6 + 4), p[0], p[1], p[2]);
  }
  for (int si = 0; si < g_mapgs.nsub; si++) {
    const gs_submesh* s = &g_mapgs.subs[si];
    for (int t = 0; t < s->triCount; t++)
      for (int k = 0; k < 3; k++) {
        float p[3], n[3], uv[2];
        gs_vertex(&g_mapgs, s, s->tris[t].v[k], nullptr, &g_mapnobones, p, n);
        gs_uv(&g_mapgs, s->tris[t].v[k], uv);
        V.insert(V.end(), {p[0] - 145.f, p[1], p[2] - 120.f, n[0], n[1], n[2], uv[0], uv[1]});
      }
  }
  g_mapTris = (int)(V.size() / 8 / 3);
  float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
  for (size_t i = 0; i + 7 < V.size(); i += 8)
    for (int k = 0; k < 3; k++) { float v = V[i + k]; if (v < mn[k]) mn[k] = v; if (v > mx[k]) mx[k] = v; }
  LOGI("map mesh built tris=%d bbox x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f",
       g_mapTris, mn[0], mx[0], mn[1], mx[1], mn[2], mx[2]);
  glGenVertexArrays(1, &g_mapVao); glBindVertexArray(g_mapVao);
  glGenBuffers(1, &g_mapVbo); glBindBuffer(GL_ARRAY_BUFFER, g_mapVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(V.size() * sizeof(float)), V.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
  glBindVertexArray(0);
}

static void upload_mesh() {
  std::vector<float> verts;
  mat34 local[G_MAX_BONES], worlds[G_MAX_BONES];
  for (int i = 0; i < g_g.n; i++) memcpy(local[i].m, g_g.bones[i].seed, sizeof(float) * 12);
  g_worlds(&g_g, local, worlds); /* seed LOCAL -> world (g.mjs seedWorlds) */
  for (int si = 0; si < g_gs.nsub; si++) {
    const gs_submesh* s = &g_gs.subs[si];
    for (int t = 0; t < s->triCount; t++)
      for (int k = 0; k < 3; k++) {
        float p[3], n[3], uv[2];
        gs_vertex(&g_gs, s, s->tris[t].v[k], worlds, &g_g, p, n);
        gs_uv(&g_gs, s->tris[t].v[k], uv);
        if (si == 0 && t == 0 && k == 0) {
          int bn = s->bone;
          if ((s->flags & 0x0002) && s->palette) bn = s->palette[2 + s->tris[t].v[k].sk / 3];
          LOGI("v0 sub0 p=%d sk=%d bone=%d flags=%x pal0=%d raw=%d,%d,%d out=%.3f,%.3f,%.3f seedW=%.3f,%.3f,%.3f",
               s->tris[t].v[k].p, s->tris[t].v[k].sk, bn, s->flags, s->palette ? s->palette[1] : -1,
               gs_bei16(g_gs.posT, s->tris[t].v[k].p * 6), gs_bei16(g_gs.posT, s->tris[t].v[k].p * 6 + 2),
               gs_bei16(g_gs.posT, s->tris[t].v[k].p * 6 + 4), p[0], p[1], p[2],
               worlds[bn].m[3], worlds[bn].m[7], worlds[bn].m[11]);
        }
        verts.insert(verts.end(), {p[0], p[1], p[2], n[0], n[1], n[2], uv[0], uv[1]});
      }
  }
  g_triCount = (int)(verts.size() / 8 / 3);
  float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
  for (size_t i = 0; i + 7 < verts.size(); i += 8)
    for (int k = 0; k < 3; k++) { float v = verts[i + k]; if (v < mn[k]) mn[k] = v; if (v > mx[k]) mx[k] = v; }
  LOGI("bbox x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f", mn[0], mx[0], mn[1], mx[1], mn[2], mx[2]);
  glGenVertexArrays(1, &g_vao); glBindVertexArray(g_vao);
  glGenBuffers(1, &g_vbo); glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
  LOGI("mesh uploaded tris=%d", g_triCount);
}

static void audio_start() {
  oboe::AudioStreamBuilder ob;
  ob.setDirection(oboe::Direction::Output)
    ->setFormat(oboe::AudioFormat::I16)
    ->setChannelCount(oboe::ChannelCount::Stereo)
    ->setFramesPerDataCallback(960)
    ->setDataCallback(&g_mixer)
    ->setErrorCallback(&g_mixer);
  oboe::Result r = ob.openStream(&g_mixer.stream);
  if (r == oboe::Result::OK) {
    g_mixer.bindBgm(0, g_sA); g_mixer.bindBgm(1, g_sB);
    auto rr = g_mixer.stream->requestStart();
    LOGI("oboe start=%d rate=%d frames=%d", (int)rr, g_mixer.stream->getSampleRate(), g_mixer.stream->getFramesPerBurst());
  } else LOGI("oboe open fail=%d", (int)r);
}

// ---------- loop ----------
static void draw_buttons(int w, int h);
static void render(int w, int h) {
  /* live size: surface resizes (bar hide / rotation / foldable) = GL adapts */
  EGLint ww, hh;
  if (eglQuerySurface(dpy, surf, EGL_WIDTH, &ww) && eglQuerySurface(dpy, surf, EGL_HEIGHT, &hh)
      && ww > 0 && hh > 0) { w = ww; h = hh; }
  glViewport(0, 0, w, h);
  glClearColor(0.06f, 0.07f, 0.1f, 1.f);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float aspect = (float)w / h;
  float fovy = 45.f * 3.14159265f / 180.f, near = 0.1f, far = 1000.f;
  float f = 1.f / tanf(fovy / 2);
  mat4 proj = {};
  proj.m[0] = f / aspect; proj.m[5] = f; proj.m[10] = (far + near) / (near - far);
  proj.m[11] = -1; proj.m[14] = 2 * far * near / (near - far);
  float yaw = g_camYaw.load() * 3.14159265f / 180.f, pit = g_camPitch.load() * 3.14159265f / 180.f, dist = g_camDist;
  float ex = g_camFx + sinf(yaw) * cosf(pit) * dist, ey = sinf(pit) * dist, ez = g_camFz + cosf(yaw) * cosf(pit) * dist;
  float fx = g_camFx, fy = 8, fz = g_camFz;
  // view = lookAt (column-major, GL)
  float zx = ex - fx, zy = ey - fy, zz = ez - fz;
  float zl = sqrtf(zx * zx + zy * zy + zz * zz); zx /= zl; zy /= zl; zz /= zl;
  float xx = zz, xy = 0, xz = -zx; // x = up×z, proper handedness (up=(0,1,0))
  float xl = sqrtf(xx * xx + xz * xz); xx /= xl; xz /= xl;
  float yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
  mat4 view = {};
  view.m[0] = xx; view.m[4] = xy; view.m[8] = xz;  view.m[12] = -(xx * ex + xy * ey + xz * ez);
  view.m[1] = yx; view.m[5] = yy; view.m[9] = yz;  view.m[13] = -(yx * ex + yy * ey + yz * ez);
  view.m[2] = zx; view.m[6] = zy; view.m[10] = zz; view.m[14] = -(zx * ex + zy * ey + zz * ez);
  view.m[15] = 1;
  g_scrW = (float)w; g_scrH = (float)h;
  mat4 vp = m4mul(proj, view);
  float dim = (1.f - fmaxf(g_fDim, scr_fade())) * g_sceneDim;

  /* [P4-M1] every pass shares one program setup; per-draw uMVP + team tint.
   * Unit transform: Ry(u->yaw)*translate; model front +Z (vision-verified M2). */
  glUseProgram(g_prog);
  GLint luDim = glGetUniformLocation(g_prog, "uDim"), luMVP = glGetUniformLocation(g_prog, "uMVP");
  GLint luTint = glGetUniformLocation(g_prog, "uTint"), luAl = glGetUniformLocation(g_prog, "uAlpha");
  glUniform1f(luDim, dim); glUniform3f(luTint, 1.f, 1.f, 1.f); glUniform1f(luAl, 1.f);
  glUniform1i(glGetUniformLocation(g_prog, "uTex"), 0);
  glActiveTexture(GL_TEXTURE0);

  /* M2.2 [BRV-MAP-001]: real bmap01 terrain under everything */
  if (g_mapTris && g_mapTex) {
    glUniformMatrix4fv(luMVP, 1, GL_FALSE, vp.m);
    glBindTexture(GL_TEXTURE_2D, g_mapTex);
    glBindVertexArray(g_mapVao);
    glDrawArrays(GL_TRIANGLES, 0, g_mapTris * 3);
  }
  /* unit pass: whole table (script-walked opening actors ride the same list;
   * g_nu is engine-thread-only so no lock). dead/hide skipped. */
  if (g_triCount && g_texId) {
    glBindTexture(GL_TEXTURE_2D, g_texId);
    glBindVertexArray(g_vao);
    for (int i = 0; i < g_nu; i++) {
      WUnit* u = &g_u[i];
      if (u->dead || u->hide) continue;
      float yc = cosf(u->yaw * 0.017453292f), ys = sinf(u->yaw * 0.017453292f);
      mat4 model = {};
      model.m[0] = yc; model.m[2] = -ys; model.m[5] = 1; model.m[8] = ys; model.m[10] = yc; model.m[15] = 1;
      model.m[12] = u->ux_i; model.m[14] = u->uz_i;
      mat4 mvp = m4mul(vp, model);
      glUniformMatrix4fv(luMVP, 1, GL_FALSE, mvp.m);
      float tint[3] = { 1.f, 1.f, 1.f };
      if (u->team == U_TEAM_ENEMY) { tint[0] = 1.f; tint[1] = .45f; tint[2] = .4f; }
      if (i == g_w.sel) { tint[0] = 1.f; tint[1] = 1.f; tint[2] = .4f; }
      else if (u->team == U_TEAM_PLAYER && u->waited) { tint[0] = .6f; tint[1] = .62f; tint[2] = .7f; }
      glUniform3fv(luTint, 1, tint);
      glDrawArrays(GL_TRIANGLES, 0, g_triCount * 3);
    }
    glUniform3f(luTint, 1.f, 1.f, 1.f);
  }
  if (g_gridN) { /* grid: line VBO, no unit matrix */
    glUniformMatrix4fv(luMVP, 1, GL_FALSE, vp.m);
    glBindTexture(GL_TEXTURE_2D, g_texWhite);
    glBindVertexArray(g_gridVao);
    glDrawArrays(GL_LINES, 0, g_gridN);
  }
  /* [P4-M1] dynamic overlay: movement range + cursor + HP bars, one alpha batch.
   * Flat ground quads; iso pitch (~24-35 deg) keeps them readable. Own tiny
   * program: pos3+rgb3 (mesh layout has no color attribute). */
  if (g_worldReady) {
    static GLuint ovProg = 0, ovVao = 0, ovVbo = 0; static GLint ovMVP = -1, ovAl = -1;
    if (!ovProg) {
      const char* OVS = "#version 300 es\n"
        "layout(location=0) in vec3 aPos; layout(location=1) in vec3 aCol;"
        "uniform mat4 uMVP; out vec3 vC;"
        "void main(){ vC=aCol; gl_Position=uMVP*vec4(aPos,1.0); }";
      const char* OFS = "#version 300 es\n"
        "precision mediump float; in vec3 vC; uniform float uA; out vec4 o;"
        "void main(){ o=vec4(vC,uA); }";
      ovProg = makeProgram(OVS, OFS);
      ovMVP = glGetUniformLocation(ovProg, "uMVP"); ovAl = glGetUniformLocation(ovProg, "uA");
      glGenVertexArrays(1, &ovVao); glGenBuffers(1, &ovVbo);
      glBindVertexArray(ovVao); glBindBuffer(GL_ARRAY_BUFFER, ovVbo);
      glBufferData(GL_ARRAY_BUFFER, 240 * 36 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
      glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    }
    static float ov[240 * 36]; static int nq;
    nq = 0;
    auto quad = [&](float x0, float z0, float x1, float z1, float y, float r, float g, float b) {
      if (nq >= 240) return;
      float c[6][2] = { {x0,z0}, {x1,z0}, {x1,z1}, {x0,z0}, {x1,z1}, {x0,z1} };
      for (int i = 0; i < 6; i++) {
        float* p = ov + nq * 36 + i * 6;
        p[0] = c[i][0]; p[1] = y; p[2] = c[i][1]; p[3] = r; p[4] = g; p[5] = b;
      }
      nq++;
    };
    if (g_w.phase == WP_RANGE && g_w.sel >= 0) {
      WUnit* u = &g_u[g_w.sel];
      for (int t = 0; t < MAP_TW * MAP_TH; t++) {
        if (g_dist[t] == 0xFF || g_dist[t] > u->mov) continue;
        int tx = t % MAP_TW, ty = t / MAP_TW;
        if (g_cost[t] >= 99 || wu_at(tx, ty, -1) >= 0) continue;
        quad(tile_cx(tx) - 4.7f, tile_cz(ty) - 4.7f, tile_cx(tx) + 4.7f, tile_cz(ty) + 4.7f, 0.35f, .24f, .36f, .86f);
      }
    }
    if (g_w.phase != WP_WON && g_w.phase != WP_LOSE) {
      float cx = tile_cx(g_w.cx), cz = tile_cz(g_w.cy);
      quad(cx - 4.9f, cz - 4.9f, cx + 4.9f, cz + 4.9f, 0.55f, .95f, .9f, .3f); /* cursor */
    }
    for (int i = 0; i < g_nu; i++) { /* HP bars over wounded units */
      WUnit* u = &g_u[i];
      if (u->dead || u->hide || u->hp >= u->mhp || u->mhp <= 0) continue;
      float x = u->ux_i, z = u->uz_i + 5.2f;
      quad(x - 4.4f, z - .8f, x + 4.4f, z + .8f, 2.6f, .12f, .03f, .03f);
      float r = (float)clampi(u->hp, 0, u->mhp) / (float)u->mhp;
      quad(x - 4.2f, z - .5f, x - 4.2f + 8.4f * r, z + .5f, 2.7f, u->team == U_TEAM_PLAYER ? .15f : .85f,
           u->team == U_TEAM_PLAYER ? .8f : .2f, .2f);
    }
    if (nq) {
      glUseProgram(ovProg);
      glUniformMatrix4fv(ovMVP, 1, GL_FALSE, vp.m);
      glUniform1f(ovAl, .45f * (1.f - fmaxf(g_fDim, scr_fade())));
      glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_DEPTH_TEST);
      glBindBuffer(GL_ARRAY_BUFFER, ovVbo);
      glBufferSubData(GL_ARRAY_BUFFER, 0, nq * 36 * sizeof(float), ov);
      glBindVertexArray(ovVao);
      glDrawArrays(GL_TRIANGLES, 0, nq * 6);
      glDisable(GL_BLEND);
      glEnable(GL_DEPTH_TEST);
    }
  }
  if (g_sm.top() == SC_DIALOG && g_font.ok) draw_dialog(w, h); /* [P3-UI-001] */
  if (g_worldReady) draw_world_hud(w, h); /* [P4-M1] turn / team / phase strip */
  draw_buttons(w, h);
  eglSwapBuffers(dpy, surf);
}

/* ---------- GL-drawn touch overlay ----------
 * Canonical button layout in normalized coords (y down). Hit-test + visuals
 * share one source of truth; held[] drives continuous actions (rapid fire). */
static const struct { float cx, cy; int code; } BTN[7] = {
  {0.09f, 0.72f, 1}, {0.25f, 0.72f, 2}, {0.17f, 0.585f, 3}, {0.17f, 0.855f, 4},
  {0.88f, 0.76f, 5}, {0.755f, 0.885f, 6}, {0.88f, 0.615f, 7},
};
#define BR 0.070f /* hit radius, fraction of screen height */
static std::atomic<int> g_held[8];
static std::atomic<int> g_slotCode[16];  /* pointerId -> held code (0 = none) */

/* Geometry: per button a square backdrop + shape marker (tri = direction,
 * fan = A/B). Single static VBO [x,y,buttonIdx], pixel coords, y-down. */
static GLuint g_btnVao = 0, g_btnVbo = 0; static int g_btnN = 0;

static void build_button_geometry(int w, int h) {
  std::vector<float> V;
  auto put = [&](float x, float y, float idx) { V.insert(V.end(), {x, y, idx}); };
  auto quad = [&](float cx, float cy, float r, float idx) {
    float x0 = (cx - r) * w, x1 = (cx + r) * w, y0 = (cy - r) * h, y1 = (cy + r) * h;
    put(x0, y0, idx); put(x1, y0, idx); put(x1, y1, idx);
    put(x0, y0, idx); put(x1, y1, idx); put(x0, y1, idx);
  };
  auto tri = [&](float cx, float cy, float r, float ang, float idx) { /* outward marker */
    float px = cx * w, py = cy * h;
    float ax = cosf(ang), ay = sinf(ang);          /* outward dir (y-down space) */
    float bx = -ay, by = ax;                       /* perp */
    float tx = px + ax * r * w * 0.55f, ty = py + ay * r * w * 0.55f;
    float b1x = px - ax * r * w * 0.35f + bx * r * w * 0.4f, b1y = py - ay * r * w * 0.35f + by * r * w * 0.4f;
    float b2x = px - ax * r * w * 0.35f - bx * r * w * 0.4f, b2y = py - ay * r * w * 0.35f - by * r * w * 0.4f;
    put(tx, ty, idx); put(b1x, b1y, idx); put(b2x, b2y, idx);
  };
  auto disc = [&](float cx, float cy, float r, float idx, int seg) {
    float px = cx * w, py = cy * h, rr = r * h * 0.55f;
    for (int i = 0; i < seg; i++) {
      float a0 = 6.2831853f * i / seg, a1 = 6.2831853f * (i + 1) / seg;
      put(px, py, idx);
      put(px + cosf(a0) * rr, py + sinf(a0) * rr, idx);
      put(px + cosf(a1) * rr, py + sinf(a1) * rr, idx);
    }
  };
  /* BTN order: 1 left, 2 right, 3 up, 4 down */
  for (int i = 0; i < 4; i++) {
    quad(BTN[i].cx, BTN[i].cy, BR, (float)BTN[i].code);
    float a = (BTN[i].code == 1) ? 3.1415927f : (BTN[i].code == 2) ? 0.f
            : (BTN[i].code == 3) ? -1.5707963f : 1.5707963f;
    tri(BTN[i].cx, BTN[i].cy, BR, a, (float)BTN[i].code);
  }
  disc(BTN[4].cx, BTN[4].cy, BR, 5, 14);  /* A big disc */
  disc(BTN[5].cx, BTN[5].cy, BR * 0.85f, 6, 14); /* B smaller */
  disc(BTN[6].cx, BTN[6].cy, BR * 0.62f, 7, 4);  /* T (texture cycle): 4-seg diamond */
  g_btnN = (int)(V.size() / 3);
  glGenVertexArrays(1, &g_btnVao); glBindVertexArray(g_btnVao);
  glGenBuffers(1, &g_btnVbo); glBindBuffer(GL_ARRAY_BUFFER, g_btnVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(V.size() * sizeof(float)), V.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
  LOGI("button geometry tris=%d", g_btnN / 3);
}

static void draw_buttons(int w, int h) {
  if (!g_prog2) return;
  static int cw = 0, chh = 0;
  if (g_btnN == 0 || cw != w || chh != h) { build_button_geometry(w, h); cw = w; chh = h; }
  glUseProgram(g_prog2);
  GLuint lu = glGetUniformLocation(g_prog2, "uScr");
  glUniform2f(lu, (float)w, (float)h);
  float held[8] = {};
  for (int i = 1; i <= 7; i++) held[i] = (float)g_held[i].load();
  glUniform1fv(glGetUniformLocation(g_prog2, "uHeld"), 8, held);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glBindVertexArray(g_btnVao);
  glDrawArrays(GL_TRIANGLES, 0, g_btnN);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}

/* ---------- scene plumbing [P3-SCN-001] ----------
 * Ops queue and execute at fade==1 so every scene change is an invisible cut. */
static void script_begin(void); /* [P4-M0] defined in script module below */
static void scene_req(int op, SceneId id) {
  if (g_fd == FD_NONE) { g_fd = FD_OUT; g_fdOp = op; g_fdId = id; }
}
static void scene_enter(SceneId s) {
  if (s == SC_DIALOG) { g_dlgPage = 0; g_dlgT0 = g_playFrames; g_dlgAll = 0; }
  if (s == SC_SCRIPT) { g_scrDlgN.clear(); g_scrDlgT.clear(); script_begin(); }
  g_sceneDim = (s == SC_ATTRACT || s == SC_WORLD || s == SC_SCRIPT) ? 1.f : 0.5f;
  g_mixer.bgmMix(s == SC_BATTLE ? 1.f : 0.f);          /* battle = boss theme crossfade */
  g_mixer.setDuck(s == SC_DIALOG ? 0.35f : 1.f);       /* dialog ducks BGM for voice clarity [P3-AUD-001] */
  LOGI("scene: enter %s depth=%d dim=%.2f bossMix=%d duck=%.2f", scene_name(s), g_sm.depth(), g_sceneDim, s == SC_BATTLE, s == SC_DIALOG ? 0.35 : 1.0);
}
static void scene_tick() {
  if (g_fd == FD_OUT) {
    g_fDim += 0.083f; /* ~12 frames @60 */
    if (g_fDim >= 1.f) {
      g_fDim = 1.f;
      SceneId was = g_sm.top();
      switch (g_fdOp) { case 1: g_sm.replace(g_fdId); break; case 2: g_sm.push(g_fdId); break; case 3: g_sm.pop(); break; }
      scene_enter(g_sm.top());
      LOGI("scene: %s -> %s", scene_name(was), scene_name(g_sm.top()));
      g_fd = FD_IN;
    }
  } else if (g_fd == FD_IN) {
    g_fDim -= 0.083f;
    if (g_fDim <= 0.f) { g_fDim = 0.f; g_fd = FD_NONE; }
  }
  SceneId s = g_sm.top();
  float dT = s == SC_DIALOG ? 22.f : s == SC_BATTLE ? 30.f : s == SC_ATTRACT ? 40.f : 110.f;
  g_camDist += (dT - g_camDist) * 0.05f;
  if (s == SC_ATTRACT) g_camYaw.store(g_camYaw.load() + 0.15f);  /* slow showcase orbit */
  else if (s == SC_BATTLE) g_camYaw.store(g_camYaw.load() + 0.4f);
}

/* ---------- script scene [P4-M0 / T-C1-EVT / BRV-SCT-005] ----------
 * The real C01.cmb (Opening0 prologue) runs on the P3-M3 VM inside the engine.
 * Natives are the on-device bridge: names+argc from docs/data natreg (baked);
 * every call logs (chain audit vs FE9_C01_OPENING_GOLD.csv); yielding natives
 * (Wait/FadeWait/TalkEvent/move-wait) park the coroutine across frames exactly
 * like the GC thread model. Strings pass as RAM pool addresses: read via
 * rdCstr, never host pointers [BRV-SCT-005 design note]. */
struct ScriptSim {
  vm::Vm vm;
  vm::Cmb cmb;
  std::deque<std::string> names; /* stable storage for Native::name */
  vm::Thread* vt = nullptr;
  bool loaded = false;
  float fade = 0.f, fadeTo = 0.f, fadeSpd = 0.f; /* script black overlay [Fade mode1/2] */
  uint64_t waitUntil = 0;                        /* latched Wait/FadeWait deadline */
  uint32_t dlgSeq = 0;                           /* ram addr of active TalkEvent string */
  int walkSlow = 0;
  uint64_t lastLog = 0;                          /* frame of last scr_natlog line (wedge detector) */
  bool ringDumped = false;                       /* one-shot op-ring dump on wedge */
};
static ScriptSim g_scr;
static inline float scr_fade(void) { return g_scr.fade; }

static void scr_vblog(void* u, const char* msg) { (void)u; LOGI("vm %s", msg); }
static const char* sarg(vm::Vm& V, uint32_t a, char* buf) {
  const char* s = a < vm::RAM_SIZE ? V.rdCstr(a, buf, 256) : nullptr;
  return s && *s ? s : (a < vm::RAM_SIZE ? "" : "<int>");
}
/* full native-call chain audit, re-entry pump lines (same name+args, consecutive
 * frame) logged once so logcat stays readable on WaitM's 600+ re-entries */
static void ring_to_file(const char* name) {
  FILE* fp = fopen(name, "w");
  if (!fp) fp = fopen("/data/data/com.fe9.poc/files/trace_ring.txt", "w"); /* private dir: always writable */
  if (!fp) { LOGI("scr ring dump: fopen failed errno=%d", errno); return; }
  const vm::Vm& V = g_scr.vm;
  uint32_t n = V.ringI < 16384 ? V.ringI : 16384, start = V.ringI - n;
  fprintf(fp, "# ring n=%u ringI=%u frame=%llu\n", n, V.ringI, (unsigned long long)V.frame);
  for (uint32_t i = start; i < V.ringI; i++) {
    auto& e = V.ring[i & 16383];
    fprintf(fp, "%s @0x%x op=%02x sp=%u\n", e.file ? e.file : "?", e.pc, e.op, e.sp);
  }
  fclose(fp);
  LOGI("scr ring dump ok n=%u", n);
}
static void scr_natlog(vm::Vm& V, int ni, const uint32_t* args, int argc) {
  static const char* pn = nullptr; static uint32_t pa0, pa1;
  const char* n = V.natives[ni].name;
  uint32_t a0 = args[0], a1 = argc > 1 ? args[1] : 0;
  g_scr.lastLog = V.frame; /* any call = chain alive, even deduped pump re-entries */
  static int streakN;
  if (pn && strcmp(pn, n) == 0 && pa0 == a0 && pa1 == a1) {
    /* same call hammered: 4th time = loop signature, dump op ring once for pc-level view */
    if (++streakN == 3) {
      static int dumps = 0;
      if (dumps < 10) {
        LOGI("scr LOOPSTREAK x%d %s -> ring file dump", streakN, n);
        ring_to_file("trace_ring.txt"); /* logd rate-limits bursts; file keeps full fidelity */
        dumps++;
      }
    }
    return;
  } /* streak: re-entry pump silent */
  pn = n; pa0 = a0; pa1 = a1; streakN = 0;
  LOGI("scr f=%llu %s(%x%s%x%s)", (unsigned long long)V.frame, V.natives[ni].name, a0,
       argc > 1 ? "," : "", argc > 1 ? a1 : 0, argc > 2 ? ",.." : "");
}
static int scrPass(vm::Vm& V, uint32_t* a, int c, uint32_t* o) { (void)a; (void)c; *o = 0; (void)V; return 0; }
static uint64_t scr_ms2f(uint32_t ms) { return ((uint64_t)ms * 3 + 49) / 50; } /* ms@60fps; 750->45f [BRV-SCT-005b] */
/* Single-slot deadline protocol: latched on first entry (wu==0), re-entry checks,
 * expiry CLEARS to 0 and returns done — re-latching on expiry would yield forever.
 * Scripts issue waits strictly sequentially, so one slot is sound. [P4-M0] */
static int scrWaitM(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  uint64_t& wu = g_scr.waitUntil;
  if (wu == 0) wu = V.frame + scr_ms2f(a[0]) + 1;
  if (V.frame < wu) return 1;
  wu = 0; return 0;
}
static int scrWaitF(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  uint64_t& wu = g_scr.waitUntil;
  if (wu == 0) wu = V.frame + (a[0] ? a[0] : 1);
  if (V.frame < wu) return 1;
  wu = 0; return 0;
}
static int scrFade(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; (void)o; /* mode 1=to black, 2=to clear; time ms@60fps */
  g_scr.fadeTo = a[0] == 1 ? 1.f : 0.f;
  uint64_t fr = scr_ms2f(a[1]);
  float d = g_scr.fadeTo - g_scr.fade;
  g_scr.fadeSpd = fr ? d / (float)fr : d; /* time 0 = snap [golden Fade(1,0)] */
  if (!fr) g_scr.fade = g_scr.fadeTo;
  return 0;
}
static int scrFadeWait(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)a; (void)c; (void)o;
  return g_scr.fadeSpd != 0.f;
}
static int scrMapLoad(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  char b[256]; LOGI("scr MapLoad(\"%s\") -> baked bmap01", sarg(V, a[0], b));
  return 0;
}
static int scrCamera(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  LOGI("scr camera(%d,%d,%d) raw -- iso cam follows unit [M0]", (int)a[0], (int)a[1], c > 2 ? (int)a[2] : 0);
  return 0;
}
static int scrFocus(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)a; (void)c; (void)o;
  uint64_t& wu = g_scr.waitUntil; /* soft focus pan beat */
  if (wu == 0) wu = V.frame + 20;
  if (V.frame < wu) return 1;
  wu = 0; return 0;
}
static int scrTalk(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  if (g_sm.top() == SC_DIALOG || g_fd != FD_NONE) return 1; /* parked: dialog showing/fading */
  if (g_scr.dlgSeq == a[0]) { g_scr.dlgSeq = 0; return 0; } /* our dialog just closed */
  char b[256]; const char* s = sarg(V, a[0], b);
  g_scrDlgN.clear(); g_scrDlgT.clear();
  g_scrDlgN.push_back("???");                 /* speaker mining = P4-M1 [BRV-TXT-*] */
  g_scrDlgT.push_back(s);                     /* real pooled string via VM ram */
  LOGI("scr TalkEvent(\"%s\") -> dialog overlay", s);
  g_scr.dlgSeq = a[0];
  scene_req(2, SC_DIALOG);
  return 1;
}
/* FE9 internal dir = 4-bit compass: bit1=S bit2=W bit4=E bit8=N (numpad map
 * 2/4/6/8 -> 1/2/4/8 [BRV-SCT-005f]); diagonals = OR (5=SE, 10=NW seen in C01) */
static void scr_dir_delta(uint32_t d, int& dx, int& dz) {
  dx = ((d & 4) ? 1 : 0) - ((d & 2) ? 1 : 0);
  dz = ((d & 1) ? 1 : 0) - ((d & 8) ? 1 : 0);
}
/* [P4-M1] unit natives -> world table: handle = 0x1000|idx; RAM-address args
 * are pid strings (sarg). All script moves land on the owning unit now. */
static int scr_unit_h(uint32_t h) { int i = (int)(h & 0xFFFu); return i < g_nu ? i : g_cur; }
static int scrSetPos(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  char b[256]; int gx = (int)a[1], gy = (int)a[2];
  if (gx < 0) gx = 0; if (gx > MAP_TW - 1) gx = MAP_TW - 1;
  if (gy < 0) gy = 0; if (gy > MAP_TH - 1) gy = MAP_TH - 1;
  char pb[64]; const char* pid = sarg(V, a[0], pb);
  int i = wu_find_create(pid && *pid ? pid : "PID_UNKNOWN");
  if (i < 0) { LOGE("scr: unit table full for %s", pid); return 0; }
  WUnit* u = &g_u[i];
  u->pathN = 0; u->gx = gx; u->gy = gy; u->ux_i = tile_cx(gx); u->uz_i = tile_cz(gy);
  u->hide = 0; u->dead = 0; /* SetPos = (re)appearance [FE9 UnitSetPosDirByPID] */
  int dx, dz; scr_dir_delta(a[3], dx, dz);
  if (dx || dz) u->yawT = atan2f((float)dx, (float)dz) * 57.29578f;
  g_cur = i;
  LOGI("scr UnitSetPosDir(\"%s\",%d,%d,dir=%x) -> unit %d", sarg(V, a[0], b), gx, gy, a[3], i);
  return 0;
}
static int scrRotate(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  char b[256]; int dx, dz; scr_dir_delta(a[1], dx, dz);
  int i = scr_unit_h(a[0]);
  if (dx || dz) g_u[i].yawT = atan2f((float)dx, (float)dz) * 57.29578f;
  LOGI("scr UnitRotate(\"%s\",dir=%x,t=%x)", sarg(V, a[0], b), a[1], c > 2 ? a[2] : 0);
  return 0;
}
static int scrMovePath(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  char b[256]; const char* s = sarg(V, a[1], b);
  char pb[64]; const char* pid = sarg(V, a[0], pb);
  int i = wu_find_create(pid && *pid ? pid : "PID_UNKNOWN");
  if (i < 0) { LOGE("scr: unit table full for %s", pid); return 0; }
  WUnit* u = &g_u[i];
  int n = 0;
  for (const char* p = s; *p; p++) { /* numpad path digits -> tile steps */
    int dx = 0, dz = 0;
    switch (*p) {
      case '8': dz = -1; break; case '2': dz = 1; break;
      case '4': dx = -1; break; case '6': dx = 1; break;
      case '7': dx = -1; dz = -1; break; case '9': dx = 1; dz = -1; break;
      case '1': dx = -1; dz = 1; break; case '3': dx = 1; dz = 1; break;
      default: continue;
    }
    step_tile_u(u, dx, dz); n++;
  }
  g_cur = i;
  LOGI("scr UnitMovePath(\"%s\",\"%s\") steps=%d slow=%d", pid, s, n, g_scr.walkSlow);
  return 0;
}
static int scrWalkDir(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; (void)o;
  int dx, dz; scr_dir_delta(a[0], dx, dz);
  if (dx || dz) step_tile_u(&g_u[g_cur], dx, dz);
  return 0;
}
static int scrMoveWait(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)a; (void)c; (void)o;
  return any_moving();
}
static int scrGetPid(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c;
  char b[256]; const char* pid = sarg(V, a[0], b);
  int i = wu_find_create(pid && *pid ? pid : "PID_UNKNOWN");
  *o = i < 0 ? 0 : (0x1000u | (uint32_t)i); /* stable handle [M0] -> table idx [P4-M1]; 0 = invalid */
  return 0;
}
static int scrGetPos(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; *o = (uint32_t)g_u[scr_unit_h(a[0])].gy; return 0;
}
static int scrGetPosX(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; *o = (uint32_t)g_u[scr_unit_h(a[0])].gx; return 0;
}
static int scrHide(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; (void)o;
  g_u[scr_unit_h(a[0])].hide = 1;
  return 0;
}
static int scrShow(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; (void)o;
  g_u[scr_unit_h(a[0])].hide = 0;
  return 0;
}
static int scrSfx(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  char b[256]; LOGI("scr SFXPlay(\"%s\") -> SE stand-in", sarg(V, a[0], b));
  return 0;
}
static int scrBgm(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  char b[256]; const char* s = sarg(V, c > 1 ? a[1] : a[0], b);
  LOGI("scr BGMPlay(mode=%u,\"%s\") -> boss/stage mix [M0]", a[0], s);
  (void)V; (void)o;
  return 0;
}
/* BGMFadeOut(slot,dur) [BRV-SCT-005k FUN_800b7db4]: dur ms, slot 0 = both BGM */
static int scrBgmFadeOut(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)c; (void)o;
  int slot = (int)(a[0] & 0xff);
  float dur = (float)(int16_t)(a[1] & 0xffff);
  LOGI("scr BGMFadeOut(slot=%d dur=%.0fms)", slot, dur);
  if (slot == 0) { g_mixer.bgmFadeOut(0, dur); g_mixer.bgmFadeOut(1, dur); }
  else g_mixer.bgmFadeOut(slot & 1, dur);
  return 0;
}
static int scrMovie(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)c; (void)o;
  uint64_t& wu = g_scr.waitUntil;
  if (wu == 0) {
    char b[256]; LOGI("scr f=%llu MoviePlay(\"%s\") -> 60f beat", (unsigned long long)V.frame, sarg(V, a[0], b));
    wu = V.frame + 60;
  }
  if (V.frame < wu) return 1;
  wu = 0; return 0; /* movie "played" (no THP decode in PoC): beat expires, chain resumes */
}
static int scrIsE3(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  (void)V; (void)a; (void)c; *o = 0; return 0; /* retail build [BRV-SCT-005e] */
}
/* set/get/clr/regist(name) [BRV-SCT-005l/m FUN_800bb018/800bb04c/800bafe4/800bb080]:
 * FE9 = NAMED EVENT-FLAG BITSET — 96-slot table @803307bc (u32 ptr + hash +0x180,
 * bitset +0x300); names registered at RUNTIME by script `regist(name)` (38 sites,
 * e.g. Startup x18), clr clears the bit, miss-lookup = 0/no-op. C-flags gf_*
 * (gameover/complete/canceled/reserved2-4) share the same table.
 * Engine: sparse map — regist=declare(0), set=1, clr=0, get=bit. */
static std::unordered_map<std::string, uint8_t> g_scrFlags;
static int scrSetFlag(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  char b[256]; const char* s = sarg(V, c > 0 ? a[0] : 0, b);
  g_scrFlags[s] = 1;
  LOGI("scr set(\"%s\") flags=%d", s, (int)g_scrFlags.size());
  (void)o;
  return 0;
}
static int scrGetFlag(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  char b[256]; const char* s = sarg(V, c > 0 ? a[0] : 0, b);
  auto it = g_scrFlags.find(s);
  *o = it == g_scrFlags.end() ? 0 : it->second;
  (void)V;
  return 0;
}
static int scrClrFlag(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  char b[256]; const char* s = sarg(V, c > 0 ? a[0] : 0, b);
  g_scrFlags[s] = 0;
  (void)o;
  return 0;
}
static int scrRegistFlag(vm::Vm& V, uint32_t* a, int c, uint32_t* o) {
  char b[256]; const char* s = sarg(V, c > 0 ? a[0] : 0, b);
  g_scrFlags.emplace(s, 0); /* declare, keep existing value [SCT-005m] */
  (void)o;
  return 0;
}
/* [BRV-SCT-005n] batch-7 quick-wins: BM vars, mind, event-cursor, skip-word,
 * diff predicates, achieve, dialog-choice, tile-action natives.
 * BM u16[16] @BMAP+0x00; diff-byte @BMAP+0xA6 (encoding Partial: 2=Hard-ish
 * /2, 3=Normal-ish x1.5; default 3 = only Normal() true). */
static uint16_t g_bmVars[16];
static uint8_t g_mindMe = 0, g_mindTgt = 0, g_evState = 0, g_diffByte = 3;
static int32_t g_evX = 0, g_evY = 0;
static uint32_t g_skipWord = 0;
static std::unordered_map<std::string, int32_t> g_achieve;
static int8_t g_dlgRes = 0;
static int scrBmGet(vm::Vm&, uint32_t* a, int c, uint32_t* o) {
  *o = c > 0 ? g_bmVars[a[0] & 15] : 0;
  return 0;
}
static int scrBmLog(vm::Vm&, uint32_t* a, int c, uint32_t* o) {
  if (c > 0) { g_bmVars[a[0] & 15] = (uint16_t)(c > 1 ? a[1] : 0);
    LOGI("scr BM%d=%u", a[0] & 15, g_bmVars[a[0] & 15]); }
  (void)o;
  return 0;
}
static int scrZero(vm::Vm&, uint32_t*, int, uint32_t* o) { *o = 0; return 0; }
static int scrGcst(vm::Vm&, uint32_t*, int, uint32_t* o) { *o = g_skipWord; return 0; }
static int scrDisSkip(vm::Vm&, uint32_t*, int, uint32_t*) { g_skipWord |= 4; return 0; }
static int scrEnSkip(vm::Vm&, uint32_t*, int, uint32_t*) { g_skipWord &= ~6u; return 0; }
static int scrNormal(vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (g_diffByte == 0 || g_diffByte == 3); return 0; }
static int scrHard(vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (g_diffByte == 0); return 0; }
static int scrManiac(vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (g_diffByte <= 1); return 0; }
static int scrAchieve(vm::Vm& V, uint32_t* a, int c, uint32_t*) {
  char b[256];
  const char* s = sarg(V, c > 0 ? a[0] : 0, b);
  int32_t v = c > 1 ? (int32_t)a[1] : 0;
  if (g_diffByte == 2) v /= 2;
  else if (g_diffByte == 3) v += v / 2;
  g_achieve[s] = v;
  LOGI("scr achieve(\"%s\")=%d", s, v);
  return 0;
}
static int scrDlg2Items(vm::Vm&, uint32_t*, int, uint32_t* o) {
  *o = 1; /* non-blocking placeholder until dialog-choice UI [P4] */
  return 0;
}
/* [BRV-SCT-005o] batch-8: camera save/restore + tile-action stubs.
 * camera follows unit mirror (g_ux/g_uz) so PastMapCamera restores the
 * follower directly; building/rect natives log until the instance+ring
 * system lands (P2-MAP backlog: action enum 0x23/0x24/0x25/0x29). */
static float g_camSnap[3];
static uint8_t g_camSnapSet = 0;
static int scrCamCopy(vm::Vm&, uint32_t*, int, uint32_t*) {
  g_camSnap[0] = g_ux; g_camSnap[1] = g_uz; g_camSnapSet = 1;
  return 0;
}
static int scrCamPast(vm::Vm&, uint32_t*, int, uint32_t*) {
  if (g_camSnapSet) { g_camFx = g_ux = g_camSnap[0]; g_camFz = g_uz = g_camSnap[1]; }
  return 0;
}
static int scrTileLog(vm::Vm&, uint32_t* a, int c, uint32_t* o) {
  if (c >= 2) LOGI("scr tileact(%d,%d)", (int)(int8_t)a[0], (int)(int8_t)a[1]);
  if (o) *o = 0; /* no instances on scr maps yet -> FE9 returns found=0 */
  return 0;
}
/* [BRV-SCT-005p] UnitsPop = capture snapshot; UnitsPush_Uninitialize =
 * restore+drop (FE9 stack-mgr DAT_803206EC). Depth 6 mirrors FE9 usage. */
static int scrUnitsPop(vm::Vm&, uint32_t*, int, uint32_t*) {
 if (g_snapN >= 6) { LOGI("scr UnitsPop: stack FULL"); return 0; }
 memcpy(g_snap[g_snapN], g_u, sizeof(WUnit) * MAXU);
 g_snapNu[g_snapN] = g_nu; g_snapN++;
 LOGI("scr UnitsPop: snapshot #%d nu=%d", g_snapN, g_nu);
 return 1;
}
static int scrUnitsRestore(vm::Vm&, uint32_t*, int, uint32_t*) {
 if (g_snapN <= 0) { LOGI("scr UnitsPush_Uninit: EMPTY stack"); return 0; }
 g_snapN--; g_nu = g_snapNu[g_snapN];
 memcpy(g_u, g_snap[g_snapN], sizeof(WUnit) * MAXU);
 LOGI("scr UnitsPush_Uninitialize: restore nu=%d", g_nu);
 return 1;
}
static int scrDieTalk(vm::Vm&, uint32_t*, int, uint32_t*) {
 LOGI("scr DefaultDieTalk lastDied=%d%s", g_lastDied,
      g_lastDied >= 0 && g_lastDied < MAXU ? g_u[g_lastDied].pid : "");
 return 1;
}
struct ScrOv { const char* n; int (*fn)(vm::Vm&, uint32_t*, int, uint32_t*); };
static const ScrOv SCR_OVS[] = {
  {"WaitM", scrWaitM}, {"Wait", scrWaitM}, {"WaitM2", scrWaitM}, {"WaitSecs", scrWaitM},
  {"WaitF", scrWaitF}, {"WaitFrames", scrWaitF},
  {"Fade", scrFade}, {"FadeIn", scrFade}, {"FadeOut", scrFade}, {"ScreenFade", scrFade},
  {"InstantFade", scrFade}, {"FadeWait", scrFadeWait}, {"isFading", scrFadeWait}, {"IsFading", scrFadeWait},
  {"MapLoad", scrMapLoad}, {"MapUnload", scrPass},
  {"SetMapCamera", scrCamera}, {"InstantSetMapCamera", scrCamera},
  {"Focus", scrFocus}, {"InstantFocus", scrPass}, {"FocusHardWait", scrFocus}, {"UnitFocusByPID", scrPass}, {"UnitFocusOff", scrPass},
  {"TalkEvent", scrTalk}, {"TalkResume", scrPass}, {"TalkExist", scrPass},
  {"UnitSetPosDirByPID", scrSetPos}, {"UnitRotateByPID", scrRotate}, {"UnitRotateOffPID", scrPass},
  {"UnitMoveDirByPID", scrMovePath}, {"UnitWalkDir", scrWalkDir}, {"UnitWalkSlow",
    [](vm::Vm& V, uint32_t* a, int c, uint32_t* o) { (void)V; (void)c; (void)o; g_scr.walkSlow = (int)a[0]; return 0; } },
  {"UnitMoveWait", scrMoveWait}, {"UnitWaitMove", scrMoveWait}, {"UnitMoveDirWait", scrMoveWait},
  {"UnitGetByPID", scrGetPid}, {"UnitGetXByPID", scrGetPosX}, {"UnitGetYByPID", scrGetPos},
  {"UnitPosXByPID", scrGetPosX}, {"UnitPosYByPID", scrGetPos},
  {"UnitAnimByPID", scrPass}, {"UnitSetAnimByPID", scrPass}, {"UnitFadeIn", scrPass}, {"UnitFadeOff", scrPass},
  {"UnitSetEquip", scrPass}, {"UnitSetWeaponAway", scrPass}, {"UnitHideByPID", scrHide}, {"UnitShowByPID", scrShow},
  {"SallySetByGroup", scrPass}, {"InstantDisposFirst", scrPass}, {"InstantDispos", scrPass},
  {"DisposFirst", scrPass}, {"Dispos", scrPass}, {"DisposSetMode", scrPass}, {"InstantSetFirstMode", scrPass},
  {"SetGrid", scrPass}, {"SetCircle", scrPass}, {"SetOutLink", scrPass},
  {"SetWhiteBackdrop", scrPass}, {"RemoveBackdrop", scrPass},
  {"BGMPlay", scrBgm}, {"BGMEffectPlay", scrBgm}, {"BGMFadeOut", scrBgmFadeOut}, {"BGMSetVol", scrPass},
  {"SFXPlay", scrSfx}, {"SEPlay", scrSfx}, {"SEStop", scrPass},
  {"MoviePlay", scrMovie}, {"ChapterMoviePlay", scrMovie},
  {"IsE3Version", scrIsE3}, {"ENVSilentOn", scrPass}, {"ENVNoisyOn", scrPass}, {"ENVNoisyOff", scrPass}, {"ENVSilentOff", scrPass},
  {"set", scrSetFlag}, {"get", scrGetFlag}, {"clr", scrClrFlag}, {"regist", scrRegistFlag},
  /* [BRV-SCT-005n] batch-7 */
  {"BMSetValue", scrBmLog}, {"BMGetValue", scrBmGet},
  {"MindGetMe", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = g_mindMe; return 0; } },
  {"MindGetTarget", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = g_mindTgt; return 0; } },
  {"EventGetX", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (uint32_t)g_evX; return 0; } },
  {"EventGetY", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (uint32_t)g_evY; return 0; } },
  {"Check", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = g_evState; return 0; } },
  {"GCST", scrGcst}, {"EnableSkip", scrEnSkip}, {"DisableSkip", scrDisSkip},
  {"GetBattleType", scrZero},
  {"Normal", scrNormal}, {"Hard", scrHard}, {"Maniac", scrManiac},
  {"AchieveSet", scrAchieve},
  {"DialogResult", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (uint8_t)g_dlgRes; return 0; } },
  {"DialogGetRes", [](vm::Vm&, uint32_t*, int, uint32_t* o) { *o = (uint8_t)g_dlgRes; return 0; } },
  {"Dialog2Items", scrDlg2Items},
  {"FocusWait", scrZero}, {"TBoxOpen", scrZero}, {"VisitOut", scrZero},
  {"Join", scrZero}, {"ForceGetFirst", scrZero},
  {"BGMQuietOff", scrPass}, {"BGMCont", scrPass}, {"BGMMute", scrPass},
  {"BGMPlayVol", scrBgm},
  /* [BRV-SCT-005o] batch-8 */
  {"CopyMapCamera", scrCamCopy}, {"PastMapCamera", scrCamPast},
  {"SetEventCameraPrec", scrPass},
  {"BuildInstHide", scrTileLog}, {"BuildInstShow", scrTileLog},
  {"BuildDestruction", scrTileLog}, {"SetShooter", scrTileLog},
  {"RectKill", scrPass}, {"RectSetCurve", scrPass}, {"DefaultDieTalk", scrDieTalk},
 /* [BRV-SCT-005p] */
 {"UnitsPop", scrUnitsPop}, {"UnitsPush_Uninitialize", scrUnitsRestore},
};
static void script_init(void) {
  const uint8_t* d; int n;
  if (!g_res.read("scripts/c01.cmb", &d, &n)) { LOGI("script: scripts/c01.cmb absent -> direct WORLD on start"); return; }
  std::vector<uint8_t> bytes(d, d + n);
  if (!g_scr.cmb.load(std::move(bytes), "C01")) { LOGI("script: C01 load FAIL"); return; }
  if (!g_res.read("scripts/natreg.csv", &d, &n)) { LOGI("script: scripts/natreg.csv absent"); return; }
  std::string csv((const char*)d, (size_t)n);
  size_t p = csv.find('\n'); p = p == std::string::npos ? csv.size() : p + 1; /* skip header */
  int cnt = 0;
  while (p < csv.size()) {
    size_t e = csv.find('\n', p); if (e == std::string::npos) e = csv.size();
    size_t cm = csv.find(',', p);
    if (cm != std::string::npos && cm < e) {
      g_scr.names.push_back(csv.substr(p, cm - p));
      g_scr.vm.regNative({g_scr.names.back().c_str(), scrPass, atoi(csv.c_str() + cm + 1)});
      cnt++;
    }
    p = e + 1;
  }
  g_scr.vm.log = {scr_vblog, nullptr};
  g_scr.vm.onNative = scr_natlog;
  g_scr.vm.addCmb(&g_scr.cmb);
  int ov = 0;
  for (const auto& b : SCR_OVS) {
    auto it = g_scr.vm.nativeIx.find(b.n);
    if (it != g_scr.vm.nativeIx.end()) { g_scr.vm.natives[it->second].fn = b.fn; ov++; }
  }
  g_scr.loaded = true;
  LOGI("script: C01 recs=%d natives=%d(bound %d) entry=Opening0", (int)g_scr.cmb.recs.size(), cnt, ov);
}
static void script_begin(void) {
  if (!g_scr.loaded) { scene_req(1, SC_WORLD); return; }
  if (g_scr.vt) return; /* resume from dialog child: coroutine still live, do NOT restart [P4-M0] */
  g_scr.fade = 1.f; g_scr.fadeTo = 1.f; g_scr.fadeSpd = 0.f; g_scr.dlgSeq = 0; /* open on black */
  g_scrDlgN.clear(); g_scrDlgT.clear(); /* replay: fresh page list [P4-M1] */
  g_scr.waitUntil = 0; g_scr.ringDumped = false; /* fresh deadline slot + re-arm wedge probe */
  delete g_scr.vt;
  g_scr.vt = vm::start(g_scr.vm, "Opening0", nullptr, 0);
  g_scr.lastLog = g_playFrames; /* grace: wedge clock starts AT begin, not at f=0 */
  LOGI("script: begin Opening0 %s", g_scr.vt ? "ok" : "NO ENTRY -> WORLD");
  if (!g_scr.vt) scene_req(1, SC_WORLD);
}
static void script_tick(void) {
  g_scr.vm.frame = g_playFrames;
  g_scr.vm.trace = true; /* op ring always on: wedge auto-dump is the debugger [P4-M0] */
  if (g_scr.vt && !g_scr.ringDumped && g_playFrames - g_scr.lastLog > 240) {
    LOGI("scr WEDGE f=%llu lastLog=%llu -> ring dump", (unsigned long long)g_playFrames,
         (unsigned long long)g_scr.lastLog);
    g_scr.vm.dumpRing(512);
    g_scr.ringDumped = true;
  }
  if (g_scr.fadeSpd != 0.f) {
    g_scr.fade += g_scr.fadeSpd;
    if ((g_scr.fadeSpd > 0.f && g_scr.fade >= g_scr.fadeTo) ||
        (g_scr.fadeSpd < 0.f && g_scr.fade <= g_scr.fadeTo)) { g_scr.fade = g_scr.fadeTo; g_scr.fadeSpd = 0.f; }
  }
  if (!g_scr.vt) return;
  vm::StepRes r = vm::run(g_scr.vm, *g_scr.vt, 300000);
  if (r == vm::STEP_DONE || r == vm::STEP_ERROR) {
    LOGI("script: Opening0 %s @f=%llu -> WORLD", r == vm::STEP_DONE ? "DONE" : "ERROR",
         (unsigned long long)g_playFrames);
    delete g_scr.vt; g_scr.vt = nullptr;
    g_scr.fade = g_scr.fadeTo = g_scr.fadeSpd = 0.f;
    if (g_sm.top() == SC_SCRIPT) scene_req(1, SC_WORLD);
    else g_scr.loaded = g_scr.loaded; /* dialog still up on WORLD: thread reaped, fine */
  }
}

/* ---------- save plumbing [P3-SAV-001] ---------- */
static SaveV1 snapSave(int slot) {
  SaveV1 s{};
  s.slot = (uint32_t)slot; s.chapter = 1; s.gx = g_gx; s.gy = g_gy;
  s.playFrames = (uint32_t)(g_playFrames & 0xFFFFFFFFu); s.rngSeed = g_rng;
  s.camYaw = g_camYaw.load(); s.gaSel = (uint8_t)g_gaSel; s.mixB = (uint8_t)g_mixB.load();
  return s;
}
static void doSave(int slot) {
  SaveV1 s = snapSave(slot);
  if (g_saves.save(slot, s))
    LOGI("save: slot%d ok tile=%d,%d frames=%u yaw=%.0f seed=%08x", slot, s.gx, s.gy, s.playFrames, s.camYaw, s.rngSeed);
  else
    LOGE("save: slot%d WRITE FAILED", slot);
}
static bool doLoad(int slot) {
  SaveV1 s;
  if (!g_saves.load(slot, &s)) { LOGI("save: slot%d absent/corrupt — refused", slot); return false; }
  g_gx = s.gx; g_gy = s.gy; g_ux = tile_cx(g_gx); g_uz = tile_cz(s.gy);
  g_camYaw.store(s.camYaw);
  /* [P4-M1] world resumes from saved tile: fresh table, IKE re-placed there */
  g_nu = 0; g_worldReady = 0; world_init(0);
  g_u[g_cur].gx = s.gx; g_u[g_cur].gy = s.gy;
  g_u[g_cur].ux_i = tile_cx(s.gx); g_u[g_cur].uz_i = tile_cz(s.gy);
  g_w.cx = s.gx; g_w.cy = s.gy;
  g_gaSel = s.gaSel & 3; if (g_gas[g_gaSel].endF > 0) { memcpy(&g_ga, &g_gas[g_gaSel], sizeof(g_ga)); g_frame = g_ga.startF; }
  g_mixB.store(s.mixB ? 1 : 0); g_mixer.bgmMix(g_mixB.load());
  g_playFrames = s.playFrames;
  g_rng = rngSeedFromState(s); /* [P3-SAV-001] seed DERIVED from state, never clock */
  LOGI("save: slot%d loaded tile=%d,%d frames=%u seed=%08x (rng reseeded from state)", slot, s.gx, s.gy, s.playFrames, g_rng);
  return true;
}
static void doNew(int slot) {
  bool had = g_saves.removeSlot(slot);
  g_gx = 14; g_gy = 14; g_ux = tile_cx(14); g_uz = tile_cz(14);
  g_nu = 0; g_worldReady = 0; /* next WORLD entry spins a fresh table [P4-M1] */
  g_camYaw.store(45.f); g_playFrames = 0; g_rng = 0xC0FFEEu;
  LOGI("save: NEW GAME slot%d (%s)", slot, had ? "old save deleted" : "no prior save");
}

static int g_texPickSel = -1; /* engine-thread only */
static void on_input(Act action) {
  if (g_paused.load() && (int)action <= ACT_DOWN) return; /* paused: grid nav frozen */
  if (action == ACT_DEBUG_TEX) { /* T = cycle every decodable FE9 image (tpl+cms) into the unit slot */
    if (!g_texId) return;
    tpl_file* srcs[2] = {&g_texA, &g_tex};
    int total = 0, siPick = 0, iPick = 0;
    for (int si = 0; si < 2; si++)
      for (int i = 0; i < srcs[si]->count; i++) if (srcs[si]->imgs[i].ok) total++;
    if (total <= 0) return;
    g_texPickSel = (g_texPickSel + 1) % total;
    int k = g_texPickSel;
    for (int si = 0; si < 2; si++)
      for (int i = 0; i < srcs[si]->count && k >= 0; i++)
        if (srcs[si]->imgs[i].ok && --k < 0) { siPick = si; iPick = i; k = -2; break; }
    tex_img* s = &srcs[siPick]->imgs[iPick];
    glBindTexture(GL_TEXTURE_2D, g_texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, s->w, s->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->rgba);
    LOGI("tex pick: src=%d #%d %dx%d ok=%d (pick %d/%d)", siPick, iPick, s->w, s->h, s->ok, g_texPickSel + 1, total);
    return;
  }
  (void)action; /* [P4-M1] gameplay keys live in world_input; on_input is the
                 * not-ready/world debug fallback: only T (tex cycle) above. */
}

/* ---------- gamepad consumer (runs on engine thread every frame) ----------
 * Mapping (PoC harness semantics; P3 menu/cursor layer will consume InputState):
 *   LEFT_STICK X   = camera orbit (yaw, continuous)  [paralel dgn tap: tap tetap path-move]
 *   RIGHT_STICK X  = look yaw fine | Y = pitch look (8..75 deg clamp)
 *   DPAD / HAT     = grid cursor nav (edge + auto-repeat 12 frames ~0.6s @20fps)
 *   BUTTON_A       = confirm  (harness tap-equivalent = overlay A action)
 *   BUTTON_B       = back     (harness = overlay B action)
 *   START          = pause/menu overlay toggle (P3 stub: freezes anim+path+nav)
 *   L3 / R3        = camera fit (yaw/pitch/focus reset)
 * No pad ever connected => this function returns immediately: touch path 100%
 * untouched (fallback guarantee). */
static void pad_update(const PadState& p) {
  if (!p.connected) return;
  unsigned e = g_padEdge.exchange(0);
  if (e & (1u << PADB_START)) {
    int on = !g_paused.load();
    g_paused.store(on);
    LOGI("pad: START -> %s", on ? "PAUSE (P3 menu stub)" : "resume");
  }
  if (e & ((1u << PADB_L3) | (1u << PADB_R3))) {
    g_camYaw.store(45.f); g_camPitch.store(24.f);
    g_camFx = g_ux; g_camFz = g_uz;   /* instant fit */
    LOGI("pad: camera fit");
  }
  if (e & (1u << PADB_A)) g_acts.push(ACT_CONFIRM);  /* confirm == overlay A semantics */
  if (e & (1u << PADB_B)) g_acts.push(ACT_CANCEL);   /* back    == overlay B semantics */
  if (e & (1u << PADB_X)) g_acts.push(ACT_DIALOG);   /* scene-stack test pushes [P3-SCN-001] */
  if (e & (1u << PADB_Y)) g_acts.push(ACT_BATTLE);
  if (e & (1u << PADB_GUIDE)) g_acts.push(ACT_BACK); /* KEYCODE_BACK lands here */
  /* dpad + hat -> grid nav with auto-repeat */
  int dcode = 0;
  if (p.btn[PADB_DPLEFT]) dcode = 1; else if (p.btn[PADB_DPRIGHT]) dcode = 2;
  else if (p.btn[PADB_DPUP]) dcode = 3; else if (p.btn[PADB_DPDOWN]) dcode = 4;
  if (!dcode) { /* hat: -1 up/left side */
    if (p.ax[PADAX_HATX] < -0.5f) dcode = 1; else if (p.ax[PADAX_HATX] > 0.5f) dcode = 2;
    else if (p.ax[PADAX_HATY] < -0.5f) dcode = 3; else if (p.ax[PADAX_HATY] > 0.5f) dcode = 4;
  }
  static int repCnt = 0, repLast = 0;
  if (dcode) {
    if (dcode != repLast || ++repCnt >= 12) { g_acts.push((Act)dcode); repLast = dcode; repCnt = 0; }
  } else { repCnt = 0; repLast = 0; }
  /* sticks: deadzone 0.25 */
  auto dz = [](float v) { return (v > -0.25f && v < 0.25f) ? 0.f : v; };
  float lxp = dz(p.ax[PADAX_LX]), rxp = dz(p.ax[PADAX_RX]), ryp = dz(p.ax[PADAX_RY]);
  if (lxp) g_camYaw.store(g_camYaw.load() + lxp * 2.2f);            /* orbit */
  if (rxp) g_camYaw.store(g_camYaw.load() + rxp * 1.1f);            /* look fine */
  if (ryp) {
    float np = g_camPitch.load() - ryp * 1.1f;
    if (np < 8.f) np = 8.f; if (np > 75.f) np = 75.f;
    g_camPitch.store(np);
  }
}

/* one snapshot per frame: engine layers consume ONE object from here on */
static InputState input_poll() {
  InputState in{};
  in.touch.tapNX = g_tapNX.load();
  in.touch.tapNY = g_tapNY.load();
  in.touch.tapReq = g_tapReq.exchange(0) != 0;
  in.touch.taps100ms = g_tapsIn100ms.exchange(0);
  in.pad.connected = g_padConnected.load() != 0;
  for (int i = 0; i < PADB_COUNT; i++) in.pad.btn[i] = g_padBtn[i].load() != 0;
  for (int i = 0; i < PADAX_COUNT; i++) in.pad.ax[i] = g_padAxis[i].load();
  return in;
}

// ---------- JNI bridge ----------
static ANativeWindow* g_window = nullptr;
static std::mutex g_windowMx;
static std::condition_variable g_windowCv;

/* core touch logic: normalized x,y + action 0 down / 1 move / 2 up|cancel */
/* P2 gate instrumentation (BRV-P2-001): tap->present latency + thermal meter */
static std::atomic<long long> g_tapDownT{0}; /* CLOCK_MONOTONIC ns of last ACTION_DOWN */
static inline long long mono_ns() {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static void touch_event(float nx, float ny, int action, int slot) {
  if (action == 0) LOGI("touch: dn n=%.3f,%.3f slot=%d", nx, ny, slot);
  if (action <= 1) { /* down / move: hit-test & track */
    int code = 0;
    float best = BR * BR * 2.25f; /* 1.5x visual, nearest-wins on overlap */
    for (int i = 0; i < 7; i++) {
      float dx = nx - BTN[i].cx, dy = ny - BTN[i].cy;
      float d2 = dx * dx + dy * dy;
      if (d2 <= best) { best = d2; code = BTN[i].code; }
    }
    if (code) {
      if (g_slotCode[slot] != code) {
        int prev = g_slotCode[slot].exchange(0);
        if (prev) g_held[prev].store(0);
        g_slotCode[slot].store(code);
        g_held[code].store(1);
        if (action == 0) { /* fresh press edge */
          g_tapDownT.store(mono_ns());
          g_tapsIn100ms.fetch_add(1);
          g_acts.push((Act)code);
        }
      }
    } else if (action == 0) {
      int prev = g_slotCode[slot].exchange(0);
      if (prev) g_held[prev].store(0);
      if (slot == 0) { g_tapDownT.store(mono_ns()); g_tapNX.store(nx); g_tapNY.store(ny); g_tapReq.store(1); } /* ground tap */
    }
  } else { /* up / cancel */
    int prev = g_slotCode[slot].exchange(0);
    if (prev) g_held[prev].store(0);
  }
}

extern "C" {
JNIEXPORT void JNICALL Java_com_fe9_poc_MainActivity_nativeTouch(JNIEnv*, jobject,
    jfloat nx, jfloat ny, jint action, jint pointerId) {
  touch_event(nx, ny, (int)action, (int)pointerId & 15);
}
JNIEXPORT void JNICALL Java_com_fe9_poc_MainActivity_nativePadButton(JNIEnv*, jobject,
    jint button, jboolean pressed) {
  int i = (int)button;
  if (i < 0 || i >= PADB_COUNT) return;
  int v = pressed ? 1 : 0;
  int prev = g_padBtn[i].exchange(v);
  if (v && !prev) g_padEdge.fetch_or(1u << (unsigned)i); /* press edge for edge-consumers */
  if (v) { if (!g_padConnected.exchange(1)) LOGI("pad: connected (source=gamepad)"); }
}
JNIEXPORT void JNICALL Java_com_fe9_poc_MainActivity_nativePadAxis(JNIEnv*, jobject,
    jint axis, jfloat value) {
  int i = (int)axis;
  if (i < 0 || i >= PADAX_COUNT) return;
  g_padAxis[i].store((float)value);
  /* triggers sit >0 for half their range; only |stick|>0.6 latches connection */
  if (i <= PADAX_RY && value > 0.6f && !g_padConnected.exchange(1)) LOGI("pad: connected (axis)");
}
extern "C" JNIEXPORT void JNICALL
Java_com_fe9_poc_MainActivity_nativeAct(JNIEnv*, jobject, jint act) {
  if (act >= 0 && act < ACT_COUNT) g_acts.push((Act)act); /* direct engine-thread producer */
}
extern "C" JNIEXPORT void JNICALL
Java_com_fe9_poc_MainActivity_nativeSetFilesDir(JNIEnv* env, jobject, jobject path) {
  if (!path) return;
  jclass cls = env->GetObjectClass(path);
  jmethodID mid = env->GetMethodID(cls, "toString", "()Ljava/lang/String;");
  auto s = (jstring)env->CallObjectMethod(path, mid);
  const char* c = env->GetStringUTFChars(s, nullptr);
  g_saves.init(c);
  LOGI("save: dir=%s/saves", c);
  env->ReleaseStringUTFChars(s, c);
}
JNIEXPORT void JNICALL Java_com_fe9_poc_MainActivity_nativeSetSurface(JNIEnv* env, jobject, jobject surface) {
  std::lock_guard<std::mutex> lk(g_windowMx);
  if (surface) g_window = ANativeWindow_fromSurface(env, surface);
  else if (g_window) { ANativeWindow_release(g_window); g_window = nullptr; }
  g_windowCv.notify_all();
}
}

// ---------- engine ----------
static void engine_main(AAssetManager* am) {
  load_asset(am);
  script_init(); /* [P4-M0] C01 Opening0 on the P3-M3 VM */
  ANativeWindow* win;
  {
    std::unique_lock<std::mutex> lk(g_windowMx);
    g_windowCv.wait(lk, [] { return g_window != nullptr; });
    win = g_window;
  }
  int w = 0, h = 0;
  if (!egl_create(win, &w, &h)) { LOGI("FATAL egl"); return; }
  g_prog = makeProgram(VS, FS); g_prog2 = makeProgram(VS2, FS2); g_prog3 = makeProgram(VS3, FS3);
  upload_texture();
  build_grid();
  upload_mesh();
  /* M2.2 [BRV-MAP-001] */
  ui_upload_tpl(&g_maptex, &g_mapTex, 0);
  if (g_mapgs.nsub) upload_map();
  ui_init();
  audio_start();
  LOGI("scene: root=%s (fade-in)", scene_name(g_sm.top())); /* cold boot lands on attract */
  long long last = 0; int frames = 0; long long fpsT0 = 0;
  long long g_latMax = 0; int g_latN = 0; /* 2s-window tap->render stats */
  while (true) {
    InputState in = input_poll();          /* ONE object for all layers */
    pad_update(in.pad);                    /* inert until a pad is seen */
    Act a;                                 /* [P3-INP-001] all sources drained here */
    SceneId top = g_sm.top();              /* [P3-SCN-001] top scene owns the frame */
    while (g_acts.pop(&a)) {
      /* SE hook [P3-AUD-001]: payload = head of real decoded STM as documented
       * stand-in until the .samp bank decodes (P2 RAM trace) feeds playSe(). */
      if (g_sB.pcm && g_sB.pcmFrames > 0) {
        static bool seLogged = false;
        int n = g_sB.pcmFrames < 48000 ? g_sB.pcmFrames : 48000;
        g_mixer.playSe(g_sB.pcm, n);
        if (!seLogged) { seLogged = true; LOGI("se: hook live, payload=real-stm stand-in n=%d (.samp P2 pending)", n); }
      }
      if (g_fd != FD_NONE) continue;       /* fade covers a scene cut: swallow input */
      if (top == SC_ATTRACT) {             /* explicit start input leaves attract */
        if (a == ACT_LOAD && doLoad(0)) { scene_req(1, SC_WORLD); continue; } /* CONTINUE */
        if (a == ACT_LEFT || a == ACT_RIGHT || a == ACT_UP || a == ACT_DOWN ||
            a == ACT_CONFIRM || a == ACT_CANCEL) scene_req(1, g_scr.loaded ? SC_SCRIPT : SC_WORLD);
      } else if (top == SC_WORLD) {
        if (a == ACT_SAVE) { doSave(0); continue; }
        if (a == ACT_NEW) { doNew(0); continue; }
        if (a == ACT_LOAD) { doLoad(0); continue; }
        if (a == ACT_DIALOG) scene_req(2, SC_DIALOG);
        else if (a == ACT_BATTLE) scene_req(2, SC_BATTLE);
        else if (a == ACT_DEBUG_TEX) on_input(a);   /* T stays live everywhere */
        else if (g_worldReady) world_input(a); /* [P4-M1] one input path: dpad/A/B all live */
        else on_input(a);                      /* world not yet spun up: debug fallback */
      } else {                             /* dialog/battle/script overlay layer */
        if (top == SC_DIALOG && a == ACT_CONFIRM) dialog_advance();
        else if (top == SC_SCRIPT) { /* the opening owns the world: input ignored [P4-M0] */ }
        else if (a == ACT_CONFIRM || a == ACT_CANCEL || a == ACT_BACK) scene_req(3, SC_ATTRACT);
      }
    }
    scene_tick();
    if (g_sm.top() == SC_WORLD && !g_worldReady) world_init(0); /* [P4-M1] spin up on entry */
    g_playFrames++;
    if (g_scr.vt) script_tick();           /* VM pump: coroutine advances one slice/frame [P4-M0] */
    if (g_sm.top() == SC_WORLD && in.touch.tapReq) {
      if (g_worldReady && !g_paused.load()) world_tap(in.touch.tapNX, in.touch.tapNY);
      else { int tx = -1, ty = -1; tap_to_tile(in.touch.tapNX, in.touch.tapNY, &tx, &ty);
             LOGI("tap->tile %d,%d (world off)", tx, ty); }
    }
    /* [P4-M1] per-frame stepper for the WHOLE table (script moves included) +
     * world machine + camera/save mirror. Drains under DIALOG/SCRIPT exactly
     * like FE9 animating units during talk (UnitMoveWait must not hang). */
    if (!g_paused.load() || any_moving()) world_step();
    world_tick();
    if (g_nu > 0) world_mirror();
    /* follow camera: FE9-style FIXED isometric yaw (no auto-rotate: keeps dpad
       directions world-stable); only focus position trails the unit */
    g_camFx += (g_ux - g_camFx) * 0.06f;
    g_camFz += (g_uz - g_camFz) * 0.06f;
    { /* [M2.2] anim state machine: move clip while the CAMERA unit walks,
       * wait clip when idle (yaw easing moved per-unit into world_step). */
      bool moving = g_cur < g_nu && g_u[g_cur].pathN > 0 && !g_paused.load();
      static int mvPrev = -1;
      int want = moving ? 0 : 2; /* 0 move_n, 2 wait_n in GA_ENTRY order */
      if (mvPrev != (moving ? 1 : 0) && g_gas[want].endF > 0) {
        mvPrev = moving ? 1 : 0;
        g_gaSel = want; memcpy(&g_ga, &g_gas[want], sizeof(g_ga)); g_frame = g_ga.startF;
      }
    }
    // advance ga: FULL RATE 1:1 vs render (move_n = 36f loop = 1.2s stride cycle;
    //   3x-slow ticker earlier desynced feet from travel = "sliding chess piece")
    bool frozen = g_paused.load() || (g_sm.top() != SC_WORLD && g_sm.top() != SC_ATTRACT);
    if (!frozen) g_frameMs++;
    if (g_frameMs >= 1) {
      g_frameMs = 0;
      g_frame++;
      if (g_ga.endF > 0) {
        if (g_frame > g_ga.endF) {
          if (g_ga.loop) { g_frame = g_ga.startF; for (int i = 0; i < g_ga.nEvents; i++) if (g_ga.events[i].codes[0]) LOGI("evt f=%d c0=%04x", g_ga.events[i].frame, g_ga.events[i].codes[0]); }
          else g_frame = g_ga.endF;
        }
      }
    }
    int tapr = in.touch.taps100ms; /* same last-frame-delta semantics as pre-pad build */
    render(w, h);
    frames++;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long long now = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    /* tap->frame-completion latency (post-render timestamp; ~= present) */
    long long td = g_tapDownT.exchange(0);
    if (td) { long long lat = now - td; if (lat > 0 && lat < 1000000000LL) { if (lat > g_latMax) g_latMax = lat; g_latN++; } }
    if (!fpsT0) fpsT0 = now;
    if (now - fpsT0 >= 2000000000LL) {
      int millic = -1;
      if (FILE* tf = fopen("/sys/class/thermal/thermal_zone0/temp", "r")) {
        long long v; if (fscanf(tf, "%lld", &v) == 1) millic = (int)v;
        fclose(tf);
      }
      size_t rused, rhits, rmiss, rfails; g_res.stats(&rused, &rhits, &rmiss, &rfails);
      LOGI("fps=%d taps2s=%d tile=%d,%d path=%d camYaw=%.0f tapLatMax=%lldus tapLatN=%d th=%dC res=%zuK h=%zu m=%zu f=%zu",
           frames * 500000000LL / (now - fpsT0), tapr, g_gx, g_gy, (int)any_moving(), g_camYaw.load(),
           g_latMax / 1000, g_latN, millic < 0 ? -1 : millic / 1000,
           rused >> 10, rhits, rmiss, rfails);
      frames = 0; fpsT0 = now; g_latMax = 0; g_latN = 0;
    }
    (void)last;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_fe9_poc_MainActivity_nativeStart(JNIEnv* env, jobject, jobject assetManager) {
  LOGI("=== FE9 P2 PoC boot ===");
  AAssetManager* am = AAssetManager_fromJava(env, assetManager);
  std::thread([am] { engine_main(am); }).detach();
}
