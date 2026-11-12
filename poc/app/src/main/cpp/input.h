/* input.h — P3-M1.2 [P3-INP-001]: ONE semantic Action enum consumed by the engine;
 * producers = overlay touch, physical gamepad, keyboard (future: replay driver —
 * M3 pushes the same enum from recorded input). Numeric values ARE the stable
 * overlay BTN codes used by BTN[]/held[]/Kotlin — NEVER renumber. */
#ifndef INPUT_H_
#define INPUT_H_
#include <cstdint>
#include <mutex>

enum Act : uint8_t {
  ACT_NONE = 0,
  ACT_LEFT = 1, ACT_RIGHT = 2, ACT_UP = 3, ACT_DOWN = 4, /* grid nav, screen-relative */
  ACT_CONFIRM = 5,                                      /* A: menu confirm / harness mix */
  ACT_CANCEL = 6,                                       /* B: back / harness anim cycle */
  ACT_DEBUG_TEX = 7,                                    /* debug-only texture cycle */
  ACT_DIALOG = 8,                                       /* scene-stack: push dialog (pad X) */
  ACT_BATTLE = 9,                                       /* scene-stack: push battle (pad Y) */
  ACT_BACK = 10,                                        /* system back (KEYCODE_BACK) */
  ACT_COUNT = 11
};

/* Tiny mutex ring: producers on UI/USB threads, single consumer on engine frame.
 * 60Hz drain of a 32-slot ring cannot fill unless the engine is wedged; overflow
 * drop is the documented failure mode (telemetry would show it via frozen fps). */
class ActQueue {
 public:
  void push(Act a) {
    if (a == ACT_NONE || a >= ACT_COUNT) return;
    std::lock_guard<std::mutex> lk(mu_);
    if (n_ == kCap) return;
    ring_[(r_ + n_) & (kCap - 1)] = a;
    n_++;
  }
  bool pop(Act* a) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!n_) return false;
    *a = ring_[r_];
    r_ = (r_ + 1) & (kCap - 1);
    n_--;
    return true;
  }

 private:
  static const int kCap = 32;
  std::mutex mu_;
  Act ring_[kCap] = {};
  int r_ = 0, n_ = 0;
};
#endif /* INPUT_H_ */
