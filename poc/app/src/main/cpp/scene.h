/* scene.h — P3-M1.3 [P3-SCN-001]: scene stack + fade-transition driver.
 * Contract: ONE scene (the top of the stack) receives actions each frame;
 * push/pop/replace only ever execute at fade==1 (scene cut invisible).
 * FE9 product rule enforced here: cold boot lands on ATTRACT; only explicit
 * start input (any dpad/A/B edge) replaces it with WORLD. */
#ifndef SCENE_H_
#define SCENE_H_
#include <cstdint>

enum SceneId : uint8_t { SC_ATTRACT = 0, SC_WORLD, SC_DIALOG, SC_BATTLE, SC_COUNT };

inline const char* scene_name(SceneId s) {
  switch (s) { case SC_ATTRACT: return "attract"; case SC_WORLD: return "world";
    case SC_DIALOG: return "dialog"; case SC_BATTLE: return "battle"; default: return "?"; }
}

class SceneStack {
 public:
  SceneStack() { s_[0] = SC_ATTRACT; n_ = 1; }  // cold boot = attract, never gameplay
  SceneId top() const { return s_[n_ - 1]; }
  int depth() const { return n_; }
  bool canPop() const { return n_ > 1; }        // never pop the root (attract)
  void push(SceneId s) { if (n_ < kMax) s_[n_++] = s; }
  void pop() { if (n_ > 1) n_--; }
  void replace(SceneId s) { s_[n_ - 1] = s; }
 private:
  static const int kMax = 4;
  SceneId s_[kMax];
  int n_;
};

/* fade machine: OUT fades to black, applies the queued op, IN fades back. */
enum FadeState : uint8_t { FD_NONE = 0, FD_OUT, FD_IN };

#endif /* SCENE_H_ */
