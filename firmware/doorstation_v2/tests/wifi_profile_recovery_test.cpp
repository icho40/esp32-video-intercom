#include "../WifiProfileRecovery.h"
#include <assert.h>
int main() {
  WifiProfileRecovery r;
  auto a = r.update(0, false, false); assert(!a.tryOther && !a.openSetup);
  a = r.update(29999, false, false); assert(!a.tryOther && !a.openSetup);
  a = r.update(30000, false, false); assert(a.tryOther && !a.openSetup);
  a = r.update(59999, false, false); assert(!a.tryOther && !a.openSetup);
  a = r.update(60000, false, false); assert(a.tryOther && a.openSetup);
  a = r.update(61000, true, false); assert(!a.tryOther && !a.openSetup);
  a = r.update(62000, false, false); assert(!a.tryOther && !a.openSetup);
  a = r.update(92000, false, true); assert(!a.tryOther && !a.openSetup);
  a = r.update(93000, false, false); assert(!a.tryOther && !a.openSetup);
  a = r.update(123000, false, false); assert(a.tryOther && !a.openSetup);
  WifiProfileRecovery wrap;
  wrap.update(UINT32_MAX-9999, false, false);
  a = wrap.update(19999, false, false); assert(!a.tryOther);
  a = wrap.update(20000, false, false); assert(a.tryOther && !a.openSetup);
  a = wrap.update(50000, false, false); assert(a.tryOther && a.openSetup);
}
