/* func_titlescreen_8007AAD4 (0x8007AAD4, titlescreen overlay).
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
  int unk4;                     /* 0x4 */
  int unk8;                     /* 0x8 */
  int nNextSoundOverrideFlags;  /* 0xC  (0x800761DC) */
  int unk10;                    /* 0x10 */
  int unk14;                    /* 0x14 */
  short nVolumeOverrideLeft;    /* 0x18 (0x800761E8) */
  short nVolumeOverrideRight;   /* 0x1A (0x800761EA) */
} SpuDriver;

extern SpuDriver D_800761D0;  /* the SPU driver state */
extern SoundDef D_800778F0;   /* the shared cutscene sound definition */
extern int D_8006FA94[];      /* title-screen SPU sample offset table */
extern int D_80077084;        /* dragon-cutscene sound voice */

extern void func_80055A78(int, void *, int, unsigned char *); /* PlaySound */

void func_titlescreen_8007AAD4(int nSoundId) {
  D_800761D0.pSoundDef = &D_800778F0;
  D_800761D0.pSoundDef->nAddr = D_8006FA94[nSoundId] + 0x1010;
  D_800761D0.pSoundDef->nLoopAddr = -1;
  D_800761D0.pSoundDef->nVolume = 0x50;
  D_800761D0.pSoundDef->nPitch = 0x800;
  D_800761D0.pSoundDef->nPitchVariance = 0;
  D_800761D0.pSoundDef->nPitchMultiplier = 0;
  D_800761D0.pSoundDef->nVarianceType = 0;
  D_800761D0.nNextSoundOverrideFlags = 1;
  D_800761D0.nVolumeOverrideLeft = 0x3FFF;
  D_800761D0.nVolumeOverrideRight = 0x3FFF;
  func_80055A78(0, 0, 16, (unsigned char *)&D_80077084);
}
