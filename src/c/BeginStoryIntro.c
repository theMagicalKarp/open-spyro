#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Enter the story-intro gamestate (0x8002d170, gamestate 0xD): zero the
   0x5C-byte mode block, prime the SPU common attr (mask 0xC0, master volume
   0x5000 into +0x10/+0x12) alongside the 0x3FFF/0x5000 volume shadow globals,
   drop SFX/music volume to 10, park the gem-cutscene scratch pointer 0x2000
   below the render scratch region, and reseed the RNG. The 0xC0 store and the
   SetSpuCommonAttr argument address the attr block 0x320 below
   g_nSfxMixVolume_blk, which gcc holds as the one materialized base across the
   call. */
extern int g_nSfxMixVolume_blk[];

void BeginStoryIntro(void) {
  int *blk;
  int new_var;
  int vol;
  g_nGamestate = 0xD;
  FillWord(&g_nGamestate0dMode, 0, 0x5C);
  blk = g_nSfxMixVolume_blk;
  blk[0] = 0x3FFF;
  vol = 0x5000;
  g_nCdAudioVolume_blk = vol;
  g_nSfxVolume = 0xA;
  *((short *)(&g_abSpuCommonAttr[0x10])) = vol;
  *((short *)(&g_abSpuCommonAttr[0x12])) = vol;
  new_var = 200;
  g_nMusicVolume = 0xA;
  blk[-new_var] = (short)0xC0;
  g_pStoryIntroStreamCursor = ((char *)g_pRenderScratchRegionBase) - 0x2000;
  SetSpuCommonAttr(blk - 200);
  g_nLevelStormFlag = 0;
  *((int *)g_pPathTableHead) = 0;
  srand(0x159);
}
