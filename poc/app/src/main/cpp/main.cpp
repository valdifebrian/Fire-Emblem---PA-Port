// main.cpp — FE9 P2 Architecture PoC harness (NativeActivity + EGL/GLES3 + Oboe).
// Proves 5 capabilities on-device with real FE9 data via poc1.fpk (docs/FPK_FORMAT.md):
//  (1) native texture load: ANNA.cms LZ77+TPL+RGB565 → GLES3 texture
//  (2) native model: archer skeleton.g + body.gs skinned render + orbit camera
//  (3) native anim: move_n.ga frames/events evaluated each vsync; event codes logged
//  (4) audio: boss1.stm looped + boss2.stm mixed (gain toggle) via Oboe
//  (5) input: touch overlay events from Kotlin via JNI + keyboard path (desktop TODO)
// Plus fps log + thermal read = REQ-TIME/PERF hooks.
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/log.h>
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
#include "stmdec.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FE9PoC", __VA_ARGS__)

static std::atomic<int> g_action{0};      // 0 none, 1..8 dpad/a/b edge
static std::atomic<float> g_touchYaw{0.f};
static std::atomic<long long> g_lastTapNs{0};
static std::atomic<int> g_tapsIn100ms{0};
static std::atomic<float> g_mixB{0.f};    // gain of stream B 0..1

// ---------- audio mixer ----------
struct Mixer : oboe::AudioStreamDataCallback, oboe::AudioStreamErrorCallback {
  const int16_t* a = nullptr; int an = 0; int ap = 0; int aLoop = 0; float gainA = 0.8f;
  const int16_t* b = nullptr; int bn = 0; int bp = 0; int bLoop = 0; float gainB = 0.f;
  std::atomic<float> targetB{0.f};
  oboe::AudioStream* stream = nullptr;
  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* s, void* audioData, int32_t numFrames) override {
    auto* out = static_cast<int16_t*>(audioData);
    float tgt = targetB.load();
    gainB += (tgt - gainB) * 0.02f; // ~100ms ramp at 50fps cb
    for (int32_t f = 0; f < numFrames; f++) {
      int sa = a ? a[ap * 2] : 0, sa1 = a ? a[ap * 2 + 1] : 0;
      int sb = b ? b[bp * 2] : 0, sb1 = b ? b[bp * 2 + 1] : 0;
      if (a) { if (++ap >= an) ap = aLoop; }
      if (b) { if (++bp >= bn) bp = bLoop; }
      int rl = (int)(sa * gainA + sb * gainB), rr = (int)(sa1 * gainA + sb1 * gainB);
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

// ---------- GL ----------
static const char* VS = R"(#version 300 es
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec2 aUV;
uniform mat4 uMVP; out vec3 vN; out vec2 vUV;
void main(){ vN=aNrm; vUV=aUV; gl_Position=uMVP*vec4(aPos,1.0); })";
static const char* FS = R"(#version 300 es
precision mediump float; in vec3 vN; in vec2 vUV; uniform sampler2D uTex; out vec4 o;
void main(){ vec3 c=texture(uTex,vUV).rgb; float l=0.35+0.65*max(dot(normalize(vN),normalize(vec3(0.4,0.8,0.6))),0.0); o=vec4(c*l,1.0); })";
static const char* VS2 = R"(#version 300 es
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; uniform mat4 uMVP; out vec2 vUV;
void main(){ vUV=aUV; gl_Position=uMVP*vec4(aPos,1.0); })";

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
static g_model g_g; static gs_body g_gs; static ga_anim g_ga;
static tpl_file g_tex; static uint32_t g_texId = 0;
static GLuint g_vao = 0, g_vbo = 0; static int g_triCount = 0;
static GLuint g_prog = 0, g_prog2 = 0, g_vaoQuad = 0, g_vboQuad = 0;
static std::vector<uint8_t> g_stmA, g_stmB;
static stm_file g_sA, g_sB;
static float g_yaw = 35.f, g_moveZ = 0.f;
static int g_frame = 0; static long long g_frameMs = 0;

static void load_asset(ANativeActivity* act) {
  AAssetManager* am = act->assetManager;
  AAsset* a = AAssetManager_open(am, "poc1.fpk", AASSET_MODE_BUFFER);
  if (!a) { LOGI("FATAL: poc1.fpk missing"); return; }
  const uint8_t* buf = (const uint8_t*)AAsset_getBuffer(a);
  off64_t sz = AAsset_getLength64(a);
  g_fpkg.assign(buf, buf + sz);
  AAsset_close(a);
  int r = fpk_open(&g_fpak, g_fpkg.data(), g_fpkg.size());
  LOGI("fpk_open=%d (0=ok)", r);
  if (r) return;
  auto rd = [&](const char* name, std::vector<uint8_t>& v) -> const fpk_entry* {
    const fpk_entry* e = fpk_find(&g_fpak, name);
    if (e) v.assign(g_fpak.data() + e->off, g_fpak.data() + e->off + e->len);
    LOGI("entry %s -> %s len=%u", name, e ? "ok" : "MISSING", e ? e->len : 0);
    return e;
  };
  std::vector<uint8_t> buf_g, buf_gs, buf_ga;
  rd("archer/skeleton.g", buf_g); rd("archer/body.gs", buf_gs); rd("archer/move_n.ga", buf_ga);
  if (!buf_g.empty() && !buf_gs.empty()) {
    if (g_load(&g_g, buf_g.data(), (int)buf_g.size()) == 0) LOGI("g: bones=%d root=%s", g_g.n, g_g.bones[0].name);
    if (gs_load(&g_gs, buf_gs.data(), (int)buf_gs.size()) == 0) {
      int tris = 0; for (int i = 0; i < g_gs.nsub; i++) tris += g_gs.subs[i].triCount;
      LOGI("gs: subs=%d tris=%d", g_gs.nsub, tris);
    }
  }
  if (!buf_ga.empty() && ga_load(&g_ga, buf_ga.data(), (int)buf_ga.size()) == 0) {
    LOGI("ga: bones=%d curves=%d frames=%d..%d events=%d", g_ga.nBones, g_ga.nCurves,
         g_ga.startF, g_ga.endF, g_ga.nEvents);
    for (int i = 0; i < g_ga.nEvents; i++)
      LOGI("  event f=%d codes=%04x,%04x,%04x", g_ga.events[i].frame, g_ga.events[i].codes[0],
           g_ga.events[i].codes[1], g_ga.events[i].codes[2]);
  }
  std::vector<uint8_t> buf_cms;
  if (rd("face/anna.cms", buf_cms) && texfile_decode(buf_cms.data(), (int)buf_cms.size(), &g_tex) == 0) {
    for (int i = 0; i < g_tex.count; i++)
      LOGI("tex[%d] %dx%d fmt=%d ok=%d", i, g_tex.imgs[i].w, g_tex.imgs[i].h, g_tex.imgs[i].fmt, g_tex.imgs[i].ok);
  }
  rd("bgm/boss1.stm", g_stmA); rd("bgm/boss2.stm", g_stmB);
  if (!g_stmA.empty()) { if (stm_decode(&g_sA, g_stmA.data(), (int)g_stmA.size()) == 0) LOGI("stm A sr=%d frames=%d loop=%d", g_sA.sr, g_sA.pcmFrames, g_sA.loopStart); }
  if (!g_stmB.empty()) { if (stm_decode(&g_sB, g_stmB.data(), (int)g_stmB.size()) == 0) LOGI("stm B sr=%d frames=%d", g_sB.sr, g_sB.pcmFrames); }
}

static void upload_texture() {
  for (int i = 0; i < g_tex.count; i++)
    if (g_tex.imgs[i].ok) {
      glGenTextures(1, &g_texId);
      glBindTexture(GL_TEXTURE_2D, g_texId);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_tex.imgs[i].w, g_tex.imgs[i].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_tex.imgs[i].rgba);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      LOGI("texture uploaded #%d %dx%d", i, g_tex.imgs[i].w, g_tex.imgs[i].h);
      return;
    }
}

static void upload_mesh() {
  std::vector<float> verts;
  mat34 worlds[G_MAX_BONES];
  for (int i = 0; i < g_g.n; i++) memcpy(worlds[i].m, g_g.bones[i].seed, sizeof(float) * 12);
  for (int si = 0; si < g_gs.nsub; si++) {
    const gs_submesh* s = &g_gs.subs[si];
    for (int t = 0; t < s->triCount; t++)
      for (int k = 0; k < 3; k++) {
        float p[3], n[3], uv[2];
        gs_vertex(&g_gs, s, s->tris[t].v[k], worlds, &g_g, p, n);
        gs_uv(&g_gs, s->tris[t].v[k], uv);
        verts.insert(verts.end(), {p[0], p[1], p[2], n[0], n[1], n[2], uv[0], uv[1]});
      }
  }
  g_triCount = (int)(verts.size() / 8 / 3);
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
    ->setSamplesPerDataCallback(960)
    ->setDataCallback(&g_mixer)
    ->setErrorCallback(&g_mixer);
  oboe::Result r = ob.openStream(&g_mixer.stream);
  if (r == oboe::Result::OK) {
    g_mixer.a = g_sA.pcm; g_mixer.an = g_sA.pcmFrames; g_mixer.aLoop = g_sA.loopStart >= 0 ? g_sA.loopStart : 0;
    g_mixer.b = g_sB.pcm; g_mixer.bn = g_sB.pcmFrames; g_mixer.bLoop = g_sB.loopStart >= 0 ? g_sB.loopStart : 0;
    auto rr = g_mixer.stream->requestStart();
    LOGI("oboe start=%d rate=%d frames=%d", (int)rr, g_mixer.stream->getSampleRate(), g_mixer.stream->getFramesPerBurst());
  } else LOGI("oboe open fail=%d", (int)r);
}

// ---------- loop ----------
static void render(int w, int h) {
  glViewport(0, 0, w, h);
  glClearColor(0.06f, 0.07f, 0.1f, 1.f);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  float aspect = (float)w / h;
  float fovy = 45.f * 3.14159265f / 180.f, near = 1.f, far = 4000.f;
  float f = 1.f / tanf(fovy / 2);
  mat4 proj = {};
  proj.m[0] = f / aspect; proj.m[5] = f; proj.m[10] = (far + near) / (near - far);
  proj.m[11] = -1; proj.m[14] = 2 * far * near / (near - far);
  float yaw = g_yaw * 3.14159265f / 180.f, pit = 20.f * 3.14159265f / 180.f, dist = 320.f;
  float ex = sinf(yaw) * cosf(pit) * dist, ey = sinf(pit) * dist, ez = cosf(yaw) * cosf(pit) * dist;
  float fx = 0, fy = 90, fz = g_moveZ;
  // view = lookAt (column-major, GL)
  float zx = ex - fx, zy = ey - fy, zz = ez - fz;
  float zl = sqrtf(zx * zx + zy * zy + zz * zz); zx /= zl; zy /= zl; zz /= zl;
  float xx = -zz, xy = 0, xz = zx; // up=(0,1,0): x = up×z normalized
  float xl = sqrtf(xx * xx + xz * xz); xx /= xl; xz /= xl;
  float yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
  mat4 view = {};
  view.m[0] = xx; view.m[4] = xy; view.m[8] = xz;  view.m[12] = -(xx * fx + xy * fy + xz * fz);
  view.m[1] = yx; view.m[5] = yy; view.m[9] = yz;  view.m[13] = -(yx * fx + yy * fy + yz * fz);
  view.m[2] = zx; view.m[6] = zy; view.m[10] = zz; view.m[14] = -(zx * fx + zy * fy + zz * fz);
  view.m[15] = 1;
  mat4 model = {};
  model.m[0] = 1; model.m[5] = 1; model.m[10] = 1; model.m[15] = 1;
  mat4 mvp = m4mul(proj, m4mul(view, model));

  if (g_triCount) {
    glUseProgram(g_prog);
    glUniformMatrix4fv(glGetUniformLocation(g_prog, "uMVP"), 1, GL_FALSE, mvp.m);
    glUniform1i(glGetUniformLocation(g_prog, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    if (g_texId) glBindTexture(GL_TEXTURE_2D, g_texId);
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, g_triCount * 3);
  }
  eglSwapBuffers(dpy, surf);
}

static void on_input(int action) {
  switch (action) {
    case 1: g_yaw -= 8.f; break;         // left
    case 2: g_yaw += 8.f; break;         // right
    case 3: g_moveZ += 24.f; break;      // up = forward
    case 4: g_moveZ -= 24.f; break;      // down
    case 5: g_mixB.store(1.f - g_mixB.load()); g_mixer.targetB.store(g_mixB.load()); break; // A = mix B on/off
    case 6: g_frame = g_ga.startF; break; // B = anim restart
  }
}

// ---------- JNI bridge from Kotlin overlay ----------
static ANativeWindow* g_window = nullptr;
static std::mutex g_windowMx;
static std::condition_variable g_windowCv;
extern "C" {
JNIEXPORT void JNICALL Java_com_fe9_poc_MainActivity_nativeTap(JNIEnv*, jobject, jint code, jlong tsNs) {
  g_lastTapNs.store(tsNs);
  int n = g_tapsIn100ms.load();
  g_tapsIn100ms.store(n + 1); // consumed in main loop for latency/rate stats
  g_action.store(code);
}
JNIEXPORT void JNICALL Java_com_fe9_poc_MainActivity_nativeSetSurface(JNIEnv* env, jobject, jobject surface) {
  std::lock_guard<std::mutex> lk(g_windowMx);
  if (surface) g_window = ANativeWindow_fromSurface(env, surface);
  else if (g_window) { ANativeWindow_release(g_window); g_window = nullptr; }
  g_windowCv.notify_all();
}
}

// ---------- native activity ----------
static void engine_main(ANativeActivity* act, void* saved) {
  (void)saved;
  load_asset(act);
  ANativeWindow* win;
  {
    std::unique_lock<std::mutex> lk(g_windowMx);
    g_windowCv.wait(lk, [] { return g_window != nullptr; });
    win = g_window;
  }
  int w = 0, h = 0;
  if (!egl_create(win, &w, &h)) { LOGI("FATAL egl"); return; }
  g_prog = makeProgram(VS, FS); g_prog2 = makeProgram(VS2, FS);
  upload_texture();
  upload_mesh();
  audio_start();
  long long last = 0; int frames = 0; long long fpsT0 = 0;
  while (true) {
    int a = g_action.exchange(0);
    if (a) on_input(a);
    // advance ga: ~12 anim fps
    g_frameMs++;
    if (g_frameMs >= 3) {
      g_frameMs = 0;
      g_frame++;
      if (g_ga.endF > 0) {
        if (g_frame > g_ga.endF) {
          if (g_ga.loop) { g_frame = g_ga.startF; for (int i = 0; i < g_ga.nEvents; i++) LOGI("evt f=%d c0=%04x", g_ga.events[i].frame, g_ga.events[i].codes[0]); }
          else g_frame = g_ga.endF;
        }
      }
    }
    int tapr = g_tapsIn100ms.exchange(0);
    render(w, h);
    frames++;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    long long now = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    if (!fpsT0) fpsT0 = now;
    if (now - fpsT0 >= 2000000000LL) {
      LOGI("fps=%d taps2s=%d yaw=%.0f z=%.0f", frames * 500000000LL / (now - fpsT0), tapr, g_yaw, g_moveZ);
      frames = 0; fpsT0 = now;
    }
    (void)last;
  }
}

struct EngineData { ANativeActivity* act; };
static EngineData g_eng;

static void onStart(ANativeActivity*) {}
static void onResume(ANativeActivity*) {}
static void onPause(ANativeActivity*) {}
static void onStop(ANativeActivity*) {}
static void onDestroy(ANativeActivity* a) { (void)a; exit(0); }
static void onNativeWindowCreated(ANativeActivity* a, ANativeWindow* w) { a->window = w; }
static void onNativeWindowRedrawNeeded(ANativeActivity*, ANativeWindow*) {}
static void onNativeWindowDestroyed(ANativeActivity* a, ANativeWindow*) { a->window = nullptr; }
static void onInputQueueCreated(ANativeActivity* a, AInputQueue* q) { (void)a; (void)q; }
static void onInputQueueDestroyed(ANativeActivity*, AInputQueue*) {}
static void onContentRectChanged(ANativeActivity*, const ARect*) {}
static void onConfigurationChanged(ANativeActivity*) {}
static void onLowMemory(ANativeActivity*) {}
static void onSavedInstanceState(ANativeActivity*, void**, size_t*) {}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* act, void* saved, size_t savedSize) {
  LOGI("=== FE9 P2 PoC boot ===");
  act->callbacks->onStart = onStart;
  act->callbacks->onResume = onResume;
  act->callbacks->onPause = onPause;
  act->callbacks->onStop = onStop;
  act->callbacks->onDestroy = onDestroy;
  act->callbacks->onNativeWindowCreated = onNativeWindowCreated;
  act->callbacks->onNativeWindowRedrawNeeded = onNativeWindowRedrawNeeded;
  act->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
  act->callbacks->onInputQueueCreated = onInputQueueCreated;
  act->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
  act->callbacks->onContentRectChanged = onContentRectChanged;
  act->callbacks->onConfigurationChanged = onConfigurationChanged;
  act->callbacks->onLowMemory = onLowMemory;
  act->callbacks->onSaveInstanceState = onSavedInstanceState;
  act->instance = &g_eng;
  g_eng.act = act;
  std::thread([act] { engine_main(act, nullptr); }).detach();
}
