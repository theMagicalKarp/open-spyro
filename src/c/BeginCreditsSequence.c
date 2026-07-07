#include "globals.h"

/* Enter the credits gamestate (0x8002d228, gamestate 0xF): clear the whole
   VRAM framebuffer, silence all sound, optionally reload the actor mesh tables
   (param nonzero), reset Spyro, zero the credits/returning-home phase globals
   (D_800757AC only when it is not the level-select magic 0xA), then kick off
   the synchronous 0x258-sector credits asset read from the CD base LBA. */
extern void ClearImage(RECT *rect, unsigned char r, unsigned char g,
                       unsigned char b);
extern int DrawSync(int mode);
extern void StopAllSoundExceptMask(unsigned int mask);
extern void TickSpuPerFrame(void);
extern void LoadActorMeshTables(void);
extern void ResetSpyroEntity(int param);
extern int CdReadSyncSectors();
extern int D_800757AC;
extern int D_80075968;
extern int D_80075878;
extern int D_800757DC;
extern int D_800757BC;
extern int D_80075768;
extern int *D_800113A0; /* shared CD sector buffer pointer */
extern int D_8007A95C;
extern int D_8007A958;

void BeginCreditsSequence(int param_1) {
  int unused[8];
  RECT rect;
  rect.w = 0x200;
  rect.x = 0;
  rect.y = 0;
  rect.h = 0x1E0;
  ClearImage(&rect, 0, 0, 0);
  DrawSync(0);
  StopAllSoundExceptMask(0);
  TickSpuPerFrame();
  if (param_1 != 0) {
    LoadActorMeshTables();
  }
  ResetSpyroEntity(1);
  g_nGamestate = 0xF;
  g_nGamestate0fMode = -1;
  D_80075968 = 0;
  D_80075878 = 0;
  D_800757DC = 0;
  D_800757BC = 0;
  D_80075768 = 0;
  g_nReturningHomePhase = 0;
  if (D_800757AC != 0xA) {
    D_800757AC = 0;
  }
  {
    int lba = *(volatile int *)&g_nCdBaseLba;
    int arg2 = *(volatile int *)&D_8007A95C;
    int arg3 = *(volatile int *)&D_8007A958;
    int *dst = *(int *volatile *)&D_800113A0;
    CdReadSyncSectors(lba, dst, arg2, arg3, 0x258);
  }
}
