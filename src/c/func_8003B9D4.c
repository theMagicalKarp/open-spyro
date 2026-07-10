#include "globals.h"

extern void func_800529E4(unsigned char *actor, int n);
extern void func_8003B854(int id, unsigned char *actor);

extern unsigned char *D_80075758;
extern int D_8006E330[];

/* 0x8003b9d4 (0xFC) — on picking up actor `a` (a fodder/pickup): record its
   kind byte in the gem-tally icon UV ring, bump the ring count, special-case
   the egg-thief actor (D_80075758 -> 3 eggs), notify the pickup handler, spawn
   the level's particle burst for the kind, then queue the matching sound
   effect for kinds 0x53..0x57. */
void func_8003B9D4(unsigned char *a) {
  short kind;
  int pad[4];

  g_abGemTallyIconUvTable[g_nGemTallyIconCount & 0x1F] =
      *(unsigned short *)(a + 0x36);
  g_nGemTallyIconCount += 1;
  if (a == D_80075758) {
    g_nLevelEggCount = 3;
  }
  func_800529E4(a, 4);
  ((void (*)(int, int, unsigned char *, int))g_pfnLevelOverlayParticleSpawn)(
      6, 0xC, a, D_8006E330[*(short *)(a + 0x36)]);

  kind = *(short *)(a + 0x36);
  if (kind == 0x53) {
    func_8003B854(1, a);
  } else if (kind == 0x54) {
    func_8003B854(2, a);
  } else if (kind == 0x55) {
    func_8003B854(5, a);
  } else if (kind == 0x56) {
    func_8003B854(0xA, a);
  } else if (kind == 0x57) {
    func_8003B854(0x19, a);
  }
}
