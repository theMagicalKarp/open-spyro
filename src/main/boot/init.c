#include "actor_cutscene.h"
#include "camera.h"
#include "cd_music.h"
#include "cd_stream.h"
#include "cutscene_id.h"
#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"
#include "level_id.h"
#include "libcd.h"
#include "main_shared.h"
#include "spu.h"

/* 0x800122a8 — one-time GPU bring-up: reset the GPU, build both frames'
   draw/disp environments (double-buffered halves of the 512x512 VRAM
   page), set the background-clear/dither flags, clear VRAM, and unmask
   the display. */
void InitGraphicsHardware(void) {
  RECT rect;
  FrameDB *base;

  SetDispMask(0);
  ResetGraph(0);
  SetGraphDebug(0);

  base = FRAME_DB(&g_FrameDrawEnv0);
  InitDefaultDrawEnv(&base[0].draw, 0, DRAW_Y, SCREEN_W, DRAW_H);
  InitDefaultDrawEnv(&base[1].draw, 0, SCREEN_H + DRAW_Y, SCREEN_W, DRAW_H);
  InitDefaultDispEnv(&base[0].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
  InitDefaultDispEnv(&base[1].disp, 0, 0, SCREEN_W, SCREEN_H);

  g_FrameDrawEnv1.ofs[1] = SCREEN_H;
  g_FrameDrawEnv0.isbg = 1;
  g_FrameDrawEnv1.isbg = 1;
  g_FrameDrawEnv0.dtd = 1;
  g_FrameDrawEnv1.dtd = 1;
  g_FrameDrawEnv0.ofs[1] = 0;
  rect.x = 0;
  rect.y = 0;
  rect.w = 0x200;
  rect.h = 0x200;
  ClearImage(&rect, 0, 0, 0);
  DrawSync(0);
  SetDispMask(1);
}

/* Bring up the controller subsystem at boot: init the BIOS pad driver with the
   two raw report buffers, seed both pads' stick calibration, clear the rumble
   state, then start pad comms, do a first poll, and install the per-VSync poll
   callback. (0x800123c8, 152 bytes.) */
void InitPadSystem(void) {
  unsigned int *pad = &g_dwPadStash;
  PadInitDirect(g_abPadRawReport, g_abPad2RawReport);
  ResetPadStickCalibrationDefaults((unsigned char *)pad);
  FinalizePadCalibration((unsigned char *)pad);
  g_nHitRumbleTimer = 0;
  g_nVibrationLevel = 0;
  g_nPulseRumbleTimer = 0;
  g_nPulseRumbleAmount = 0;
  g_nPadSetMainModePending = 1;
  PadStartCom();
  PollPadAndDistributeInput();
  InstallVSyncCallback((unsigned int)PollPadAndDistributeInput);
}

/* Spyro audio bring-up: resets the memory-card subsystem (mode 0).
   (0x80012460, 32 bytes.) */
void SpuInit(void) { MemCardReset(0); }

/* The CdMix attenuation bytes (g_bCdMixLeftToLeft.., 0x800776d0) reached
   from the music-stream block's held base, as the original addresses them. */
#define CD_MIX_ATV ((unsigned char *)g_anMusicStreamBlock + 0x21C)

/* Bring up the CD subsystem at boot: init the drive, set double-speed read
   mode (CdlSetmode 0x80), install the async read callback, then prime the
   music-stream status word and the CdMix attenuator buffer (0x7F,0,0x7F,0).
   (0x80012480, 140 bytes.) */
void InitCdSystem(void) {
  unsigned char mode;
  mode = CdlModeSpeed;
  CdInit();
  CdControl(CdlSetmode, &mode, 0);
  CdReadCallback(CdReadCompleteCallback);
  g_anMusicStreamBlock[0] = MUSIC_ST_IDLE;
  g_nLastCdMusicCommand = 0;
  g_nPendingMusicCommand = 0;
  g_bCdMixRightToRight = CD_MIX_FULL;
  CD_MIX_ATV[0] = CD_MIX_FULL; /* g_bCdMixLeftToLeft */
  g_bCdMixRightToLeft = 0;
  g_bCdMixLeftToRight = 0;
  CdMix(CD_MIX_ATV);
}

#undef CD_MIX_ATV

/* Boot-time asset directory load: sector 0x25 read synchronously into the
   shared CD buffer, then 0x330 bytes copied out to the asset directory that
   Initialize consults for the follow-up art/world/texture/CLUT loads. The
   LBA global is volatile-reloaded for the call argument, reusing the
   store's base register. (0x8001250c, 96 bytes.) */
void LoadAssetDirectoryFromCd(void) {
  volatile int *lba = g_anCdBaseLbaBlock;

  *lba = 0x25;
  CdReadSyncSectors(*lba, g_pSharedLoadBuffer, 0x800, 0,
                    CD_STALL_TIMEOUT_TICKS);
  CopyWords(&g_anAssetDirectory, g_pSharedLoadBuffer, 0x330);
}

/* One 8-byte entry of g_anMusicTrackLbaTable, addressed by its byte offset
   (the original's $at-indexed stores; an entry-array index changes them). */
typedef struct TrackLba {
  int start;
  int end;
} TrackLba;
#define TRACK_AT(off) ((TrackLba *)((char *)g_anMusicTrackLbaTable + (off)))

/* 0x8001256c, 152 bytes — build the 6x8 music-track LBA table: every track
   entry gets its bank's base LBA and the LBA one track-length later. */
int BuildAudioTrackTable(void) {
  int pad[2];
  int bank;
  int track;
  int idx;
  int *lengths;

  bank = 0;
  lengths = g_anCdAudioTrackLengths;
  for (; bank < 6; bank++) {
    int *len;
    int row;

    for (track = 0, row = bank * 8, len = lengths; track < 8; track++) {
      idx = (row + track) * 8;
      TRACK_AT(idx)->start = g_anCdAudioBankBaseLba[bank];
      TRACK_AT(idx)->end = g_anCdAudioBankBaseLba[bank] + *len;
      len++;
    }
    lengths += 8;
  }

  g_dwMusicStreamStatus = MUSIC_ST_IDLE;
  return 1;
}

#undef TRACK_AT

/* Reset all per-level / per-world progress state for a new game (0x80012604,
   376 bytes). Zeros the per-level gem/dragon/egg tallies and the visited/gem-%
   byte tables, resets the save-options block, clears the per-world persistent
   flags + per-level byte rows, then reinitializes the scalar inventory globals
   and FillWord-clears the spawn-anchor block and the level kill bitmaps. */

void ResetLevelState(void) {
  int i;
  int *eggs;
  int *dragons;
  int *gems;
  int n;
  unsigned char *p;
  unsigned char *row;
  int off;
  int *flags;
  unsigned char *q;
  int *anchor;
  int v;

  i = 0;
  eggs = g_anLevelEggsRecovered;
  dragons = g_anLevelDragonsRescued;
  gems = g_anLevelGemsCollected;
  do {
    *gems = 0;
    *dragons = 0;
    if (i < 0x12) {
      *eggs = 0;
    }
    eggs += 1;
    dragons += 1;
    g_abLevelVisitedFlag[i] = 0;
    g_abLevelExited[i] = 0;
    i += 1;
    gems += 1;
  } while (i < 0x24);

  n = 5;
  p = &g_abWorldBalloonistState[5];
  do {
    *p = 0;
    n -= 1;
    p -= 1;
  } while (n >= 0);
  g_abWorldBalloonistState[0] = BALLOONIST_OPEN;

  row = g_abFlightBonusPaid;
  off = 0;
  flags = g_anFlightClearTimes;
  do {
    *flags = 0;
    n = 4;
    q = row + 4;
    do {
      *q = 0;
      n -= 1;
      q -= 1;
    } while (n >= 0);
    row += 5;
    off += 5;
    flags += 1;
  } while (off < 0x1E);

  anchor = &g_nSpawnAnchorValid;
  v = 0;
  g_nSparxHealth = SPARX_GOLD;
  g_nExtraLives = 4;
  g_nTotalTreasureCollected = 0;
  g_nTotalDragonsRescued = 0;
  g_nTotalEggsRecovered = 0;
  g_nLevelGemsAtEntry = 0;
  g_nGemTallyIconCount = 0;
  g_nLevelKeyState = KEYSTATE_NONE;
  g_nSparxFoodCount = 0;
  g_nSparxHealthStash = -1;
  g_anLevelEscapeRunCount = 0;
  g_nLevelEscapeRunCountLane1 = 0;
  FillWord(anchor, v, 0x68);
  FillWord(g_anLevelKillBitmapTable, 0, 0x480);
}

/* Seed a fresh game: default level, vibration on, default camera options, then
   wipe per-level state. */
void InitNewGameState(void) {
  g_nActiveLevelId = LEVEL_ARTISANS;
  g_nOptionVibrationEnabled = 1;
  g_dwActiveCameraOptions = CAM_OPT_PASSIVE;
  ResetLevelState();
}

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
    rect.h = SCREEN_H;
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
      CdReadSyncSectors(lba1, buf1, CD_SECTOR_BYTES, off1, stallTicks);
    }
    timOff = g_anWorldTimAudioChunkOffsetBlock;
    CopyWords(timOff, *(void *volatile *)&g_pSharedLoadBuffer,
              ARCHIVE_DESC_BYTES);
    top = (char *)0x801C0000 - g_nCrt0StackSize;
    CdReadSyncSectors(lba[0], top, TIM_PAGE_BYTES, timOff[0] + archOff[0],
                      stallTicks);
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
  rect.h = SCREEN_H;
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
  rect.h = SCREEN_H;
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
  rect.h = SCREEN_H;
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
  clear.w = SCREEN_W;
  clear.h = SCREEN_H * 2;
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
