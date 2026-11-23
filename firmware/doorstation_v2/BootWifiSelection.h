#pragma once
#include <stdint.h>

// 0 = waiting, 1/2 = button selection, 3 = timeout/default.
// Both buttons together are ignored; a stable single press wins.
class BootWifiSelection {
 public:
  explicit BootWifiSelection(uint32_t started) : started_(started) {}
  uint8_t update(uint32_t now, bool p1, bool p2) {
    if (result_) return result_;
    if ((uint32_t)(now - started_) >= 20000) return result_ = 3;
    uint8_t pressed = p1 == p2 ? 0 : (p1 ? 1 : 2);
    if (pressed != pending_) { pending_ = pressed; changed_ = now; }
    if (pressed && (uint32_t)(now - changed_) >= 40) result_ = pressed;
    return result_;
  }
 private:
  uint32_t started_, changed_ = 0;
  uint8_t pending_ = 0, result_ = 0;
};
