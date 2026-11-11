#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "save_game.h"

/* 0x80059864 (484 bytes) — memcard save-block serializer: packs the current
   game state into a SAVE_IMAGE_BYTES buffer (options bytes, per-world tables,
   per-level visited/gem/dragon/egg tallies, kill bitmaps) and stamps the
   checksum (SaveGameBuffer, save_game.h). Callers: SaveMenu_Update
   (save-write) and Titlescreen_Update (fresh saves on a new card). */
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

  FillWord(buf, 0, SAVE_IMAGE_BYTES);
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
  CopyWords(buf + SAVE_OFFSET(flight), g_anFlightClearTimes,
            FLIGHT_LEVEL_COUNT * 4);
  w = 0;
  tbl = g_abFlightBonusPaid;
  row = buf;
  do {
    dst = row + SAVE_OFFSET(flight.bonusPaid);
    src = tbl;
    end = row + SAVE_OFFSET(flight.bonusPaid[1]);
    do {
      *dst++ = *src++;
    } while ((int)dst < (int)end);
    tbl += FLIGHT_BONUS_SLOTS;
    w += 1;
    row += FLIGHT_BONUS_SLOTS;
  } while (w < FLIGHT_LEVEL_COUNT);
  CopyWords(buf + SAVE_OFFSET(levelVisited), g_abLevelVisitedFlag,
            TOTAL_LEVEL_COUNT);
  CopyWords(buf + SAVE_OFFSET(levelExited), g_abLevelExited, TOTAL_LEVEL_COUNT);
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
  } while (i < TOTAL_LEVEL_COUNT);
  i = 0;
  do {
    *(i + buf + SAVE_OFFSET(balloonistState)) = g_abWorldBalloonistState[i];
    i += 1;
  } while (i < WORLD_COUNT);
  CopyWords(buf + SAVE_OFFSET(killBitmaps), g_anLevelKillBitmapTable,
            TOTAL_LEVEL_COUNT * 32);
  ((SaveGameBuffer *)buf)->checksum = ComputeSaveGameChecksum(start);
}
