#pragma once
#include <stdint.h>
#include <string.h>

// Entscheidungslogik fuer den Wechsel zu einem staerkeren Repeater/Zugangspunkt
// desselben WLANs. Ohne Arduino-Abhaengigkeiten, damit sie nativ testbar ist.

struct WifiRoamingCandidate {
  uint8_t bssid[6];
  int32_t rssi;
  int32_t channel;
  bool sameSsid;
};

class WifiRoamingPolicy {
public:
  static constexpr int32_t WEAK_RSSI_DBM = -70;       // darunter wird gesucht
  static constexpr int32_t MIN_GAIN_DB = 10;          // so viel besser muss ein anderer sein
  static constexpr uint32_t SETTLE_MS = 30000;        // nach Verbindungsaufbau abwarten
  static constexpr uint32_t SCAN_INTERVAL_MS = 120000;// hoechstens alle 2 Minuten suchen

private:
  bool wasConnected = false;
  uint32_t connectedSince = 0;
  bool scanned = false;
  uint32_t lastScan = 0;

public:
  // Wird regelmaessig aufgerufen; liefert true, wenn jetzt gesucht werden soll.
  bool shouldScan(uint32_t now, bool connected, bool idle, int32_t rssi) {
    if (!connected) { wasConnected = false; return false; }
    if (!wasConnected) { wasConnected = true; connectedSince = now; }
    if (!idle || rssi >= WEAK_RSSI_DBM) return false;
    if ((uint32_t)(now - connectedSince) < SETTLE_MS) return false;
    if (scanned && (uint32_t)(now - lastScan) < SCAN_INTERVAL_MS) return false;
    scanned = true;
    lastScan = now;
    return true;
  }

  // Index des besten anderen Zugangspunkts mit gleichem Netznamen, oder -1.
  static int pickCandidate(const uint8_t currentBssid[6], int32_t currentRssi,
                           const WifiRoamingCandidate* list, int count) {
    int best = -1;
    for (int i = 0; i < count; i++) {
      if (!list[i].sameSsid) continue;
      if (memcmp(list[i].bssid, currentBssid, 6) == 0) continue;
      if (list[i].rssi < currentRssi + MIN_GAIN_DB) continue;
      if (best < 0 || list[i].rssi > list[best].rssi) best = i;
    }
    return best;
  }
};
