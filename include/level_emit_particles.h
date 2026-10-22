/* level_emit_particles.h — declarations shared by the *_EmitParticles.c clones, declared once.
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

#endif /* OPEN_SPYRO_LEVEL_EMIT_PARTICLES_H */
