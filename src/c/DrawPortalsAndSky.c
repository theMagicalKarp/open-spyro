#include "funcs.h"
#include "globals.h"
#include "match_asm.h"
#include "portal.h"
#include "types.h"
#include "vecmath.h"

/* DrawPortalsAndSky - VMA 0x80050bd0, 5148 bytes, frame 0x1a0.

   Matching notes (all load-bearing):
   - `maxX` is the same variable as the earlier CLZ `shift` (both s4): its
     first reference then precedes the first edge pass's cse block, so cse
     makes it the canonical zero register for that pass's loop guard and
     combine re-emits its `= 0` last; in the second pass nEdges (last used
     by the final `.live = 0`) wins instead and lands in the guard's delay
     slot.
   - the first dot-product block's aggregates live at function scope (the
     original keeps both sets of temp slots), and flagX/flagY/facing are
     declared before sumX/sumY with dist after them, so every spill slot
     lands where the original's does (spill order follows pseudo order).
   - `register ... asm()` pins on the edge-loop k / nPoints wrap / divisor,
     the `ko = k * 12` held offset, and an empty asm ref on yk reproduce the
     original's temp registers in the edge loops.
   - the mesh colour is read through a plain (non-struct) MEM so it is not
     hoisted above the preceding scalar stores. */

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

extern unsigned char
    g_abVisZoneGates[]; /* per-vis-zone gate bytes (one per portal zone) */
extern EdgeRec g_aPortalSilhouetteEdges[]; /* silhouette edge records */
extern ScreenPt g_aPortalRingScreenPts[];  /* projected ring points */
/* point k through a held byte offset (the original keeps k*12 live) */
#define KPT (*(ScreenPt *)((char *)g_aPortalRingScreenPts + ko))
extern int g_nCameraVisZone[]; /* [-0x10] view mtx, [-0x15] world mtx */

/* Render stage 4: draw the portals, each filled with its destination's sky
   (EmitPortalSky / EmitPortalSkyFogShaded, or a flat EmitPortalColorFill up
   close), then the active sky (EmitSkyMesh). The portal-sky view matrix is
   rebuilt every frame from the camera euler angles plus a slow spin/tilt
   (g_nPortalSkyYawSpin /
   ...PitchTilt, both advanced 2/frame here) and squashed to 5/8 on the middle
   row. Each live portal ring is projected point-by-point into
   g_aPortalRingScreenPts, depth-cued (and fog-blended during storms), then
   walked twice: once to accumulate the centroid, screen extents and silhouette
   edges of the ring as-is, and — when the portal is nearer than 0x4000 — once
   more with every point pushed 2 px away from the centroid, giving the fattened
   outline pass. A pass is emitted only when its extents fall inside the 512x240
   frame and the ring faces the camera. (0x80050bd0, 5148 bytes.) */
void DrawPortalsAndSky(void) {
  MATRIX viewMtx;         /* sp+0x10 */
  MATRIX outMtx;          /* sp+0x30 */
  int vDelta[3];          /* sp+0x50 */
  int vCenter[3];         /* sp+0x60 */
  int vAxis[3];           /* sp+0x70 */
  int vRel[3];            /* sp+0x80 */
  unsigned char euler[3]; /* sp+0x90 */
  MATRIX eulerMtx;        /* sp+0x98 */
  unsigned int color;
  int v;
  int i;
  int j;
  int k;
  int ko;
  int n;
  int maxX; /* doubles as the CLZ shift (one variable in the original) */
  int flagX;
  int flagY;
  int facing;
  int sumX;
  int sumY;
  int dist;
  int minX;
  int minY;
  int maxY;
  int flags;
  int nEdges;
  int xk;
  int yk;
  int dx;
  int dy;
  int zone;

  g_nPortalSkyYawSpin = (g_nPortalSkyYawSpin + 2) & 0xfff;
  g_nPortalSkyPitchTilt = (g_nPortalSkyPitchTilt + 2) & 0xfff;
  if (g_nPortalSkyPitchTilt >= 0x801) {
    g_nPortalSkyPitchTilt -= 0x1000;
  }
  if (g_nPortalSkyPitchTilt >= 0x81) {
    g_nPortalSkyPitchTilt = 0x80;
  }

  FillWord(&viewMtx, 0, 0x20);
  SetTransMatrix(&viewMtx);
  viewMtx.m[0][0] = 0x1000;
  v = LookupCosine(g_anCameraEulerPitchBlock[0] - g_nPortalSkyPitchTilt);
  viewMtx.m[1][1] = v;
  v = LookupSine(g_anCameraEulerPitchBlock[0] - g_nPortalSkyPitchTilt);
  viewMtx.m[2][1] = v;
  v = LookupSine(g_anCameraEulerPitchBlock[0] - g_nPortalSkyPitchTilt);
  viewMtx.m[1][2] = -v;
  v = LookupCosine(g_anCameraEulerPitchBlock[0] - g_nPortalSkyPitchTilt);
  viewMtx.m[2][2] = v;

  FillWord(&outMtx, 0, 0x20);
  v = LookupCosine(g_nCameraEulerYaw + g_nPortalSkyYawSpin);
  outMtx.m[0][0] = v;
  v = LookupSine(g_nCameraEulerYaw + g_nPortalSkyYawSpin);
  outMtx.m[2][0] = -v;
  outMtx.m[1][1] = 0x1000;
  v = LookupSine(g_nCameraEulerYaw + g_nPortalSkyYawSpin);
  outMtx.m[0][2] = v;
  v = LookupCosine(g_nCameraEulerYaw + g_nPortalSkyYawSpin);
  outMtx.m[2][2] = v;
  MulMatrix(&viewMtx, &outMtx);

  FillWord(&outMtx, 0, 0x20);
  v = LookupCosine(g_nCameraEulerRoll);
  outMtx.m[0][0] = v;
  v = LookupSine(g_nCameraEulerRoll);
  outMtx.m[1][0] = -v;
  v = LookupSine(g_nCameraEulerRoll);
  outMtx.m[0][1] = v;
  v = LookupCosine(g_nCameraEulerRoll);
  outMtx.m[1][1] = v;
  outMtx.m[2][2] = 0x1000;
  MulMatrix(&viewMtx, &outMtx);

  CopyWords(&outMtx, &viewMtx, 0x14);
  v = viewMtx.m[1][0] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  outMtx.m[1][0] = v >> 9;
  v = viewMtx.m[1][1] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  outMtx.m[1][1] = v >> 9;
  v = viewMtx.m[1][2] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  outMtx.m[1][2] = v >> 9;

  for (i = 0; i < g_nPortalCount; i++) {
    if (g_apPortals[i]->nGateIndex < 0 ||
        g_abVisZoneGates[g_apPortals[i]->nGateIndex] != 0) {
      flagY = 1;
      flagX = 1;
      n = LookupCosine(g_nVblankTickCount * 16 + (i << 9));
      color = abs(n) >> 1;
      color = ComputeDepthCuedColor(
          *(unsigned int *)((char *)g_apPortals[i]->pSky + 0x10), 0xffffff,
          color);
      SubtractVector(vDelta, g_apPortals[i]->pts[0], g_anCameraPos);
      maxX =
          CountLeadingZeros(abs(vDelta[0]) + abs(vDelta[1]) + abs(vDelta[2]));
      if (maxX >= 0xf) {
        maxX >>= 1;
        RShiftVector3(vDelta, maxX);
        dist = VectorLength(vDelta, VEC_LEN_XYZ) << maxX;
      } else {
        dist = VectorLength(vDelta, VEC_LEN_XYZ);
      }
      maxX = CountLeadingZeros(dist) - 0xd;
      if (maxX < 0) {
        maxX = 0;
      }
      if (g_nPortalDistanceFogEnabled != 0 && dist >= 0x4001) {
        j = (0x8000 - dist) >> 2;
        if (j < 0) {
          j = 0;
        }
        if (j >= 0x1001) {
          j = 0x1000;
        }
        color = ComputeDepthCuedColor(g_dwWorldFogColor, color, j);
      }

      ZeroVector(vCenter);
      for (j = 0; j < g_apPortals[i]->nPoints; j++) {
        ProjectWorldPointGTE((int *)&g_aPortalRingScreenPts[j],
                             g_apPortals[i]->pts[j], maxX);
        AddVector(vCenter, vCenter, g_apPortals[i]->pts[j]);
      }
      vCenter[0] /= g_apPortals[i]->nPoints;
      vCenter[1] /= g_apPortals[i]->nPoints;
      vCenter[2] /= g_apPortals[i]->nPoints;
      SubtractVector(vDelta, g_anCameraPos, g_apPortals[i]->pts[1]);
      RShiftVector3(vDelta, maxX);

      facing = -vDelta[0] * g_apPortals[i]->nx -
               vDelta[1] * g_apPortals[i]->ny - vDelta[2] * g_apPortals[i]->nz;
      minX = 0x13f8;
      minY = 0xf0;
      maxY = 0;
      flags = 0;
      j = 0;
      nEdges = 0;
      maxX = 0;
      g_nPortalOtDepthBin = 0;
      sumX = 0;
      sumY = 0;
      g_pPortalDeferredTriStackTop = g_anPortalDeferredTriStack;

      for (; j < g_apPortals[i]->nPoints; j++) {
        register int k asm("$5");

        if (facing > 0) {
          k = j + 1;
          if (k == g_apPortals[i]->nPoints) {
            k = 0;
          }
        } else {
          k = j - 1;
          if (k < 0) {
            {
              register int n asm("$2") = g_apPortals[i]->nPoints;

              k = n - 1;
            }
          }
        }
        sumX += g_aPortalRingScreenPts[k].x;
        sumY += g_aPortalRingScreenPts[k].y;
        g_nPortalOtDepthBin += g_aPortalRingScreenPts[k].z;
        if ((unsigned int)(g_aPortalRingScreenPts[k].x - 1) < 0x1ff &&
            (unsigned int)(g_aPortalRingScreenPts[k].y - 1) < 0xef &&
            g_aPortalRingScreenPts[k].z > 0) {
          flags = 1;
        }
        ko = k * 12;
        xk = KPT.x;
        if (maxX < xk) {
          do {
            maxX = xk;
          } while (0);
        }
        if (xk < minX) {
          minX = xk;
        }
        yk = KPT.y;
        if (maxY < yk) {
          maxY = yk;
        }
        if (yk < minY) {
          minY = yk;
        }
        if ((g_aPortalRingScreenPts[j].x < 0x200 || xk < 0x200) &&
            (g_aPortalRingScreenPts[j].x > 0 || xk > 0) &&
            (g_aPortalRingScreenPts[j].y < 0xf0 || yk < 0xf0) &&
            (g_aPortalRingScreenPts[j].y > 0 || yk > 0)) {
          dx = g_aPortalRingScreenPts[j].x - xk;
          if (dx < 0) {
            dx = -dx;
          }
          if (dx >= 3) {
            flagX = 0;
          }
          dy = g_aPortalRingScreenPts[j].y - yk;
          KEEP_IN_REG(yk);
          if (dy < 0) {
            dy = -dy;
          }
          if (dy >= 3) {
            flagY = 0;
          }
          g_aPortalSilhouetteEdges[nEdges].live = 1;
          g_aPortalSilhouetteEdges[nEdges].p0 =
              (g_aPortalRingScreenPts[j].y << 16) |
              (unsigned short)g_aPortalRingScreenPts[j].x;
          g_aPortalSilhouetteEdges[nEdges].p1 =
              (KPT.y << 16) | (unsigned short)KPT.x;
          g_aPortalSilhouetteEdges[nEdges].nx =
              g_aPortalRingScreenPts[j].y - KPT.y;
          g_aPortalSilhouetteEdges[nEdges].ny =
              KPT.x - g_aPortalRingScreenPts[j].x;
          g_aPortalSilhouetteEdges[nEdges].c =
              -g_aPortalSilhouetteEdges[nEdges].nx *
                  g_aPortalRingScreenPts[j].x -
              g_aPortalSilhouetteEdges[nEdges].ny * g_aPortalRingScreenPts[j].y;
          nEdges++;
        }
      }

      if ((flagX == 0 && flagY == 0) || nEdges == 0) {
        {
          register int np asm("$2") = g_apPortals[i]->nPoints;

          sumX /= np;
          g_nPortalOtDepthBin = (g_nPortalOtDepthBin / np) >> 7;
          sumY /= np;
        }
        if (g_nPortalOtDepthBin >= 0x100) {
          g_nPortalOtDepthBin += 0x40;
        }
        if (g_nPortalOtDepthBin >= 0x800) {
          g_nPortalOtDepthBin = 0x7ff;
        }
        if ((minX < 0x200 && minY < 0xf0 && maxX > 0 && maxY > 0 &&
             g_nPortalOtDepthBin > 0 && nEdges != 0) ||
            (minX <= 0 && minY <= 0 && maxX >= 0x200 && maxY >= 0xf0 &&
             g_nPortalOtDepthBin > 0)) {
          flags |= 2;
        } else {
          flags = 0;
        }
        do {
          do {
            if (flags == 2 && dist < 0x1000) {
              SubtractVector(vRel, vCenter, g_anCameraPos);
              vAxis[0] = 0x1000;
              vAxis[1] = 0;
              vAxis[2] = 0;
              euler[0] = (unsigned short)g_nCameraEulerRoll >> 4;
              euler[1] = (unsigned short)g_nCameraEulerPitch >> 4;
              euler[2] = (unsigned short)g_nCameraEulerYaw >> 4;
              ApplyEulerRotation(euler, (int *)&eulerMtx, 0);
              RotateVectorByMatrix((int *)&eulerMtx, vAxis, vAxis);
              if (vAxis[0] * vRel[0] + vAxis[1] * vRel[1] + vAxis[2] * vRel[2] <
                  0) {
                flags = 0;
              }
            }
          } while (0);
        } while (0);
        if (flags != 0) {
          if (minX < 0) {
            minX = 0;
          }
          if (minY < 0) {
            minY = 0;
          }
          if (maxX >= 0x201) {
            maxX = 0x200;
          }
          if (maxY >= 0xf1) {
            maxY = 0xf0;
          }
          g_anPortalScissorRect[0] = minY << 16;
          g_anPortalScissorRect[1] = maxY << 16;
          g_anPortalScissorRect[2] = maxX << 16;
          g_anPortalScissorRect[3] = minX << 16;
          g_aPortalSilhouetteEdges[nEdges].live = 0;
          if (dist < 0x4000) {
            if (dist >= 0x3001) {
              g_nFadeProgress = dist - 0x3000;
              g_dwSkyFadeColor = ComputeDepthCuedColor(
                  *(unsigned int *)((char *)g_apPortals[i]->pSky + 0x10), color,
                  dist - 0x3000);
              EmitPortalColorFill(&g_dwSkyFadeColor);
            } else {
              EmitPortalColorFill(&g_apPortals[i]->pSky->color);
            }
          } else {
            EmitPortalColorFill(&color);
          }
        }

        if (dist < 0x4000) {
          minX = 0x200;
          maxX = 0;
          minY = 0xf0;
          maxY = 0;
          flags = 0;
          g_pPortalDeferredTriStackTop = g_anPortalDeferredTriStack;
          nEdges = 0;
          for (j = 0; j < g_apPortals[i]->nPoints; j++) {
            int t;

            t = g_aPortalRingScreenPts[j].x;
            if (sumX < t) {
              g_aPortalRingScreenPts[j].x = t + 2;
            } else if (t < sumX) {
              g_aPortalRingScreenPts[j].x = t - 2;
            }
            t = g_aPortalRingScreenPts[j].y;
            if (sumY < t) {
              g_aPortalRingScreenPts[j].y = t + 2;
            } else if (t < sumY) {
              g_aPortalRingScreenPts[j].y = t - 2;
            }
            t = g_aPortalRingScreenPts[j].x;
            if (maxX < t) {
              maxX = t;
            }
            if (t < minX) {
              minX = t;
            }
            t = g_aPortalRingScreenPts[j].y;
            if (maxY < t) {
              maxY = t;
            }
            if (t < minY) {
              minY = t;
            }
          }

          for (j = 0; j < g_apPortals[i]->nPoints; j++) {
            register int k asm("$5");

            if (facing > 0) {
              k = j + 1;
              if (k == g_apPortals[i]->nPoints) {
                k = 0;
              }
            } else {
              k = j - 1;
              if (k < 0) {
                {
                  register int n asm("$2") = g_apPortals[i]->nPoints;

                  k = n - 1;
                }
              }
            }
            if ((unsigned int)(g_aPortalRingScreenPts[k].x - 1) < 0x1ff &&
                (unsigned int)(g_aPortalRingScreenPts[k].y - 1) < 0xef &&
                g_aPortalRingScreenPts[k].z > 0) {
              flags = 1;
            }
            if ((g_aPortalRingScreenPts[j].x < 0x200 ||
                 g_aPortalRingScreenPts[k].x < 0x200) &&
                (g_aPortalRingScreenPts[j].x > 0 ||
                 g_aPortalRingScreenPts[k].x > 0) &&
                (g_aPortalRingScreenPts[j].y < 0xf0 ||
                 g_aPortalRingScreenPts[k].y < 0xf0) &&
                (g_aPortalRingScreenPts[j].y > 0 ||
                 g_aPortalRingScreenPts[k].y > 0)) {
              g_aPortalSilhouetteEdges[nEdges].live = 1;
              g_aPortalSilhouetteEdges[nEdges].p0 =
                  (g_aPortalRingScreenPts[j].y << 16) |
                  (unsigned short)g_aPortalRingScreenPts[j].x;
              g_aPortalSilhouetteEdges[nEdges].p1 =
                  (g_aPortalRingScreenPts[k].y << 16) |
                  (unsigned short)g_aPortalRingScreenPts[k].x;
              g_aPortalSilhouetteEdges[nEdges].nx =
                  g_aPortalRingScreenPts[j].y - g_aPortalRingScreenPts[k].y;
              g_aPortalSilhouetteEdges[nEdges].ny =
                  g_aPortalRingScreenPts[k].x - g_aPortalRingScreenPts[j].x;
              g_aPortalSilhouetteEdges[nEdges].c =
                  -g_aPortalSilhouetteEdges[nEdges].nx *
                      g_aPortalRingScreenPts[j].x -
                  g_aPortalSilhouetteEdges[nEdges].ny *
                      g_aPortalRingScreenPts[j].y;
              nEdges++;
            }
          }

          if ((minX < 0x200 && minY < 0xf0 && maxX > 0 && maxY > 0 &&
               g_nPortalOtDepthBin > 0 && nEdges != 0) ||
              (minX <= 0 && minY <= 0 && maxX >= 0x200 && maxY >= 0xf0 &&
               g_nPortalOtDepthBin > 0)) {
            flags |= 2;
          } else {
            flags = 0;
          }
          if (flags == 2 && dist < 0x1000) {
            int vAxis2[3];           /* sp+0xc0 */
            int vRel2[3];            /* sp+0xd0 */
            unsigned char euler2[3]; /* sp+0xe0 */
            MATRIX eulerMtx2;        /* sp+0xe8 */

            SubtractVector(vRel2, vCenter, g_anCameraPos);
            vAxis2[0] = 0x1000;
            vAxis2[1] = 0;
            vAxis2[2] = 0;
            euler2[0] = (unsigned short)g_nCameraEulerRoll >> 4;
            euler2[1] = (unsigned short)g_nCameraEulerPitch >> 4;
            euler2[2] = (unsigned short)g_nCameraEulerYaw >> 4;
            ApplyEulerRotation(euler2, (int *)&eulerMtx2, 0);
            RotateVectorByMatrix((int *)&eulerMtx2, vAxis2, vAxis2);
            if (vAxis2[0] * vRel2[0] + vAxis2[1] * vRel2[1] +
                    vAxis2[2] * vRel2[2] <
                0) {
              flags = 0;
            }
          }
          if (flags != 0) {
            if (minX < 0) {
              minX = 0;
            }
            if (minY < 0) {
              minY = 0;
            }
            if (maxX >= 0x201) {
              maxX = 0x200;
            }
            if (maxY >= 0xf1) {
              maxY = 0xf0;
            }
            g_anPortalScissorRect[0] = minY << 16;
            g_anPortalScissorRect[1] = maxY << 16;
            g_anPortalScissorRect[2] = maxX << 16;
            g_anPortalScissorRect[3] = minX << 16;
            g_aPortalSilhouetteEdges[nEdges].live = 0;
            if (dist >= 0x3001) {
              g_nFadeProgress = dist - 0x3000;
              g_dwSkyFadeColor = ComputeDepthCuedColor(
                  *(unsigned int *)((char *)g_apPortals[i]->pSky + 0x10), color,
                  dist - 0x3000);
              EmitPortalSkyFogShaded((int *)g_apPortals[i]->pSky, &viewMtx,
                                     &outMtx);
            } else {
              EmitPortalSky((int *)g_apPortals[i]->pSky, &viewMtx, &outMtx);
            }
          }
        }
      }
    }
  }

  zone = g_nCameraVisZone[0];
  if (zone >= g_nVisZoneCount) {
    zone = -1;
  }
  EmitSkyMesh(zone, (MATRIX *)&g_nCameraVisZone[-0x10],
              (MATRIX *)&g_nCameraVisZone[-0x15]);
}
