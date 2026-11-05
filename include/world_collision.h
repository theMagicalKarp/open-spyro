/* world_collision.h — the world collision header (g_pWorldCollisionHeader),
 * the fourth section of the world bundle UnpackWorldDataChunks walks.
 *
 * Its pointer fields arrive as offsets relative to the header and the unpack
 * makes them absolute. Every reader besides the unpack is hand-written asm
 * (the ray/sphere/floor probes, UnpackWorldCollisionTri, the world chunk
 * animations), which reaches the header as word +0x2C of the world-table
 * block at g_pWorldChunkArray. The field roles below come from those probes:
 *   - CastRayWorldAndActors / CollideSphereWithWorldAndActors /
 *     FindGroundHeightBelow / FindWorldFloorBelowPoint walk xIndex then
 *     yzIndex to the candidate triangles in a cell, then unpack tris.
 *   - FindGroundHeightBelow and CollideSphereWithWorldAndActors publish
 *     groundCode[hit] to g_nGroundSurfaceType when hit < groundCodeCount
 *     (else 0xFF); its low 6 bits index g_pTriggerRegionTable.
 *   - FindWorldFloorBelowPoint publishes visZone[hit] to
 *     g_nWorldFloorVisZone. */

#ifndef OPEN_SPYRO_WORLD_COLLISION_H
#define OPEN_SPYRO_WORLD_COLLISION_H

typedef struct WorldCollisionHeader {
  int pad00;           /* 0x00 not read by any probe */
  int groundCodeCount; /* 0x04 triangles [0, n) have a groundCode byte */
  int xIndex;          /* 0x08 X-axis cell index (ushorts) */
  int yzIndex;         /* 0x0C combined Y/Z cell index (ushorts) */
  int tris;            /* 0x10 packed triangle records, stride 0xC */
  int visZone;         /* 0x14 per-triangle vis-zone byte */
  int groundCode;      /* 0x18 per-triangle ground code byte */
} WorldCollisionHeader;

/* COLL_HDR keeps the original's byte-offset access: an `int *` index
 * (`h[2]`) gives a different address RTL and UnpackWorldDataChunks stops
 * matching. */
#define COLL_HDR(hdr, field)                                                   \
  (*(int *)((char *)(hdr) + (int)&((WorldCollisionHeader *)0)->field))

#endif /* OPEN_SPYRO_WORLD_COLLISION_H */
