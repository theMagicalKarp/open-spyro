#include "cd_stream.h"
#include "cutscene_id.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Boot-time bring-up + title sequence (0x800127C0, 0x530). Brings up the GPU,
   SPU, pad, sound engine, CD and GTE, points both load buffers past the
   titlescreen overlay, then runs the boot presentation as four 8-frame fades
   driven by AddBiasToColorRuns: the title art fades in and back out around the
   four sync CD loads, then the splash art (loaded into its own buffer 0x5A000
   below the world chunk) fades in while the world bundle streams, each pair
   separated by a 210-vblank hold. Ends by clearing the screen, blitting the
   loaded frame back into VRAM, carving the work-area top out of the crt0 stack
   size and handing off to the story intro.

   Loop forms are NOT uniform and the difference is load-bearing. Fade 1 is a
   real do/while over the direct alias array: loop.c hoists the invariant
   `&g_pLoadStagingBuffer` into the preheader, which is what gives the original
   its held s0 base, and the RECT literals still re-materialize per iteration
   because the inner `if (i == 0)` splits the body. Fades 2/3/4 must be
   goto-loops -- as do/while, loop.c hoists their RECT literals into
   callee-saved registers
   (+4 insns each). Each fade also carries its OWN counter/bias/buffer locals:
   one shared counter lives across the whole body and steals the low
   callee-saved register the buffer base needs.

   The fade-out loops pass `-(j + 1) << 5` and bump the counter at the
   bottom. The arg is then a fresh pseudo, so F14's birthing boost puts its
   addiu after the a0/a1 arg moves, and the bnez slot stays a nop because
   the loop head is `la a0`.

   The CD stall timeout (stallTicks) is pinned in s0, which keeps the CdBaseLba
   base in s1. The first read pins lba1/off1 to a0/a3, which gives the
   original's `lw a0; lw a3` order. The buffer load has to be PLAIN: reorg's
   mark_referenced_resources ASSIGNS `volatil` from the last MEM it skips,
   so a volatile load just before `li s0,600` keeps the li out of
   LoadAssetDirectoryFromCd's delay slot. The third read's two counts are
   volatile locals read before the buffer pointer (orig a2, a3, then a1). */
void Initialize(void) {
  void **buf;
  int i;
  int bias;
  int start;
  void *fill;
  int *lba;
  int *archOff;
  int *timOff;
  char *top;
  char *splashRuns;
  void **buf2;
  int j;
  char *splashFrame;
  int k;
  int bias3;
  int n;
  char *splashFrame2;
  unsigned char *isrgb24;
  RECT rect;
  RECT clear;

  InitGraphicsHardware();
  SpuInit();
  InitPadSystem();
  InitSoundEngine();
  InitCdSystem();
  BuildAudioTrackTable();
  InitGeom();
  SetGeomOffset(0x100, 0x78);
  SetGeomScreen(0x155);
  g_apLoadStagingBufferBlock[0] = &g_abTitlescreenOverlayEnd;
  g_pLevelDataBuffer = &g_abTitlescreenOverlayEnd;
  start = VSync(-1);
  SetDispMask(0);
  fill = g_apLoadStagingBufferBlock[0];
  g_FrameDispEnv0.isrgb24 = 1;
  g_FrameDispEnv1.isrgb24 = 1;
  FillWord(fill, 0, 0x5A000);
  i = 0;
  bias = -0xE0;
  do {
    AddBiasToColorRuns(&g_apMainDrawEntityList, g_apLoadStagingBufferBlock[0],
                       bias);
    rect.w = 0x300;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xF0;
    LoadImage(&rect, g_apLoadStagingBufferBlock[0]);
    DrawSync(0);
    VSync(0);
    PutDispEnv(&g_FrameDispEnv1);
    if (i == 0) {
      SetDispMask(1);
    }
    bias += 0x20;
    i += 1;
  } while (i < 8);

  LoadAssetDirectoryFromCd();
  {
    register int stallTicks asm("$16") = CD_STALL_TIMEOUT_TICKS;
    lba = (int *)g_anCdBaseLbaBlock;
    archOff = g_anWorldArchiveOffsetBlock;
    {
      register int lba1 asm("$4") = lba[0];
      register int off1 asm("$7") = *(volatile int *)archOff;
      void *buf1 = g_pSharedLoadBuffer;
      CdReadSyncSectors(lba1, buf1, 0x800, off1, stallTicks);
    }
    timOff = g_anWorldTimAudioChunkOffsetBlock;
    CopyWords(timOff, *(void *volatile *)&g_pSharedLoadBuffer, 0x1D0);
    top = (char *)0x801C0000 - g_nCrt0StackSize;
    CdReadSyncSectors(lba[0], top, 0x40000, timOff[0] + archOff[0], stallTicks);
    {
      int cnt3 = *(volatile int *)&g_nLevelLoadChunkSize;
      int off3 = *(volatile int *)&g_nLevelLoadByteOffset;
      CdReadSyncSectors(lba[0], *(void *volatile *)&g_pSharedLoadBuffer, cnt3,
                        off3, stallTicks);
    }
    {
      int cnt4 = *(volatile int *)&g_nAssetDirBootBlockSize;
      int off4 = *(volatile int *)&g_anAssetDirectory;
      splashRuns = top - *(volatile int *)&g_nAssetDirBootBlockSize;
      CdReadSyncSectors(lba[0], splashRuns, cnt4, off4, stallTicks);
    }
  }
hold_title:
  if (VSync(-1) - start < 0xD2) {
    VSync(0);
    goto hold_title;
  }
  FillWord(g_apLoadStagingBufferBlock2[0], 0, 0x5A000);
  j = 0;
  buf2 = g_apLoadStagingBufferBlock2;
fade_out_title:
  AddBiasToColorRuns(&g_apMainDrawEntityList, buf2[0], -(j + 1) << 5);
  rect.w = 0x300;
  rect.x = 0;
  rect.y = 0;
  rect.h = 0xF0;
  LoadImage(&rect, buf2[0]);
  DrawSync(0);
  VSync(0);
  PutDispEnv(&g_FrameDispEnv1);
  j += 1;
  if (j < 8) {
    goto fade_out_title;
  }

  start = VSync(-1);
  FillWord(splashRuns - 0x5A000, 0, 0x5A000);
  splashFrame = splashRuns - 0x5A000;
  k = 0;
  bias3 = -0xE0;
fade_in_splash:
  AddBiasToColorRuns(splashRuns, splashFrame, bias3);
  rect.w = 0x300;
  rect.x = 0;
  rect.y = 0;
  rect.h = 0xF0;
  LoadImage(&rect, splashFrame);
  DrawSync(0);
  VSync(0);
  PutDispEnv(&g_FrameDispEnv1);
  bias3 += 0x20;
  k += 1;
  if (k < 8) {
    goto fade_in_splash;
  }

  g_nCdStreamState = CSTREAM_READ_AUDIO;
  g_nCutsceneId = CUTSCENE_TITLE_BACKDROP;
stream_world:
  TickCutsceneBundleLoadStream();
  if (g_nCdStreamState < CSTREAM_DONE) {
    goto stream_world;
  }
hold_splash:
  if (VSync(-1) - start < 0xD2) {
    VSync(0);
    goto hold_splash;
  }
  n = 0;
  splashFrame2 = splashRuns - 0x5A000;
fade_out_splash:
  AddBiasToColorRuns(splashRuns, splashFrame2, -(n + 1) << 5);
  rect.w = 0x300;
  rect.x = 0;
  rect.y = 0;
  rect.h = 0xF0;
  LoadImage(&rect, splashFrame2);
  DrawSync(0);
  VSync(0);
  PutDispEnv(&g_FrameDispEnv1);
  n += 1;
  if (n < 8) {
    goto fade_out_splash;
  }

  SetDispMask(0);
  isrgb24 = &g_FrameDispEnv0.isrgb24;
  *isrgb24 = 0;
  g_FrameDispEnv1.isrgb24 = 0;
  PutDispEnv(
      (DISPENV *)(isrgb24 + 0x73)); /* g_FrameDispEnv1, off the held base */
  clear.x = 0;
  clear.y = 0;
  clear.w = 0x200;
  clear.h = 0x1E0;
  ClearImage(&clear, 0, 0, 0);
  DrawSync(0);
  SetDispMask(1);
  rect.x = 0x200;
  rect.y = 0;
  rect.w = 0x200;
  rect.h = 0x100;
  LoadImage(&rect, top);
  DrawSync(0);
  g_pWorkAreaTop = (char *)RAM_END - g_nCrt0StackSize;
  LoadActorMeshTables();
  BeginCutscene();
  g_nScreenFadeLevel = 0xF;
  BeginStoryIntro();
  g_nFrameStep = 2;
  g_nFrameTicks = 0;
  g_nWorldChunkLodDepth = 0x8000;
}
