#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "save_game.h"

/* 0x80059864 (484 bytes) — memcard save-block serializer: packs the current
   game state into a 0x600-byte buffer (options bytes, per-world tables,
   per-level visited/gem/dragon/egg tallies, kill bitmaps) and stamps the
   checksum (SaveGameBuffer, save_game.h). Sole caller Gamestate0B_Update
   (save-write). */
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
  buf[SAVE_OFFSET(levelId)] = g_nActiveLevelId;
  buf[SAVE_OFFSET(sfxVolume)] = g_nSfxVolume;
  buf[SAVE_OFFSET(musicVolume)] = g_nMusicVolume;
  buf[SAVE_OFFSET(vibration)] = g_nOptionVibrationEnabled;
  SAVE_RAW(buf, unsigned int, cameraOptions) = g_dwActiveCameraOptions;
  start = buf;
  buf[SAVE_OFFSET(monoMix)] = g_nSoundMonoMixFlag;
  buf[SAVE_OFFSET(escapeRunCount)] = g_anLevelEscapeRunCount;
  buf[SAVE_OFFSET(escapeRunCountLane1)] = g_nLevelEscapeRunCountLane1;
  buf[SAVE_OFFSET(extraLives)] = g_nExtraLives;
  CopyWords(buf + SAVE_OFFSET(worlds), g_anWorldPersistentFlags, 0x14);
  w = 0;
  tbl = g_abWorldPerLevelByteTable;
  row = buf;
  do {
    dst = row + SAVE_OFFSET(worlds.worldLevelBytes);
    src = tbl;
    end = row + SAVE_OFFSET(worlds.worldLevelBytes[1]);
    do {
      *dst++ = *src++;
    } while ((int)dst < (int)end);
    tbl += 5;
    w += 1;
    row += 5;
  } while (w < 5);
  CopyWords(buf + SAVE_OFFSET(levelVisited), g_abLevelVisitedFlag, 0x24);
  CopyWords(buf + SAVE_OFFSET(levelExited), g_abLevelExited, 0x24);
  i = 0;
  eggs = g_anLevelEggsRecovered;
  gems = g_anLevelGemsCollected;
  half = buf;
  dragons = g_anLevelDragonsRescued;
  do {
    v = *dragons++;
    row2 = buf + i;
    i += 1;
    row2[SAVE_OFFSET(levelDragons)] = v;
    SAVE_RAW(half, short, levelGems) = *gems++;
    row2[SAVE_OFFSET(levelEggs)] = *eggs++;
    half += 2;
  } while (i < 0x24);
  i = 0;
  do {
    *(i + buf + SAVE_OFFSET(balloonistState)) = g_abWorldBalloonistState[i];
    i += 1;
  } while (i < 6);
  CopyWords(buf + SAVE_OFFSET(killBitmaps), g_anLevelKillBitmapTable, 0x480);
  SAVE_RAW(buf, unsigned int, checksum) = ComputeSaveGameChecksum(start);
}
