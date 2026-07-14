#include "globals.h"
#include "types.h"

extern void StopAllSoundExceptMask(int mask);
extern void TickSpuPerFrame(void);
extern void ClearImage(RECT *rect, int r, int g, int b);
extern void DrawSync(int mode);
extern void InitActorMeshScratchRegions(int mode);
extern void TickWorldBundleLoadStream(void);
extern void TickCdMusicStream(void);
extern void BeginGemCutscene(void);
extern void AdvanceGemCutsceneFrame(int timer);
extern void DispatchSpecialCutscene(void);

extern void *D_800113A0;

/* 0x800331ac — GS_GEM_CUTSCENE (0xE) per-tick handler: on first entry (timer
   0, hub world) clear the screen, stream the cutscene bundle in synchronously
   and re-seed via BeginGemCutscene; per tick advance the cutscene timer toward
   twice the threshold, fade g_nGenericCountdown at both ends, allow a pad
   press to skip ahead, then hand off to DispatchSpecialCutscene at the end. */
void GemCutscene_Update(void) {
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
    g_pDrawBufA = D_800113A0;
    g_pDrawBufB = D_800113A0;
    if (g_nCurrentWorldId == 3) {
      g_pWorkAreaTop = (void *)(0x80200000 - g_nCrt0StackSize);
      InitActorMeshScratchRegions(1);
    }
    while (g_nCdStreamState < 0xA) {
      TickWorldBundleLoadStream();
      TickCdMusicStream();
    }
    BeginGemCutscene();
  }

  path = (int *)g_pPathTableHead;
  t = path[0] + g_nFrameStep;
  end = path[2] << 1;
  path[0] = t;
  if (t < end) {
    if (t < 0x10 || (t = end - t, t < 0x10)) {
      g_nGenericCountdown = 0x10 - t;
    } else {
      g_nGenericCountdown = 0;
    }
    if (g_nGenericCountdown >= 0x10) {
      g_nGenericCountdown = 0xF;
    }
    if (g_nCurrentWorldId == 1 && (g_dwPadHeld & 0x840)) {
      p = (int *)g_pPathTableHead;
      cur = p[0];
      if (cur >= 0xF1 && cur < (p[2] << 1) - 0x20) {
        p[2] = (cur >> 1) + 0x10;
      }
    }
    AdvanceGemCutsceneFrame(*(int *)g_pPathTableHead);
  } else {
    DispatchSpecialCutscene();
  }
}
