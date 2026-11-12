/* resmgr.h — P3-M1.1 [P3-RES-001]: byte-budgeted LRU cache + async prefetch on top
 * of any raw provider (FPK entry copy / override file). Header-only, std-only.
 *
 * Contract:
 *  - read(): pointer/len stay valid until THIS entry is evicted (budget pressure)
 *    or clear(); engine code MUST re-resolve each use (O(1) map hit, zero copy).
 *  - prefetch(): fire-and-forget, deduped; worker never runs the provider under
 *    the cache mutex (IO off-thread).
 *  - Budget evicts least-recently-USED (read or insert), never a name being read.
 *  - clear() only when no outstanding raw pointers are held (scene teardown).
 */
#ifndef RESMGR_H_
#define RESMGR_H_
#include <condition_variable>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ResMgr {
 public:
  typedef bool (*Provider)(const char* name, std::vector<uint8_t>& out);

  void init(Provider p, size_t budget) {
    provider_ = p;
    budget_ = budget ? budget : (16u << 20);
    worker_ = std::thread([this] { loop(); });
  }
  ~ResMgr() { stop(); }
  void stop() {
    { std::lock_guard<std::mutex> lk(mu_); quit_ = true; }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
  }

  bool read(const char* name, const uint8_t** data, int* len) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (auto it = map_.find(name); it != map_.end()) {
        hits_++; touch(it->second.get()); return out(it->second.get(), data, len);
      }
    }
    std::vector<uint8_t> v;                      // provider WITHOUT lock (file/IO)
    if (!provider_(name, v)) {
      std::lock_guard<std::mutex> lk(mu_); fails_++;
      return false;
    }
    std::lock_guard<std::mutex> lk(mu_);
    if (auto it = map_.find(name); it != map_.end()) {  // worker won the race
      hits_++; touch(it->second.get());
      return out(it->second.get(), data, len);
    }
    misses_++;
    evict_until(v.size());
    auto e = std::make_unique<Entry>();
    e->v = std::move(v);
    e->lit = lru_.insert(lru_.begin(), std::string(name));
    Entry* ep = e.get();
    map_.emplace(*ep->lit, std::move(e));
    used_ += ep->v.size();
    return out(ep, data, len);
  }

  void prefetch(const char* name) {
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (map_.count(name) || queued_.count(name)) return;
      queued_.insert(name);
      q_.push_back(name);
    }
    cv_.notify_one();
  }
  bool cached(const char* name) const {
    std::lock_guard<std::mutex> lk(mu_);
    return map_.count(name) != 0;
  }
  void stats(size_t* used, size_t* hits, size_t* misses, size_t* fails) const {
    std::lock_guard<std::mutex> lk(mu_);
    *used = used_; *hits = hits_; *misses = misses_; *fails = fails_;
  }
  void clear() {
    std::lock_guard<std::mutex> lk(mu_);
    map_.clear(); lru_.clear(); used_ = 0;
  }

 private:
  struct Entry {
    std::vector<uint8_t> v;
    std::list<std::string>::iterator lit;
  };
  void touch(Entry* e) { lru_.splice(lru_.begin(), lru_, e->lit); }  // mu_ held
  static bool out(Entry* e, const uint8_t** d, int* n) {
    *d = e->v.data(); *n = (int)e->v.size(); return true;
  }
  void evict_until(size_t need) {  // mu_ held
    while (used_ + need > budget_ && !lru_.empty()) {
      std::string last = lru_.back();
      lru_.pop_back();
      auto it = map_.find(last);
      if (it != map_.end()) { used_ -= it->second->v.size(); map_.erase(it); }
    }
  }
  void loop() {
    for (;;) {
      std::string name;
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [this] { return quit_ || !q_.empty(); });
        if (quit_) return;
        name = q_.front();
        q_.erase(q_.begin());
      }
      std::vector<uint8_t> v;
      bool ok = provider_(name.c_str(), v);      // IO off-thread
      {
        std::lock_guard<std::mutex> lk(mu_);
        queued_.erase(name);
        if (ok && !map_.count(name)) {
          evict_until(v.size());
          auto e = std::make_unique<Entry>();
          e->v = std::move(v);
          e->lit = lru_.insert(lru_.begin(), name);
          used_ += e->v.size();
          map_.emplace(name, std::move(e));
          prefetched_++;
        } else if (!ok) fails_++;
      }
    }
  }

  Provider provider_ = nullptr;
  size_t budget_ = 0, used_ = 0;
  size_t hits_ = 0, misses_ = 0, fails_ = 0, prefetched_ = 0;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::unordered_map<std::string, std::unique_ptr<Entry>> map_;
  std::list<std::string> lru_;              // front = freshest
  std::vector<std::string> q_;              // simple FIFO of pending names
  std::unordered_set<std::string> queued_;  // dedupe set
  bool quit_ = false;
  std::thread worker_;
};
#endif /* RESMGR_H_ */
