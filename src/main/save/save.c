#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "save_game.h"
#include "spu.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Views off the held spawn-yaw base (g_nLevelSpawnYaw). */
#define SPAWN_POS (&g_anLevelSpawnYawBlock[-3]) /* g_anLevelSpawnPos */
#define CHECKPOINT_KILLS                                                       \
  (&g_anLevelSpawnYawBlock[-19])                  /* g_anCheckpointKillBitmap */
#define LIVE_KILLS (&g_anLevelSpawnYawBlock[-11]) /* g_anLiveKillBitmap */

/* 0x80059474 (0xF8) — latch `actor`'s position and facing as the
   level spawn anchor: store the wrapped yaw (<<4 & ANGLE12_MASK) and position,
   mark the anchor valid, bias spawn Z up 0x164, snapshot the level's
   rescued/gem tallies into the persistence block, merge the pending block into
   the committed one (copy 0x20 bytes then OR the 8 words), and carry the spawn
   flag word across. */
/* Two scheduling levers carry this function:
   - the anchor flag's `1` is carried by `t` (the snapshot temp) on purpose: as
     a bare literal its pseudo is single-set, so sched.c birthing_insn_p boosts
     the `li` to max_priority and it lands next to its store; sharing the
     already twice-set `t` makes it non-birthing and it drifts to the block
     front where the original has it (F14 carrier, 4 insns -> 2);
   - the merge source is spelled INLINE at the CopyWords call and the loop
     cursor `src` is assigned AFTER it (cse folds the two into the one
     `addiu s0,s0,-44`, but at the later insn's position), so the `move a0,dst`
     arg copy schedules ahead of it. Assigning `src` before the call — in any
     position, including at the call itself — always emits the addiu first. */

void SetSpawnAnchorFromActor(Actor *actor, int heading) {
  int *rescued;
  int *dst;
  int *src;
  int i;
  int t;
  g_anLevelSpawnYawBlock[0] = (ANGLE8_TO_12(heading)) & ANGLE12_MASK;
  CopyVector(SPAWN_POS, &actor->posX);
  t = 1;
  *((volatile int *)(&g_nSpawnAnchorValid)) = t;
  dst = CHECKPOINT_KILLS;
  *((volatile int *)(&g_nLevelSpawnPosZ)) += SPYRO_RADIUS;
  rescued = &g_anLevelDragonsRescued[g_nLevelIndex];
  {
    t = *rescued;
    *((volatile int *)(&g_nSpawnAnchorDragonsRescued)) = t;
    t = g_anLevelGemsCollected[g_nLevelIndex];
    *(&g_nSpawnAnchorReserved) = 0;
    *((volatile int *)(&g_nSpawnAnchorGemsCollected)) = t;
  }
  CopyWords(dst, LIVE_KILLS, 0x20);
  src = LIVE_KILLS;
  i = 0;
  do {
    int v = *src;
    *dst = (*dst) | v;
    src += 1;
    i += 1;
    dst += 1;
  } while (i < 8);
  g_nLevelProgressCheckpoint = g_nLevelProgress;
}

#undef SPAWN_POS
#undef CHECKPOINT_KILLS
#undef LIVE_KILLS

/* 0x8005956c — the save image checksum: the byte sum of a SaveGameBuffer up
   to (not including) its checksum field. BuildSaveGameBuffer stamps it and the
   save menu / title screen compare it after a read. */
int ComputeSaveGameChecksum(unsigned char *data) {
  int sum = 0;
  unsigned char *end = data + SAVE_OFFSET(checksum);

  do {
    sum += *data;
    data++;
  } while (data < end);
  return sum;
}

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
  sfxMix = (sfx * SPU_VOLUME_MAX) / VOLUME_STEPS;
  g_nSfxMixVolume_blk[0] = sfxMix;
  g_nSfxVolume = sfx;
  g_nSoundMasterVolume = (sfxMix << 0xC) / SPU_VOLUME_MAX;
  music = buf[SAVE_OFFSET(musicVolume)];
  common = ((unsigned char *)g_nSfxMixVolume_blk) - 0x320;
  *((int *)common) = SPU_COMMON_CDVOL;
  g_nMusicVolume = music;
  g_nCdAudioVolume = music << MUSIC_VOLUME_SHIFT;
  g_nCdDaVolumeCurrent = music << MUSIC_VOLUME_SHIFT;
  g_nSpuCommonCdVolumeR = music << MUSIC_VOLUME_SHIFT;
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
  if (lives < SAVE_MIN_LIVES) {
    g_nExtraLives = SAVE_MIN_LIVES;
  }
  CopyWords(g_anFlightClearTimes, buf + SAVE_OFFSET(flight),
            FLIGHT_LEVEL_COUNT * 4);
  off = SAVE_OFFSET(levelExited);
  flightLevel = 0;
  row = buf;
  tbl = g_abFlightBonusPaid;
  do {
    dst = tbl;
    src = row + SAVE_OFFSET(flight.bonusPaid);
    end = tbl + FLIGHT_BONUS_SLOTS;
    do {
      *(dst++) = *(src++);
    } while (((int)dst) < ((int)end));
    row += FLIGHT_BONUS_SLOTS;
    flightLevel += 1;
    tbl += FLIGHT_BONUS_SLOTS;
  } while (flightLevel < FLIGHT_LEVEL_COUNT);
  CopyWords(g_abLevelVisitedFlag, buf + SAVE_OFFSET(levelVisited),
            TOTAL_LEVEL_COUNT);
  CopyWords(g_abLevelExited, buf + off, TOTAL_LEVEL_COUNT);
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
    hasEggs = i < EGG_LEVEL_COUNT;
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
  } while (i < TOTAL_LEVEL_COUNT);
  i = 0;
  do {
    g_abWorldBalloonistState[i] = *((buf + i) + SAVE_OFFSET(balloonistState));
    i += 1;
  } while (i < WORLD_COUNT);
  CopyWords(g_anLevelKillBitmapTable, buf + SAVE_OFFSET(killBitmaps),
            TOTAL_LEVEL_COUNT * 32);
  return ((SaveGameBuffer *)buf)->checksum == ComputeSaveGameChecksum(start);
}

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
