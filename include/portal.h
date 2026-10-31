/* portal.h — the level portals listed by g_apPortals (g_nPortalCount
 * entries), unpacked from the level bundle by UnpackWorldDataChunks and drawn
 * by DrawPortalsAndStaticActors as a swirling ring (g_nPortalViewYawSpin / PitchTilt).
 * Entry word 6 is the portal's actor (its path is where Spyro glides out) and
 * word 7 the level it leads to: returning from a level, TickLevelTransitionStream
 * finds the portal whose word 7 is g_nReturnPortalLevelId and places Spyro
 * on it. */

#ifndef OPEN_SPYRO_PORTAL_H
#define OPEN_SPYRO_PORTAL_H

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

/* One portal mesh header; only the tint word at +0x10 is read here. */
typedef struct PortalMesh {
  int pad00;
  int pad04;
  int pad08;
  int pad0C;
  unsigned int color; /* +0x10 */
} PortalMesh;

/* A portal: a convex ring of world-space points (its opening) that gets
   projected to screen space and stroked as an outline. */
typedef struct Portal {
  PortalMesh *pMesh; /* +0x00 */
  int nPoints;            /* +0x04 */
  int nx;                 /* +0x08 */
  int ny;                 /* +0x0c */
  int nz;                 /* +0x10 */
  int nGateIndex;         /* +0x14 */
  int actor;  /* +0x18 g_pActorListBase index of the portal actor */
  int levelId; /* +0x1c level it leads to */
  int pts[8][3]; /* +0x20 */
} Portal;

#endif /* OPEN_SPYRO_PORTAL_H */
