/* savemgr.h — P3-M1.5 [P3-SAV-001]: save schema v1 + validation + atomic slot IO.
 * Engine-thread only (file IO off the audio/UI paths). Layout is explicit
 * little-endian-friendly POD, versioned, CRC32-guarded; loader REFUSES anything
 * that fails magic/version/CRC (never trust on-disk bytes). Writes go to .tmp +
 * fsync + rename so a crash mid-write can never destroy the previous save.
 * RNG rule: seed is DERIVED from the save-state hash (deterministic resume),
 * never from wall clock. */
#ifndef SAVEMGR_H_
#define SAVEMGR_H_
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

struct SaveV1 {
  uint32_t magic;      // 'FE9S'
  uint32_t ver;        // schema version = 1
  uint32_t slot;       // slot id echoed for cross-check
  uint32_t crc;        // CRC32 over bytes after this field (payload region)
  /* ---- payload (CRC-covered) ---- */
  uint32_t chapter;    // chapter index, 1-based
  int32_t gx, gy;      // unit tile
  uint32_t playFrames; // cumulative engine frames (elapsed time proxy)
  uint32_t rngSeed;    // RNG state at save
  float camYaw;        // camera yaw at save
  uint8_t gaSel;       // anim-set selection
  uint8_t mixB;        // boss-mix toggle
  uint8_t flags[2];    // reserved bits
  uint32_t reserved[8];
};

inline uint32_t saveCrc(const SaveV1& s) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&s) + 16;
  uint32_t c = 0xFFFFFFFFu;
  size_t n = sizeof(SaveV1) - 16;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (~(c & 1) + 1));
  }
  return ~c;
}

class SaveMgr {
 public:
  static const int kSlots = 3;
  void init(const char* dir) {
    snprintf(dir_, sizeof dir_, "%s", dir);
    mkdir(dir_, 0770);
    char p[300]; snprintf(p, sizeof p, "%s/saves", dir_);
    mkdir(p, 0770);
  }

  int path(int slot, char* out, size_t cap, const char* suffix = "") const {
    return snprintf(out, cap, "%s/saves/slot%d.fe9save%s", dir_, slot, suffix);
  }

  bool save(int slot, SaveV1 s) const {
    if (slot < 0 || slot >= kSlots) return false;
    s.magic = 0x46453953; s.ver = 1; s.slot = (uint32_t)slot;  // 'FE9S' LE
    s.crc = 0; s.crc = saveCrc(s);
    char p[512]; path(slot, p, sizeof p, ".tmp");
    FILE* f = fopen(p, "wb");
    if (!f) return false;
    bool ok = fwrite(&s, sizeof s, 1, f) == 1;
    if (ok) { fflush(f); fsync(fileno(f)); ok = fclose(f) == 0; f = nullptr; }
    if (f) fclose(f);
    if (!ok) { remove(p); return false; }
    char d[512]; path(slot, d, sizeof d);
    if (rename(p, d) != 0) { remove(p); return false; }
    return true;
  }

  /* returns true iff a VALID save filled `out`; corrupt/foreign bytes never leak. */
  bool load(int slot, SaveV1* out) const {
    if (slot < 0 || slot >= kSlots) return false;
    char p[512]; path(slot, p, sizeof p);
    FILE* f = fopen(p, "rb");
    if (!f) return false;
    SaveV1 s;
    bool got = fread(&s, sizeof s, 1, f) == 1;
    fclose(f);
    if (!got) return false;
    if (s.magic != 0x46453953 || s.ver != 1 || s.slot != (uint32_t)slot) return false;
    uint32_t want = s.crc; s.crc = 0;
    if (saveCrc(s) != want) return false;
    *out = s;
    return true;
  }

  bool removeSlot(int slot) const {
    char p[512]; path(slot, p, sizeof p);
    return remove(p) == 0;
  }
  bool exists(int slot) const {
    char p[512]; path(slot, p, sizeof p);
    return access(p, F_OK) == 0;
  }

 private:
  char dir_[256] = {};
};

/* deterministic 32-bit RNG; seed derived from state hash [P3-SAV-001] */
inline uint32_t rngSeedFromState(const SaveV1& s) {
  uint32_t h = saveCrc(s);
  h ^= h >> 13; h *= 0x9E3779B1u; h ^= h >> 16;
  return h ? h : 1u;
}
inline uint32_t rngNext(uint32_t* st) {
  uint32_t x = *st; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *st = x; return x;
}
#endif /* SAVEMGR_H_ */
