/* sky.h — the sky: untextured chunks drawn camera-centred (EmitSkyMesh zeroes
 * the GTE translation) behind the world, plus the background colour the
 * frame is cleared to. */

#ifndef OPEN_SPYRO_SKY_H
#define OPEN_SPYRO_SKY_H

/* A sky header: the level's own (g_nLevelSkyChunkCount..), the one drawn
 * (g_nSkyChunkCount..), and each portal's destination sky share it. The
 * portal ring is filled with this sky (EmitPortalSky) or, up close, with its
 * flat background colour (EmitPortalColorFill). */
typedef struct SkyHeader {
  int chunkCount;       /* +0x00 */
  void **chunks;        /* +0x04 */
  int zoneListCount;    /* +0x08 */
  void **zoneLists;     /* +0x0C per vis zone */
  unsigned int color;   /* +0x10 background colour (g_dwWorldFogColor when active) */
} SkyHeader;

#endif /* OPEN_SPYRO_SKY_H */
