#pragma once
#include <stdint.h>
struct WifiRecoveryAction { bool tryOther; bool openSetup; };
class WifiProfileRecovery {
  bool tracking = false;
  uint32_t since = 0, lastAttempt = 0;
public:
  WifiRecoveryAction update(uint32_t now, bool connected, bool candidateActive) {
    if (connected || candidateActive) { tracking = false; return {false, false}; }
    if (!tracking) { tracking = true; since = lastAttempt = now; }
    bool retry = (uint32_t)(now - lastAttempt) >= 30000;
    if (retry) lastAttempt = now;
    return {retry, (uint32_t)(now - since) >= 60000};
  }
};
