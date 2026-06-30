#include "globals.h"

extern int
    g_anGamestate0dBlock[]; /* 0x80078d78 — gamestate-0xD context block */
extern int D_80078D88;
extern int MemCardSync(int mode, int *result, int *extra);

/* On START (pad bit 0x10): arm the story-intro flow — set the gamestate-0xD
   mode (block field 0) to 1, clear its scratch flag (D_80078D88) and the intro
   substate, then kick a MemCardSync poll whose result/extra slots sit at
   +0x40/+0x44 in the same block. All three block accesses share one base
   register. (0x80032ab0, 88 bytes.) */
void func_80032AB0(void) {
  if (g_dwPadPressed & 0x10) {
    g_anGamestate0dBlock[0] = 1;
    D_80078D88 = 0;
    g_nStoryIntroSubstate = 0;
    MemCardSync(0, &g_anGamestate0dBlock[0x10], &g_anGamestate0dBlock[0x11]);
  }
}
