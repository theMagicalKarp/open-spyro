#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "save_game.h"

/* 0x80059594 (720 bytes) — memcard save-block deserializer: the inverse of
   BuildSaveGameBuffer.  Unpacks a 0x600-byte save buffer back into live game
   state (options bytes, scaled SPU/CD volumes, per-world tables, per-level
   tallies with recomputed world totals, kill bitmaps) and returns whether the
   stored checksum matches.

   The SpuCommonAttr block at -0x320 is reached through the g_nSfxMixVolume_blk
   incomplete-array alias (A20).  In the tally loop, the egg-guard compare `c`
   and the treasure sum `t` are computed ABOVE the empty do/while: the barrier's
   loop notes land on the `*gems` store, and sched1 then makes every later insn
   depend on it (B23), so anything that must be emitted before that store has
   to be computed before the barrier. */
int ApplySaveGameBuffer(unsigned char *buf) {
  unsigned char *start;
  int off;
  unsigned char *common;
  int sfx;
  int sm;
  int mv;
  int w;
  int i;
  unsigned char *tbl;
  unsigned char *dst;
  unsigned char *src;
  unsigned char *end;
  int *eggs;
  int *gems;
  unsigned char *half;
  int lives;
  unsigned int cam;
  int *dragons;
  unsigned char *row;
  unsigned char *row2;
  int d;
  int g;
  int e;
  int wd;
  int wt;
  int we;
  int t;
  int c;
  g_nActiveLevelId = buf[SAVE_OFFSET(levelId)];
  sfx = buf[SAVE_OFFSET(sfxVolume)];
  sm = (sfx * 0x3FFF) / 10;
  g_nSfxMixVolume_blk[0] = sm;
  g_nSfxVolume = sfx;
  g_nSoundMasterVolume = (sm << 0xC) / 16383;
  mv = buf[SAVE_OFFSET(musicVolume)];
  common = ((unsigned char *)g_nSfxMixVolume_blk) - 0x320;
  *((int *)common) = 0xC0;
  g_nMusicVolume = mv;
  g_nCdAudioVolume_blk = mv << 0xB;
  g_nCdDaVolumeCurrent = mv << 0xB;
  g_nSpuCommonCdVolumeR = mv << 0xB;
  SetSpuCommonAttr(common);
  cam = SAVE_RAW(buf, unsigned int, cameraOptions);
  g_nOptionVibrationEnabled = buf[SAVE_OFFSET(vibration)];
  g_dwActiveCameraOptions = cam;
  g_nSoundMonoMixFlag = buf[SAVE_OFFSET(monoMix)];
  g_anLevelEscapeRunCount = buf[SAVE_OFFSET(escapeRunCount)];
  g_nLevelEscapeRunCountLane1 = buf[SAVE_OFFSET(escapeRunCountLane1)];
  g_nExtraLives = buf[SAVE_OFFSET(extraLives)];
  lives = g_nExtraLives;
  start = buf;
  if (lives < 4) {
    g_nExtraLives = 4;
  }
  CopyWords(g_anWorldPersistentFlags, buf + SAVE_OFFSET(worlds), 0x14);
  off = SAVE_OFFSET(levelExited);
  w = 0;
  row = buf;
  tbl = g_abWorldPerLevelByteTable;
  do {
    dst = tbl;
    src = row + SAVE_OFFSET(worlds.worldLevelBytes);
    end = tbl + 5;
    do {
      *(dst++) = *(src++);
    } while (((int)dst) < ((int)end));
    row += 5;
    w += 1;
    tbl += 5;
  } while (w < 5);
  CopyWords(g_abLevelVisitedFlag, buf + SAVE_OFFSET(levelVisited), 0x24);
  CopyWords(g_abLevelExited, buf + off, 0x24);
  i = 0;
  eggs = g_anLevelEggsRecovered;
  gems = g_anLevelGemsCollected;
  half = buf;
  dragons = g_anLevelDragonsRescued;
  g_nWorldDragonsRescued = 0;
  g_nWorldTreasureCollected = 0;
  g_nWorldEggsRecovered = 0;
  do {
    row2 = buf + i;
    d = row2[SAVE_OFFSET(levelDragons)];
    wd = g_nWorldDragonsRescued;
    wt = g_nWorldTreasureCollected;
    *dragons = d;
    g = SAVE_RAW(half, short, levelGems);
    g_nWorldDragonsRescued = wd + d;
    c = i < 0x12;
    t = wt + g;
    EBB_BREAK();
    *gems = g;
    g_nWorldTreasureCollected = t;
    if (c) {
      e = row2[SAVE_OFFSET(levelEggs)];
      we = g_nWorldEggsRecovered;
      *eggs = e;
      g_nWorldEggsRecovered = we + e;
    }
    eggs += 1;
    gems += 1;
    half += 2;
    i += 1;
    dragons += 1;
  } while (i < 0x24);
  i = 0;
  do {
    g_abWorldBalloonistState[i] = *((buf + i) + SAVE_OFFSET(balloonistState));
    i += 1;
  } while (i < 6);
  CopyWords(g_anLevelKillBitmapTable, buf + SAVE_OFFSET(killBitmaps), 0x480);
  return SAVE_RAW(buf, int, checksum) == ComputeSaveGameChecksum(start);
}
