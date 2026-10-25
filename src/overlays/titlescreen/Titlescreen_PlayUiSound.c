#include "level_balloonist_cutscene.h"
/* Titlescreen_PlayUiSound (0x8007AAD4, titlescreen overlay).
 *
 * Queue one of the title-screen UI sounds: point the SPU driver at the shared
 * cutscene sound definition, fill it for the requested sample, and fire it at
 * full volume through the dragon-cutscene voice.
 *
 * Externs below keep our splat autolabel names for the main-EXE symbols,
 * which the overlay link PROVIDE()s by address.
 * The SPU-driver fields must be accessed through one struct (not separate scalar
 * externs): gcc 2.7.2 reloads the sound-definition pointer after every store
 * through a struct field, which is what the original codegen shows — a plain
 * global pointer gets constant-folded into direct stores and diverges.
 *
 * Verified byte-identical inside the relinked titlescreen.ovl.
 */

typedef struct {
  int nAddr;              /* 0x0 SPU sample address */
  int nLoopAddr;          /* 0x4 */
  short nVolume;          /* 0x8 */
  short nPitch;           /* 0xA */
  short nPitchVariance;   /* 0xC */
  short nPitchMultiplier; /* 0xE */
  int nVarianceType;      /* 0x10 */
} SoundDef;

typedef struct {
  SoundDef *pSoundDef;          /* 0x0  (0x800761D0) active sound definition */
  int pad04;                     /* 0x4 */
  int pad08;                     /* 0x8 */
  int nNextSoundOverrideFlags;  /* 0xC  (0x800761DC) */
  int pad10;                    /* 0x10 */
  int pad14;                    /* 0x14 */
  short nVolumeOverrideLeft;    /* 0x18 (0x800761E8) */
  short nVolumeOverrideRight;   /* 0x1A (0x800761EA) */
} SpuDriver;

extern SpuDriver g_pLevelSampleBankEntries;  /* the SPU driver state */
extern SoundDef g_CutsceneSoundDef;   /* the shared cutscene sound definition */
extern int g_anTitleSampleOffsets[];      /* title-screen SPU sample offset table */
extern int g_bDragonRescueSfxVoiceMarker;        /* dragon-cutscene sound voice */


void Titlescreen_PlayUiSound(int nSoundId) {
  g_pLevelSampleBankEntries.pSoundDef = &g_CutsceneSoundDef;
  g_pLevelSampleBankEntries.pSoundDef->nAddr = g_anTitleSampleOffsets[nSoundId] + 0x1010;
  g_pLevelSampleBankEntries.pSoundDef->nLoopAddr = -1;
  g_pLevelSampleBankEntries.pSoundDef->nVolume = 0x50;
  g_pLevelSampleBankEntries.pSoundDef->nPitch = 0x800;
  g_pLevelSampleBankEntries.pSoundDef->nPitchVariance = 0;
  g_pLevelSampleBankEntries.pSoundDef->nPitchMultiplier = 0;
  g_pLevelSampleBankEntries.pSoundDef->nVarianceType = 0;
  g_pLevelSampleBankEntries.nNextSoundOverrideFlags = 1;
  g_pLevelSampleBankEntries.nVolumeOverrideLeft = 0x3FFF;
  g_pLevelSampleBankEntries.nVolumeOverrideRight = 0x3FFF;
  PlaySoundEffect(0, 0, 16, (unsigned char *)&g_bDragonRescueSfxVoiceMarker);
}
