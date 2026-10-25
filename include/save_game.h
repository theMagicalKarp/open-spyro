/* save_game.h — the 0x600-byte save image BuildSaveGameBuffer packs and
 * ApplySaveGameBuffer unpacks (three of them follow the memory-card file
 * header in the titlescreen's card buffer). Field order follows the two
 * serializers; ComputeSaveGameChecksum covers the image up to `checksum`. */

#ifndef OPEN_SPYRO_SAVE_GAME_H
#define OPEN_SPYRO_SAVE_GAME_H

#define TOTAL_LEVEL_COUNT 36

typedef struct {
  int worldFlags[5];                      /* 0x00 g_anWorldPersistentFlags */
  unsigned char worldLevelBytes[5][5];    /* 0x14 g_abWorldPerLevelByteTable */
} SaveGameWorlds;                         /* 0x30 */

typedef struct {
  unsigned char levelId;                  /* 0x00 g_nActiveLevelId */
  unsigned char sfxVolume;                /* 0x01 */
  unsigned char musicVolume;              /* 0x02 */
  unsigned char vibration;                /* 0x03 */
  int cameraOptions;                      /* 0x04 g_dwActiveCameraOptions */
  unsigned char monoMix;                  /* 0x08 */
  unsigned char escapeRunCount;           /* 0x09 g_anLevelEscapeRunCount */
  unsigned char escapeRunCountLane1;      /* 0x0A g_nLevelEscapeRunCountLane1 */
  unsigned char extraLives;               /* 0x0B */
  int unk_0x0C;                           /* 0x0C */
  SaveGameWorlds worlds;                  /* 0x10 */
  unsigned char levelVisited[TOTAL_LEVEL_COUNT];    /* 0x40 */
  unsigned char levelExited[TOTAL_LEVEL_COUNT]; /* 0x64 */
  unsigned char levelDragons[TOTAL_LEVEL_COUNT];    /* 0x88 */
  short levelGems[TOTAL_LEVEL_COUNT];               /* 0xAC */
  unsigned char levelEggs[18];            /* 0xF4 worlds 0-2 only; the
                                           * serializers walk all 36 and the
                                           * balloonist bytes overwrite the tail */
  unsigned char balloonistState[6];       /* 0x106 */
  unsigned char killBitmaps[TOTAL_LEVEL_COUNT * 32]; /* 0x10C */
  int checksum;                           /* 0x58C */
  char pad[0x70];
} SaveGameBuffer;                         /* 0x600 */

/* Byte offset of a SaveGameBuffer field, and that field read through a byte
 * pointer `p` offset from the image base (the serializers walk the image
 * with byte-pointer cursors). */
#define SAVE_OFFSET(field) ((int)&((SaveGameBuffer *)0)->field)
#define SAVE_RAW(p, type, field) (*(type *)((char *)(p) + SAVE_OFFSET(field)))

#endif /* OPEN_SPYRO_SAVE_GAME_H */
