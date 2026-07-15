#include "globals.h"

extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void CopyWords(void *dst, void *src, int byte_count);
extern int ComputeSaveGameChecksum(unsigned char *data);

extern int D_80075838;
extern int D_8007583C;

/* 0x80059864 (484 bytes) — memcard save-block serializer: packs the current
   game state into a 0x600-byte buffer (options bytes, per-world tables,
   per-level visited/gem/dragon/egg tallies, kill bitmaps) and stamps the
   checksum at +0x58C. Sole caller Gamestate0B_Update (save-write). */
void BuildSaveGameBuffer(unsigned char *buf) {
  unsigned char *start;
  int w;
  int i;
  unsigned char *tbl;
  unsigned char *row;
  unsigned char *dst;
  unsigned char *src;
  unsigned char *end;
  int *eggs;
  int *gems;
  unsigned char *half;
  int *dragons;
  unsigned char *row2;
  int v;

  FillWord(buf, 0, 0x600);
  buf[0] = g_nActiveLevelId;
  buf[1] = g_nSfxVolume;
  buf[2] = g_nMusicVolume;
  buf[3] = g_nOptionVibrationEnabled;
  *(unsigned int *)(buf + 4) = g_dwActiveCameraOptions;
  start = buf;
  buf[8] = g_nSoundMonoMixFlag;
  buf[9] = D_80075838;
  buf[0xA] = D_8007583C;
  buf[0xB] = g_nExtraLives;
  CopyWords(buf + 0x10, g_anWorldPersistentFlags, 0x14);
  w = 0;
  tbl = g_abWorldPerLevelByteTable;
  row = buf;
  do {
    dst = row + 0x24;
    src = tbl;
    end = row + 0x29;
    do {
      *dst++ = *src++;
    } while ((int)dst < (int)end);
    tbl += 5;
    w += 1;
    row += 5;
  } while (w < 5);
  CopyWords(buf + 0x40, g_abLevelVisitedFlag, 0x24);
  CopyWords(buf + 0x64, g_abLevelGemPercent, 0x24);
  i = 0;
  eggs = g_anLevelEggsRecovered;
  gems = g_anLevelGemsCollected;
  half = buf;
  dragons = g_anLevelDragonsRescued;
  do {
    v = *dragons++;
    row2 = buf + i;
    i += 1;
    row2[0x88] = v;
    *(short *)(half + 0xAC) = *gems++;
    row2[0xF4] = *eggs++;
    half += 2;
  } while (i < 0x24);
  i = 0;
  do {
    *(i + buf + 0x106) = g_abSaveOptionsBlock[i];
    i += 1;
  } while (i < 6);
  CopyWords(buf + 0x10C, g_anLevelKillBitmapTable, 0x480);
  *(unsigned int *)(buf + 0x58C) = ComputeSaveGameChecksum(start);
}
