#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "types.h"

/* 0x800331ac — GS_CUTSCENE (0xE) per-tick handler: on first entry (timer
   0, hub world) clear the screen, stream the cutscene bundle in synchronously
   and re-seed via BeginCutscene; per tick advance the cutscene timer toward
   twice the threshold, fade g_nScreenFadeLevel at both ends, allow a pad
   press to skip ahead, then hand off to DispatchSpecialCutscene at the end. */
void Cutscene_Update(void) {
  RECT rect;
  int *path;
  int t;
  int end;
  int *p;
  int cur;

  if (g_nCurrentWorldId >= 2 && *(int *)g_pPathTableHead == 0) {
    StopAllSoundExceptMask(0);
    TickSpuPerFrame();
    rect.w = 0x200;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    g_nCdStreamState = 0;
    g_pDrawBufA = g_pSharedLoadBuffer;
    g_pDrawBufB = g_pSharedLoadBuffer;
    if (g_nCurrentWorldId == 3) {
      g_pWorkAreaTop = (void *)(0x80200000 - g_nCrt0StackSize);
      InitActorMeshScratchRegions(1);
    }
    while (g_nCdStreamState < 0xA) {
      TickWorldBundleLoadStream();
      TickCdMusicStream();
    }
    BeginCutscene();
  }

  path = (int *)g_pPathTableHead;
  t = path[0] + g_nFrameStep;
  end = path[2] << 1;
  path[0] = t;
  if (t < end) {
    if (t < 0x10 || (t = end - t, t < 0x10)) {
      g_nScreenFadeLevel = 0x10 - t;
    } else {
      g_nScreenFadeLevel = 0;
    }
    if (g_nScreenFadeLevel >= 0x10) {
      g_nScreenFadeLevel = 0xF;
    }
    if (g_nCurrentWorldId == 1 && (g_dwPadHeld & (PAD_CROSS | PAD_START))) {
      p = (int *)g_pPathTableHead;
      cur = p[0];
      if (cur >= 0xF1 && cur < (p[2] << 1) - 0x20) {
        p[2] = (cur >> 1) + 0x10;
      }
    }
    AdvanceCutsceneFrame(*(int *)g_pPathTableHead);
  } else {
    DispatchSpecialCutscene();
  }
}
