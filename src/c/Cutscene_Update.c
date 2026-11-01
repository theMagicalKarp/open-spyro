#include "cd_stream.h"
#include "cutscene_id.h"
#include "cutscene_path.h"
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
  CutscenePath *path;
  int t;
  int end;
  CutscenePath *p;
  int cur;

  if (g_nCutsceneId >= 2 && g_pPathTableHead->frame == 0) {
    StopAllSoundExceptMask(0);
    TickSpuPerFrame();
    rect.w = 0x200;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    g_nCdStreamState = CSTREAM_READ_HEADER;
    g_pDrawBufA = g_pSharedLoadBuffer;
    g_pDrawBufB = g_pSharedLoadBuffer;
    if (g_nCutsceneId == CUTSCENE_LOOT_ENDING) {
      g_pWorkAreaTop = (void *)(0x80200000 - g_nCrt0StackSize);
      InitActorMeshScratchRegions(1);
    }
    while (g_nCdStreamState < CSTREAM_DONE) {
      TickCutsceneBundleLoadStream();
      TickCdMusicStream();
    }
    BeginCutscene();
  }

  path = g_pPathTableHead;
  t = path->frame + g_nFrameStep;
  end = path->length << 1;
  path->frame = t;
  if (t < end) {
    if (t < 0x10 || (t = end - t, t < 0x10)) {
      g_nScreenFadeLevel = 0x10 - t;
    } else {
      g_nScreenFadeLevel = 0;
    }
    if (g_nScreenFadeLevel >= 0x10) {
      g_nScreenFadeLevel = 0xF;
    }
    if (g_nCutsceneId == CUTSCENE_OPENING &&
        (g_dwPadHeld & (PAD_CROSS | PAD_START))) {
      p = g_pPathTableHead;
      cur = p->frame;
      if (cur >= 0xF1 && cur < (p->length << 1) - 0x20) {
        p->length = (cur >> 1) + 0x10;
      }
    }
    AdvanceCutsceneFrame(g_pPathTableHead->frame);
  } else {
    DispatchSpecialCutscene();
  }
}
