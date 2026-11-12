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

#define FPK_IMPL
#define CMSDEC_IMPL
#include "fpk.h"
#include "cmsdec.h"
#include "gload.h"
#include "gsload.h"
#include "gaanim.h"
#include "texfile.h"
#include "ui.h"
#include "stmdec.h"
#include "resmgr.h"
#include "input.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FE9PoC", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FE9PoC", __VA_ARGS__)

static ActQueue g_acts;  // [P3-INP-001] one edge queue, three producers (touch/pad/keyboard)
static std::atomic<float> g_touchYaw{0.f};
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
             std::atomic<float> tgt{0.f}; float g = 0.f; } s[NSLOT];
  std::atomic<float> master{1.f}, duck{1.f};
  int seNext = NBGM;
  oboe::AudioStream* stream = nullptr;
  void bindBgm(int i, const stm_file& d) { /* call on engine thread before start */
    s[i].p = d.pcm; s[i].n = d.pcmFrames; s[i].pos = 0; s[i].loop = d.loopStart >= 0 ? d.loopStart : 0;
    s[i].tgt.store(i == 0 ? 0.8f : 0.f);
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
        c.g += (tgt - c.g) * (i < NBGM ? 0.02f : 0.3f); /* BGM slow crossfade, SE fast attack */
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
static float g_fDim = 1.f;              // 1 = black, 0 = full image (uDim multiplier)
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
precision mediump float; in vec3 vN; in vec2 vUV; uniform sampler2D uTex; uniform float uDim; out vec4 o;
void main(){ vec3 c=texture(uTex,vUV).rgb; float l=0.35+0.65*max(dot(normalize(vN),normalize(vec3(0.4,0.8,0.6))),0.0); o=vec4(c*l*uDim,1.0); })";
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
static const char* const GA_LABEL[4] = {"move_n", "move_bw", "wait_n", "wait_bw"};
static tpl_file g_tex; static tpl_file g_texA; static uint32_t g_texId = 0;
static GLuint g_vao = 0, g_vbo = 0; static int g_triCount = 0;
static GLuint g_prog = 0, g_prog2 = 0, g_prog3 = 0;
/* ---------- UI engine [P3-UI-001] ---------- */
static UiFont g_font;                       /* talk.gcf atlas: real FE9 dialog font */
static tpl_file g_fuki, g_arrow;            /* real window chrome: bubble + cursor anim */
static GLuint g_fukiTex = 0, g_arrowTex = 0, g_whiteTex = 0;
static GLuint g_uiVao = 0, g_uiVbo = 0;
static UiBatch g_uiB;
static int g_dlgPage = 0; static long long g_dlgT0 = 0; static int g_dlgAll = 0;
static std::vector<uint8_t> g_gcf;
static std::vector<uint8_t> g_stmA, g_stmB;
static stm_file g_sA, g_sB;
static std::atomic<float> g_camYaw{45.f}; /* deg; FIXED isometric default, pad-orbit writable */
static float g_unitYaw = 45.f;
static std::atomic<float> g_camPitch{24.f}; /* deg; RIGHT_STICK look, L3/R3 refit resets it */
static int g_gx = 4, g_gy = 4;        /* unit tile coords 0..9 */
static float g_ux = -5.f, g_uz = -5.f; /* unit world pos (tile centers: -45+10*g) */
static float g_camFx = 0.f, g_camFz = 0.f; /* smoothed camera focus */
static int g_path[48]; static int g_pathN = 0; /* queued tiles, flat (gx,gy) FIFO */
static std::atomic<int> g_tapReq{0};
static std::atomic<float> g_tapNX{0.f}, g_tapNY{0.f};
static float g_scrW = 1.f, g_scrH = 1.f;
static int g_frame = 0; static long long g_frameMs = 0;

/* ---------- grid movement ----------
 * Tile = 10 world units, grid lines at -50..50 step 10; tile (gx,gy) center =
 * (-45+10gx, -45+10gy). Path = FIFO of tile coords. Unit yaw = atan2(dx,dz)
 * (model front assumed +Z at identity; flip +180 if vision shows otherwise). */
#define TILE_W 10.f
static float tile_cx(int g) { return -45.f + 10.f * g; }
static void path_push(int tx, int ty) {
  if (g_pathN >= 22) return; /* spam cap */
  g_path[2 * g_pathN] = tx; g_path[2 * g_pathN + 1] = ty; g_pathN++;
}
static void step_tile(int dgx, int dgy) { /* enqueue 1 tile step from queue tail */
  int sx = g_gx, sy = g_gy;
  if (g_pathN > 0) { sx = g_path[2 * (g_pathN - 1)]; sy = g_path[2 * (g_pathN - 1) + 1]; }
  int nx = sx + dgx, ny = sy + dgy;
  if (nx < 0 || nx > 9 || ny < 0 || ny > 9) return;
  g_unitYaw = atan2f((float)dgx, (float)dgy) * 57.29578f;
  path_push(nx, ny);
}
/* dpad button (1 L,2 R,3 U,4 D) -> best world axis so that motion matches the
   screen direction under the current camera yaw (any yaw works, iso 45 too) */
static void dpad_step(int code) {
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
  step_tile(axes[best][0], axes[best][1]);
}
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
  int gx = (int)floorf((hx + 50.f) / TILE_W), gy = (int)floorf((hz + 50.f) / TILE_W);
  if (gx < 0) gx = 0; if (gx > 9) gx = 9; if (gy < 0) gy = 0; if (gy > 9) gy = 9;
  *tx = gx; *ty = gy;
}
static void queue_path(int tx, int ty) { /* x-first L-path from current tile */
  int dx = tx - g_gx, dy = ty - g_gy, n = abs(dx) + abs(dy);
  if (n > 18) { n = 18; } /* clamp long taps */
  int cx = g_gx, cy = g_gy, sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
  for (int i = 0; i < n; i++) {
    if (cx != tx) cx += sx; else if (cy != ty) cy += sy; else break;
    path_push(cx, cy);
  }
}

static void load_asset(AAssetManager* am) {
  /* Persistent storage: loaders keep raw-data pointers (posT/name/etc);
   * a local vector here would dangle at upload time. */
  static std::vector<uint8_t> buf_g, buf_gs, buf_cms, buf_tex;
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
static void dialog_advance(void) {
  int shown = (int)((g_playFrames - g_dlgT0) / 2);
  int len = (int)strlen(DLG_TEXT[g_dlgPage]);
  if (!g_dlgAll && shown < len) { g_dlgAll = 1; return; } /* A = reveal all first */
  if (g_dlgPage < 2) { g_dlgPage++; g_dlgT0 = g_playFrames; g_dlgAll = 0; }
  else scene_req(3, SC_ATTRACT);                          /* end of page stack: pop */
}
static void draw_dialog(int w, int h) {
  static const float C_FRM[4] = {0.13f, 0.15f, 0.22f, 0.96f};
  static const float C_PALE[4] = {0.92f, 0.91f, 0.87f, 0.96f};
  static const float C_BODY[4] = {0.09f, 0.08f, 0.07f, 1.f};
  static const float C_WHITE[4] = {1.f, 1.f, 1.f, 1.f};
  float bx = w * 0.055f, by = h * 0.66f, bw = w * 0.89f, bh = h * 0.27f;
  float pad = bw * 0.012f;
  float sc = h / 540.f; /* glyph units are GC-native ~19px lines: 2x at 1080p */
  ui_batch_reset(&g_uiB);
  /* FE9-style pale plate with dark frame (procedural until rectdesc in M2.3) */
  ui_fill(&g_uiB, bx, by, bx + bw, by + bh);
  ui_flush(g_whiteTex, C_FRM, w, h);
  ui_batch_reset(&g_uiB);
  ui_fill(&g_uiB, bx + pad, by + pad, bx + bw - pad, by + bh - pad);
  ui_flush(g_whiteTex, C_PALE, w, h);
  /* nameplate: real fukidasi banner image above the box, real font name */
  if (g_fukiTex && g_fuki.count > 1 && g_fuki.imgs[1].ok) {
    float pw = bw * 0.22f, ph = pw * g_fuki.imgs[1].h / g_fuki.imgs[1].w;
    float px = bx + bw * 0.015f, py = by - ph * 0.5f;
    ui_batch_reset(&g_uiB);
    ui_quad(&g_uiB, px, py, px + pw, py + ph, 0, 0, 1, 1);
    ui_flush(g_fukiTex, C_WHITE, w, h);
    float ns = sc * 1.05f;
    float tw = ui_text_w(&g_font, DLG_NAME[g_dlgPage], ns);
    ui_batch_reset(&g_uiB);
    ui_text(&g_uiB, &g_font, px + (pw - tw) * 0.5f, py + ph * 0.22f, ns,
            DLG_NAME[g_dlgPage], pw, 1, 1 << 30);
    ui_flush(g_font.tex, C_BODY, w, h);
  }
  /* body: word-wrap + typewriter (2 chars/frame, A to reveal) */
  int visible = g_dlgAll ? (1 << 30) : (int)((g_playFrames - g_dlgT0) / 2);
  ui_batch_reset(&g_uiB);
  ui_text(&g_uiB, &g_font, bx + bw * 0.025f, by + bh * 0.14f, sc,
          DLG_TEXT[g_dlgPage], bw * 0.9f, 4, visible);
  ui_flush(g_font.tex, C_BODY, w, h);
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
static void build_grid() {
  glGenTextures(1, &g_texWhite);
  glBindTexture(GL_TEXTURE_2D, g_texWhite);
  const unsigned char w4[16] = {220, 220, 230, 255, 220, 220, 230, 255,
                                220, 220, 230, 255, 220, 220, 230, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, w4);
  std::vector<float> V;
  for (int i = -5; i <= 5; i++) {
    float c = i * 10.f;
    for (int s = 0; s < 2; s++) { /* [p3 n3 uv2] line along x and along z */
      float x0 = s ? c : -50.f, z0 = s ? -50.f : c;
      float x1 = s ? c : 50.f, z1 = s ? 50.f : c;
      V.insert(V.end(), {x0, 0, z0, 0, 1, 0, 0, 0, x1, 0, z1, 0, 1, 0, 0, 0});
    }
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
  /* unit transform: Ry(unitYaw)*translate. model front assumed +Z (flip 180 if vision says so) */
  float yc = cosf(g_unitYaw * 0.017453292f), ys = sinf(g_unitYaw * 0.017453292f);
  mat4 model = {};
  model.m[0] = yc; model.m[2] = -ys; model.m[5] = 1; model.m[8] = ys; model.m[10] = yc; model.m[15] = 1;
  model.m[12] = g_ux; model.m[13] = 0; model.m[14] = g_uz;
  mat4 vp = m4mul(proj, view);
  mat4 mvp = m4mul(vp, model);

  if (g_triCount) {
    glUseProgram(g_prog);
    glUniform1f(glGetUniformLocation(g_prog, "uDim"), g_fDim * g_sceneDim);
    glUniformMatrix4fv(glGetUniformLocation(g_prog, "uMVP"), 1, GL_FALSE, mvp.m);
    glUniform1i(glGetUniformLocation(g_prog, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    if (g_texId) glBindTexture(GL_TEXTURE_2D, g_texId);
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_triCount * 3);
  }
  if (g_gridN) { /* grid + cursor ignore unit model matrix */
    glUniformMatrix4fv(glGetUniformLocation(g_prog, "uMVP"), 1, GL_FALSE, vp.m);
    glBindTexture(GL_TEXTURE_2D, g_texWhite);
    glBindVertexArray(g_gridVao);
    glDrawArrays(GL_LINES, 0, g_gridN);
    /* tile cursor: bright square under unit */
    static GLuint curVao = 0, curVbo = 0;
    if (!curVao) {
      glGenVertexArrays(1, &curVao); glGenBuffers(1, &curVbo);
      glBindVertexArray(curVao); glBindBuffer(GL_ARRAY_BUFFER, curVbo);
      glBufferData(GL_ARRAY_BUFFER, 48 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
      glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    }
    float cx = tile_cx(g_gx), cz = tile_cx(g_gy), s = 4.4f, y0 = 0.2f;
    float c[6][2] = { {cx-s,cz-s}, {cx+s,cz-s}, {cx+s,cz+s}, {cx-s,cz-s}, {cx+s,cz+s}, {cx-s,cz+s} };
    float cv[48];
    for (int i = 0; i < 6; i++) {
      cv[i*8+0] = c[i][0]; cv[i*8+1] = y0; cv[i*8+2] = c[i][1];
      cv[i*8+3] = 0; cv[i*8+4] = 1; cv[i*8+5] = 0; cv[i*8+6] = 0; cv[i*8+7] = 0;
    }
    glBindBuffer(GL_ARRAY_BUFFER, curVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof cv, cv);
    glBindVertexArray(curVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  if (g_sm.top() == SC_DIALOG && g_font.ok) draw_dialog(w, h); /* [P3-UI-001] */
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
static void scene_req(int op, SceneId id) {
  if (g_fd == FD_NONE) { g_fd = FD_OUT; g_fdOp = op; g_fdId = id; }
}
static void scene_enter(SceneId s) {
  if (s == SC_DIALOG) { g_dlgPage = 0; g_dlgT0 = g_playFrames; g_dlgAll = 0; }
  g_sceneDim = (s == SC_ATTRACT || s == SC_WORLD) ? 1.f : 0.5f;
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
  float dT = s == SC_DIALOG ? 22.f : s == SC_BATTLE ? 30.f : s == SC_ATTRACT ? 40.f : 55.f;
  g_camDist += (dT - g_camDist) * 0.05f;
  if (s == SC_ATTRACT) g_camYaw.store(g_camYaw.load() + 0.15f);  /* slow showcase orbit */
  else if (s == SC_BATTLE) g_camYaw.store(g_camYaw.load() + 0.4f);
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
  g_gx = s.gx; g_gy = s.gy; g_ux = tile_cx(g_gx); g_uz = tile_cx(g_gy);
  g_pathN = 0; g_camYaw.store(s.camYaw);
  g_gaSel = s.gaSel & 3; if (g_gas[g_gaSel].endF > 0) { memcpy(&g_ga, &g_gas[g_gaSel], sizeof(g_ga)); g_frame = g_ga.startF; }
  g_mixB.store(s.mixB ? 1 : 0); g_mixer.bgmMix(g_mixB.load());
  g_playFrames = s.playFrames;
  g_rng = rngSeedFromState(s); /* [P3-SAV-001] seed DERIVED from state, never clock */
  LOGI("save: slot%d loaded tile=%d,%d frames=%u seed=%08x (rng reseeded from state)", slot, s.gx, s.gy, s.playFrames, g_rng);
  return true;
}
static void doNew(int slot) {
  bool had = g_saves.removeSlot(slot);
  g_gx = 4; g_gy = 4; g_ux = tile_cx(4); g_uz = tile_cx(4); g_pathN = 0;
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
  switch (action) {
    case ACT_LEFT: case ACT_RIGHT: case ACT_UP: case ACT_DOWN:
      dpad_step((int)action); break; /* screen-direction -> world axis */
    case ACT_CONFIRM: g_mixB.store(1.f - g_mixB.load()); g_mixer.bgmMix(g_mixB.load()); break; // A = mix B on/off
    case ACT_CANCEL: { /* B = cycle anim set */
      g_gaSel = (g_gaSel + 1) & 3;
      if (g_gas[g_gaSel].endF > 0) { memcpy(&g_ga, &g_gas[g_gaSel], sizeof(g_ga)); g_frame = g_ga.startF; }
      LOGI("anim -> %s frames %d..%d", GA_LABEL[g_gaSel], g_ga.startF, g_ga.endF);
      break;
    }
  }
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
  ui_init();
  upload_mesh();
  build_grid();
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
            a == ACT_CONFIRM || a == ACT_CANCEL) scene_req(1, SC_WORLD);
      } else if (top == SC_WORLD) {
        if (a == ACT_SAVE) { doSave(0); continue; }
        if (a == ACT_NEW) { doNew(0); continue; }
        if (a == ACT_LOAD) { doLoad(0); continue; }
        if (a == ACT_DIALOG) scene_req(2, SC_DIALOG);
        else if (a == ACT_BATTLE) scene_req(2, SC_BATTLE);
        else on_input(a);
      } else {                             /* dialog/battle overlay any scene */
        if (top == SC_DIALOG && a == ACT_CONFIRM) dialog_advance();
        else if (a == ACT_CONFIRM || a == ACT_CANCEL || a == ACT_BACK) scene_req(3, SC_ATTRACT);
      }
    }
    scene_tick();
    g_playFrames++;
    if (g_sm.top() == SC_WORLD && in.touch.tapReq) {
      int tx = -1, ty = -1;
      tap_to_tile(in.touch.tapNX, in.touch.tapNY, &tx, &ty);
      if (tx >= 0 && !g_paused.load()) { LOGI("tap->tile %d,%d (unit %d,%d)", tx, ty, g_gx, g_gy); queue_path(tx, ty); }
    }
    /* step along queued path: 1.4 world-units/frame ~ 0.35 s/tile @20fps */
    if (g_pathN > 0 && !g_paused.load() && g_sm.top() == SC_WORLD) {
      int tgx = g_path[0], tgy = g_path[1];
      float tx = tile_cx(tgx), tz = tile_cx(tgy);
      float dx = tx - g_ux, dz = tz - g_uz;
      float dl = sqrtf(dx * dx + dz * dz);
      if (dl < 1.45f) {
        g_ux = tx; g_uz = tz; g_gx = tgx; g_gy = tgy;
        for (int i = 1; i < g_pathN; i++) { g_path[2*i-2] = g_path[2*i]; g_path[2*i-1] = g_path[2*i+1]; }
        g_pathN--;
      } else { g_ux += dx / dl * 1.4f; g_uz += dz / dl * 1.4f; }
    }
    /* follow camera: FE9-style FIXED isometric yaw (no auto-rotate: keeps dpad
       directions world-stable); only focus position trails the unit */
    g_camFx += (g_ux - g_camFx) * 0.06f;
    g_camFz += (g_uz - g_camFz) * 0.06f;
    // advance ga: ~12 anim fps (frozen while paused or under an overlay scene;
    //   attract keeps idling so the showcase unit breathes)
    bool frozen = g_paused.load() || (g_sm.top() != SC_WORLD && g_sm.top() != SC_ATTRACT);
    if (!frozen) g_frameMs++;
    if (g_frameMs >= 3) {
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
           frames * 500000000LL / (now - fpsT0), tapr, g_gx, g_gy, g_pathN, g_camYaw.load(),
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
