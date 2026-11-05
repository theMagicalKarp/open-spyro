/* portal.h — the level portals listed by g_apPortals (g_nPortalCount
 * entries), unpacked from the level bundle by UnpackWorldDataChunks and drawn
 * by DrawPortalsAndSky as a swirling ring (g_nPortalSkyYawSpin / PitchTilt).
 * Entry word 6 is the portal's actor (its path is where Spyro glides out) and
 * word 7 the level it leads to: returning from a level, TickLevelTransitionStream
 * finds the portal whose word 7 is g_nReturnPortalLevelId and places Spyro
 * on it. */

#ifndef OPEN_SPYRO_PORTAL_H
#define OPEN_SPYRO_PORTAL_H

#include "sky.h"

/* A hub portal's camera path (type 398's desc; g_pPortalCamPathData): node
 * count and current node, then `count` waypoints. UpdatePortalPathCamera walks
 * the camera along them (SelectCameraMode seeds `index` at either end); the
 * portal actors pull Spyro toward the midpoint of the first two. */
typedef struct PortalCamPoint {
  int pos[3]; /* 0x00 */
  int pad0C;  /* 0x0C */
} PortalCamPoint;

typedef struct PortalCamPath {
  unsigned char count;    /* 0x00 waypoints */
  unsigned char index;    /* 0x01 the waypoint the camera is heading for */
  unsigned char pad02[6]; /* 0x02 */
  PortalCamPoint point[2]; /* 0x08 `count` of them */
} PortalCamPath;


/* A portal: a convex ring of world-space points (its opening) that gets
   projected to screen space and stroked as an outline. */
typedef struct Portal {
  SkyHeader *pSky;   /* +0x00 the destination level's sky */
  int nPoints;            /* +0x04 */
  int nx;                 /* +0x08 ring plane normal: the camera's side of it picks the winding DrawPortalsAndSky walks the ring in */
  int ny;                 /* +0x0c */
  int nz;                 /* +0x10 */
  int gateChunk;          /* +0x14 world chunk gating the portal (g_abWorldChunkDrawn); < 0 = always drawn */
  int actor;  /* +0x18 g_pActorListBase index of the portal actor */
  int levelId; /* +0x1c level it leads to */
  int pts[8][3]; /* +0x20 nPoints ring points (UnpackWorldDataChunks skips 8 + nPoints * 3 words) */
} Portal;

/* One projected ring point: screen x/y plus the OT depth term. */
typedef struct ScreenPt {
  int x;
  int y;
  int z;
} ScreenPt;

/* One silhouette edge: live flag, the two packed endpoints, the edge normal
   and its constant term (the half-plane test the rasterizer clips against). */
typedef struct EdgeRec {
  int live;
  int p0;
  int p1;
  int nx;
  int ny;
  int c;
} EdgeRec;

extern EdgeRec g_aPortalSilhouetteEdges[]; /* DrawPortalsAndSky's silhouette edges */
extern ScreenPt g_aPortalRingScreenPts[];  /* the ring points projected to screen */

#endif /* OPEN_SPYRO_PORTAL_H */
