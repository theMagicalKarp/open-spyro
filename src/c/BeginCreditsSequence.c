#include "globals.h"
#include "main_shared.h"

/* Enter the credits gamestate (0x8002d228, gamestate 0xF): clear the whole
   VRAM framebuffer, silence all sound, optionally reload the actor mesh tables
   (param nonzero), reset Spyro, zero the credits/returning-home phase globals
   (g_nCreditsPageIndex only when it is not the level-select magic 0xA), then
   kick off the synchronous 0x258-sector credits asset read from the CD base
   LBA. */
extern void ClearImage(RECT *rect, unsigned char r, unsigned char g,
                       unsigned char b);
extern int DrawSync(int mode);
extern void StopAllSoundExceptMask(unsigned int mask);
extern void ResetSpyroEntity(int param);
extern int CdReadSyncSectors();
extern int g_nCreditsPageIndex;
extern int g_nCreditsFirstVisibleLine;
extern int g_nCreditsLastVisibleLine;
extern int g_nCreditsLineCount;
extern int g_pCreditsScrollHeader;
extern int g_pCreditsScratchBuf;
extern int *g_pSharedLoadBuffer; /* shared CD sector buffer pointer */
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
  g_nCreditsFirstVisibleLine = 0;
  g_nCreditsLastVisibleLine = 0;
  g_nCreditsLineCount = 0;
  g_pCreditsScrollHeader = 0;
  g_pCreditsScratchBuf = 0;
  g_nReturningHomePhase = 0;
  if (g_nCreditsPageIndex != 0xA) {
    g_nCreditsPageIndex = 0;
  }
  {
    int lba = *(volatile int *)&g_nCdBaseLba;
    int size = *(volatile int *)&D_8007A95C;
    int offset = *(volatile int *)&D_8007A958;
    int *dst = *(int *volatile *)&g_pSharedLoadBuffer;
    CdReadSyncSectors(lba, dst, size, offset, 0x258);
  }
}
