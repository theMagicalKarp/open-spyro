#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "save_game.h"

/* 0x80059594 (720 bytes) — memcard save-block deserializer: the inverse of
   BuildSaveGameBuffer.  Unpacks a 0x600-byte save buffer back into live game
   state (options bytes, scaled SPU/CD volumes, per-world tables, per-level
   tallies with recomputed game-wide totals, kill bitmaps) and returns whether
   the stored checksum matches.

   The SpuCommonAttr block at -0x320 is reached through the g_nSfxMixVolume_blk
   incomplete-array alias (A20).  In the tally loop, the egg-guard compare
   `hasEggs` and the treasure sum `newTotalGems` are computed ABOVE the empty
   do/while: the barrier's loop notes land on the `*gems` store, and sched1 then
   makes every later insn depend on it (B23), so anything that must be emitted
   before that store has to be computed before the barrier. */
int ApplySaveGameBuffer(unsigned char *buf) {
  unsigned char *start;
  int off;
  unsigned char *common;
  int sfx;
  int sfxMix;
  int music;
  int flightLevel;
  int i;
  unsigned char *tbl;
  unsigned char *dst;
  unsigned char *src;
  unsigned char *end;
  int *eggs;
  int *gems;
  unsigned char *gemRow;
  int lives;
  unsigned int cam;
  int *dragons;
  unsigned char *row;
  unsigned char *levelRow;
  int dragonCount;
  int gemCount;
  int eggCount;
  int totalDragons;
  int totalGems;
  int totalEggs;
  int newTotalGems;
  int hasEggs;
  g_nActiveLevelId = buf[SAVE_OFFSET(levelId)];
  sfx = buf[SAVE_OFFSET(sfxVolume)];
  sfxMix = (sfx * 0x3FFF) / 10;
  g_nSfxMixVolume_blk[0] = sfxMix;
  g_nSfxVolume = sfx;
  g_nSoundMasterVolume = (sfxMix << 0xC) / 16383;
  music = buf[SAVE_OFFSET(musicVolume)];
  common = ((unsigned char *)g_nSfxMixVolume_blk) - 0x320;
  *((int *)common) = 0xC0;
  g_nMusicVolume = music;
  g_nCdAudioVolume_blk = music << 0xB;
  g_nCdDaVolumeCurrent = music << 0xB;
  g_nSpuCommonCdVolumeR = music << 0xB;
  SetSpuCommonAttr(common);
  cam = ((SaveGameBuffer *)buf)->cameraOptions;
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
  CopyWords(g_anFlightClearTimes, buf + SAVE_OFFSET(flight), 0x14);
  off = SAVE_OFFSET(levelExited);
  flightLevel = 0;
  row = buf;
  tbl = g_abFlightBonusPaid;
  do {
    dst = tbl;
    src = row + SAVE_OFFSET(flight.bonusPaid);
    end = tbl + 5;
    do {
      *(dst++) = *(src++);
    } while (((int)dst) < ((int)end));
    row += 5;
    flightLevel += 1;
    tbl += 5;
  } while (flightLevel < 5);
  CopyWords(g_abLevelVisitedFlag, buf + SAVE_OFFSET(levelVisited), 0x24);
  CopyWords(g_abLevelExited, buf + off, 0x24);
  i = 0;
  eggs = g_anLevelEggsRecovered;
  gems = g_anLevelGemsCollected;
  gemRow = buf;
  dragons = g_anLevelDragonsRescued;
  g_nTotalDragonsRescued = 0;
  g_nTotalTreasureCollected = 0;
  g_nTotalEggsRecovered = 0;
  do {
    levelRow = buf + i;
    dragonCount = levelRow[SAVE_OFFSET(levelDragons)];
    totalDragons = g_nTotalDragonsRescued;
    totalGems = g_nTotalTreasureCollected;
    *dragons = dragonCount;
    gemCount = SAVE_RAW(gemRow, short, levelGems);
    g_nTotalDragonsRescued = totalDragons + dragonCount;
    hasEggs = i < 0x12;
    newTotalGems = totalGems + gemCount;
    EBB_BREAK();
    *gems = gemCount;
    g_nTotalTreasureCollected = newTotalGems;
    if (hasEggs) {
      eggCount = levelRow[SAVE_OFFSET(levelEggs)];
      totalEggs = g_nTotalEggsRecovered;
      *eggs = eggCount;
      g_nTotalEggsRecovered = totalEggs + eggCount;
    }
    eggs += 1;
    gems += 1;
    gemRow += 2;
    i += 1;
    dragons += 1;
  } while (i < 0x24);
  i = 0;
  do {
    g_abWorldBalloonistState[i] = *((buf + i) + SAVE_OFFSET(balloonistState));
    i += 1;
  } while (i < 6);
  CopyWords(g_anLevelKillBitmapTable, buf + SAVE_OFFSET(killBitmaps), 0x480);
  return ((SaveGameBuffer *)buf)->checksum == ComputeSaveGameChecksum(start);
}
