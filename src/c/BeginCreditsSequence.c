#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Enter the credits gamestate (0x8002d228, gamestate 0xF): clear the whole
   VRAM framebuffer, silence all sound, optionally reload the actor mesh tables
   (param nonzero), reset Spyro, zero the credits/returning-home phase globals
   (g_nCreditsPageIndex only when it is not the level-select magic 0xA), then
   kick off the synchronous 0x258-sector credits asset read from the CD base
   LBA. */

void BeginCreditsSequence(int reloadMeshes) {
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
  if (reloadMeshes != 0) {
    LoadActorMeshTables();
  }
  ResetSpyroEntity(1);
  g_nGamestate = GS_CREDITS;
  g_nCreditsMode = -1;
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
    int size = *(volatile int *)&g_nCreditsSceneByteSize;
    int offset = *(volatile int *)&g_nCreditsSceneSectorOffset;
    int *dst = *(int *volatile *)&g_pSharedLoadBuffer;
    CdReadSyncSectors(lba, dst, size, offset, 0x258);
  }
}
