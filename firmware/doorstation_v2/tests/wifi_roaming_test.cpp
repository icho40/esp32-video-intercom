#include "../WifiRoaming.h"
#include <assert.h>
#include <stdint.h>

static WifiRoamingCandidate ap(uint8_t id, int32_t rssi, bool same = true) {
  WifiRoamingCandidate c{{0, 0, 0, 0, 0, id}, rssi, 1, same};
  return c;
}

int main() {
  // shouldScan: erst nach Einschwingzeit, nur im Ruhezustand, nur bei schwachem Signal.
  WifiRoamingPolicy p;
  assert(!p.shouldScan(0, false, true, -80));
  assert(!p.shouldScan(1000, true, true, -80));          // gerade verbunden
  assert(!p.shouldScan(30999, true, true, -80));
  assert(!p.shouldScan(31000, true, false, -80));        // nicht IDLE
  assert(!p.shouldScan(31000, true, true, -70));         // Signal ausreichend
  assert(p.shouldScan(31000, true, true, -71));
  assert(!p.shouldScan(150999, true, true, -80));        // Intervall laeuft
  assert(p.shouldScan(151000, true, true, -80));
  assert(!p.shouldScan(152000, false, true, -80));       // Verbindung weg
  assert(!p.shouldScan(153000, true, true, -80));        // neu verbunden: wieder warten
  assert(!p.shouldScan(182999, true, true, -80));
  assert(!p.shouldScan(183000, true, true, -80));        // Intervall seit letzter Suche gilt weiter
  assert(p.shouldScan(271000, true, true, -80));

  // millis()-Ueberlauf
  WifiRoamingPolicy w;
  assert(!w.shouldScan(UINT32_MAX - 9999, true, true, -80));
  assert(w.shouldScan(20000, true, true, -80));

  // pickCandidate
  const uint8_t cur[6] = {0, 0, 0, 0, 0, 1};
  WifiRoamingCandidate list[] = {ap(1, -50), ap(2, -71), ap(3, -69), ap(4, -40, false), ap(5, -60)};
  // Index 0: aktueller AP, 1: nur 9 dB besser, 3: anderes WLAN, 4 ist besser als 2.
  assert(WifiRoamingPolicy::pickCandidate(cur, -80, list, 5) == 4);
  assert(WifiRoamingPolicy::pickCandidate(cur, -59, list, 3) == -1);
  assert(WifiRoamingPolicy::pickCandidate(cur, -80, list, 0) == -1);
  return 0;
}
