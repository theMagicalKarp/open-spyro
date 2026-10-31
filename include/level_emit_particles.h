/* level_emit_particles.h — declarations shared by the *_EmitParticles.c clones (and the
 * SpawnActor / BeginActorCutscene clones that use the same names), declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H
#define OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H

#include "sparx.h"

extern void *AllocEmitListRecord(int cls);

extern void VectorToSVectorShr2(void *dst, int *src);          /* vector -> short[3] >>2 */

extern short g_anCosineLut[];        /* cosine table */

extern short g_anSineLut[];        /* sine table */

extern int g_anCameraPos[];          /* camera world position */

extern int g_anSpyroWorldPos[];          /* Spyro world position */

extern int g_anSpyroBodyMtx[];          /* Spyro orientation matrix (position at -0x34) */

extern int g_nSparxHealth;

extern unsigned char g_bSpyroPersistentEulerZ;

/* Declared identically (by type) in two or more including files; hoisted here. */
extern char *g_apSpritePrimQueue[];
extern int g_dwHudBobSinePhase;
extern char g_pOtDepthBinHead0[];
extern char *g_pRenderScratchPrimTop[];
extern char *g_pRenderScratchRegionBase;
extern struct Actor *g_pSpriteRecordWriteCursor; /* sprite-record queue tail (grows down) */

/* Attach-point lookup the particle clones aim from. */
struct Actor;
extern void GetActorAttachPoint(struct Actor *parent, int slot, int *dst);
extern struct Actor *g_pActorListBase; /* level actor pool (tracking sparks store an owner index) */

#endif /* OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H */
