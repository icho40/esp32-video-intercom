#include "../BootWifiSelection.h"
#include <assert.h>
int main() {
  BootWifiSelection idle(100);
  assert(idle.update(20099,false,false)==0);
  assert(idle.update(20100,false,false)==3);
  assert(idle.update(20101,true,false)==3);
  BootWifiSelection p1(0);
  assert(p1.update(100,true,false)==0);
  assert(p1.update(139,true,false)==0);
  assert(p1.update(140,true,false)==1);
  assert(p1.update(150,false,true)==1);
  BootWifiSelection p2(0);
  assert(p2.update(1,false,true)==0);
  assert(p2.update(41,false,true)==2);
  BootWifiSelection bounce(0);
  assert(bounce.update(10,true,false)==0);
  assert(bounce.update(20,false,false)==0);
  assert(bounce.update(30,true,true)==0);
  assert(bounce.update(100,true,true)==0);
  assert(bounce.update(200,false,true)==0);
  assert(bounce.update(240,false,true)==2);
  BootWifiSelection wrap(0xfffffff0u);
  assert(wrap.update(0x10u,false,false)==0);
  assert(wrap.update(0x4e10u,false,false)==3);
  BootWifiSelection edge(0);
  assert(edge.update(19990,true,false)==0);
  assert(edge.update(20000,true,false)==3);
}
