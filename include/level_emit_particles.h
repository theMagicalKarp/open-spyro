/* level_emit_particles.h — declarations shared by the *_EmitParticles.c clones (and the
 * SpawnActor / BeginActorCutscene clones that use the same names), declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H
#define OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H

extern void *AllocEmitListRecord(int cls);

extern void VectorToSVectorShr2(void *dst, int *src);          /* vector -> short[3] >>2 */

extern short g_anCosineLut[];        /* cosine table */

extern short g_anSineLut[];        /* sine table */

extern int g_anCameraPos[];          /* camera world position */

extern int g_anSpyroWorldPos[];          /* Spyro world position */

extern int g_anSpyroBodyMtx[];          /* Spyro orientation matrix (position at -0x34) */

extern int g_nLevelReadyFlag;

extern unsigned char g_bSpyroPersistentEulerZ;

/* Declared identically (by type) in two or more including files; hoisted here. */
extern char *g_apSpritePrimQueue[];
extern int g_dwHudBobSinePhase;
extern char g_pOtDepthBinHead0[];
extern char *g_pRenderScratchPrimTop[];
extern char *g_pRenderScratchRegionBase;
extern char *g_pSpriteRecordWriteCursor;

#endif /* OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H */
