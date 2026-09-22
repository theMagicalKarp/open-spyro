#include "globals.h"

extern void CopyWords(void *dst, void *src, int byte_count);
extern void SetSpuCommonAttr(unsigned char *attr);
extern int ComputeSaveGameChecksum(unsigned char *data);

extern int D_80075838;
extern int D_8007583C;
extern short D_80075F18;
extern short D_80075F1A;
extern int D_80076224;
extern int D_80076228;
extern int D_80076228_blk[];

/* 0x80059594 (720 bytes) — memcard save-block deserializer: the inverse of
   BuildSaveGameBuffer.  Unpacks a 0x600-byte save buffer back into live game
   state (options bytes, scaled SPU/CD volumes, per-world tables, per-level
   tallies with recomputed world totals, kill bitmaps) and returns whether the
   stored checksum at +0x58C matches.

   The SpuCommonAttr block at -0x320 is reached through the D_80076228_blk
   incomplete-array alias (A20).  In the tally loop, the egg-guard compare `c`
   and the treasure sum `t` are computed ABOVE the empty do/while: the barrier's
   loop notes land on the `*gems` store, and sched1 then makes every later insn
   depend on it (B23), so anything that must be emitted before that store has
   to be computed before the barrier. */
int func_80059594(unsigned char *buf) {
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
  g_nActiveLevelId = buf[0];
  sfx = buf[1];
  sm = (sfx * 0x3FFF) / 10;
  D_80076228_blk[0] = sm;
  g_nSfxVolume = sfx;
  g_nSoundMasterVolume = (sm << 0xC) / 16383;
  mv = buf[2];
  common = ((unsigned char *)D_80076228_blk) - 0x320;
  *((int *)common) = 0xC0;
  g_nMusicVolume = mv;
  D_80076224 = mv << 0xB;
  D_80075F18 = mv << 0xB;
  D_80075F1A = mv << 0xB;
  SetSpuCommonAttr(common);
  cam = *((unsigned int *)(buf + 4));
  g_nOptionVibrationEnabled = buf[3];
  g_dwActiveCameraOptions = cam;
  g_nSoundMonoMixFlag = buf[8];
  D_80075838 = buf[9];
  D_8007583C = buf[0xA];
  g_nExtraLives = buf[0xB];
  lives = g_nExtraLives;
  start = buf;
  if (lives < 4) {
    g_nExtraLives = 4;
  }
  CopyWords(g_anWorldPersistentFlags, buf + 0x10, 0x14);
  off = 0x64;
  w = 0;
  row = buf;
  tbl = g_abWorldPerLevelByteTable;
  do {
    dst = tbl;
    src = row + 0x24;
    end = tbl + 5;
    do {
      *(dst++) = *(src++);
    } while (((int)dst) < ((int)end));
    row += 5;
    w += 1;
    tbl += 5;
  } while (w < 5);
  CopyWords(g_abLevelVisitedFlag, buf + 0x40, 0x24);
  CopyWords(g_abLevelGemPercent, buf + off, 0x24);
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
    d = row2[0x88];
    wd = g_nWorldDragonsRescued;
    wt = g_nWorldTreasureCollected;
    *dragons = d;
    g = *((short *)(half + 0xAC));
    g_nWorldDragonsRescued = wd + d;
    c = i < 0x12;
    t = wt + g;
    do {
    } while (0);
    *gems = g;
    g_nWorldTreasureCollected = t;
    if (c) {
      e = row2[0xF4];
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
    g_abSaveOptionsBlock[i] = *((buf + i) + 0x106);
    i += 1;
  } while (i < 6);
  CopyWords(g_anLevelKillBitmapTable, buf + 0x10C, 0x480);
  return (*((int *)(buf + 0x58C))) == ComputeSaveGameChecksum(start);
}
