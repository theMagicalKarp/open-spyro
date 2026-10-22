#include "types.h"

/* DrawActors - VMA 0x80050bd0, 5148 bytes, frame 0x1a0.

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

extern int abs(int x);

extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void CopyWords(void *dst, void *src, int byte_count);
extern void SetTransMatrix(MATRIX *m);
extern void MulMatrix(MATRIX *a, MATRIX *b);
extern int LookupSine(unsigned int angle);
extern int LookupCosine(unsigned int angle);
extern void SubtractVector(int *dst, int *a, int *b);
extern void AddVector(int *dst, int *a, int *b);
extern void ZeroVector(int *v);
extern void RShiftVector3(int *v, unsigned int bits);
extern unsigned int VectorLength(int *v, int with_z);
extern int CountLeadingZeros(int x);
extern unsigned int ComputeDepthCuedColor(unsigned int base, unsigned int color,
                                          unsigned int t);
extern void ProjectWorldPointGTE(int *dst, int *src, unsigned int shift);
extern void ApplyEulerRotation(unsigned char *euler, MATRIX *m, MATRIX *p);
extern void RotateVectorByMatrix(MATRIX *m, int *src, int *dst);
extern void EmitDynamicActorMeshFogged(unsigned int *color);
extern void EmitDynamicActorMeshScissored(int *mesh, MATRIX *viewMtx,
                                          MATRIX *outMtx);
extern void EmitDynamicActorMeshFogShaded(int *mesh, MATRIX *viewMtx,
                                          MATRIX *outMtx);
extern void EmitStaticActorMeshList(int lod, MATRIX *viewMtx, MATRIX *worldMtx);

/* One scripted-mob mesh header; only the tint word at +0x10 is read here. */
typedef struct ScriptedMobMesh {
  int pad00;
  int pad04;
  int pad08;
  int pad0C;
  unsigned int color; /* +0x10 */
} ScriptedMobMesh;

/* A scripted "mob" billboard: a convex ring of world-space points that gets
   projected to screen space and stroked as an outline. */
typedef struct ScriptedMob {
  ScriptedMobMesh *pMesh; /* +0x00 */
  int nPoints;            /* +0x04 */
  int nx;                 /* +0x08 */
  int ny;                 /* +0x0c */
  int nz;                 /* +0x10 */
  int nGateIndex;         /* +0x14 */
  int pad18;
  int pad1C;
  int pts[8][3]; /* +0x20 */
} ScriptedMob;

extern int g_nScriptedMobViewYawSpin;
extern int g_nScriptedMobViewPitchTilt;
extern int g_nScriptedMobCount;
extern ScriptedMob *g_apScriptedMobEntries[];
extern short g_nCameraEulerRoll;
extern short g_nCameraEulerPitch;
extern short g_nCameraEulerYaw;
extern int g_anCameraPos[3];
extern int g_nVblankTickCount;
extern int g_nLevelStormFlag;
extern unsigned int g_dwWorldFogColor;
extern int g_nActorMeshOtDepthBin;
extern int g_anActorDeferredTriStack[64];
extern int *g_pActorDeferredTriStackTop;
extern int g_anActorScissorRect[4];
extern int g_nFadeProgress;
extern unsigned int g_dwActorMeshFadeColor;

/* Held-base alias for &g_nCameraEulerPitch (0x80076e1e). */
extern short g_anCameraEulerPitchBlock[];

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

extern unsigned char g_abVisZoneGates[]; /* per-mob visibility gate bytes */
extern EdgeRec g_aMobSilhouetteEdges[];  /* silhouette edge records */
extern ScreenPt g_aMobRingScreenPts[];   /* projected ring points */
/* point k through a held byte offset (the original keeps k*12 live) */
#define KPT (*(ScreenPt *)((char *)g_aMobRingScreenPts + ko))
extern int
    g_nCameraFloorSurfaceType[];   /* [-0x10] view mtx, [-0x15] world mtx */
extern int g_nStaticMeshLodCutoff; /* static-mesh LOD cutoff */

/* Render stage 4: draw the scripted-mob billboards, then the static actor
   mesh list. The camera-relative view matrix is rebuilt every frame from the
   camera euler angles plus a slow spin/tilt (g_nScriptedMobViewYawSpin /
   ...PitchTilt, both advanced 2/frame here) and squashed to 5/8 on the middle
   row. Each live mob is projected point-by-point into g_aMobRingScreenPts,
   depth-cued (and fog-blended during storms), then walked twice: once to
   accumulate the centroid, screen extents and silhouette edges of the ring
   as-is, and — when the mob is nearer than 0x4000 — once more with every point
   pushed 2 px away from the centroid, giving the fattened outline pass. A pass
   is emitted only when its extents fall inside the 512x240 frame and the ring
   faces the camera. (0x80050bd0, 5148 bytes.) */
void DrawActors(void) {
  MATRIX viewMtx;         /* sp+0x10 */
  MATRIX outMtx;          /* sp+0x30 */
  int vDelta[3];          /* sp+0x50 */
  int vCenter[3];         /* sp+0x60 */
  int vAxis[3];           /* sp+0x70 */
  int vRel[3];            /* sp+0x80 */
  unsigned char euler[3]; /* sp+0x90 */
  MATRIX eulerMtx;        /* sp+0x98 */
  unsigned int color;
  int iVar1;
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
  int lod;

  g_nScriptedMobViewYawSpin = (g_nScriptedMobViewYawSpin + 2) & 0xfff;
  g_nScriptedMobViewPitchTilt = (g_nScriptedMobViewPitchTilt + 2) & 0xfff;
  if (g_nScriptedMobViewPitchTilt >= 0x801) {
    g_nScriptedMobViewPitchTilt -= 0x1000;
  }
  if (g_nScriptedMobViewPitchTilt >= 0x81) {
    g_nScriptedMobViewPitchTilt = 0x80;
  }

  FillWord(&viewMtx, 0, 0x20);
  SetTransMatrix(&viewMtx);
  viewMtx.m[0][0] = 0x1000;
  iVar1 =
      LookupCosine(g_anCameraEulerPitchBlock[0] - g_nScriptedMobViewPitchTilt);
  viewMtx.m[1][1] = iVar1;
  iVar1 =
      LookupSine(g_anCameraEulerPitchBlock[0] - g_nScriptedMobViewPitchTilt);
  viewMtx.m[2][1] = iVar1;
  iVar1 =
      LookupSine(g_anCameraEulerPitchBlock[0] - g_nScriptedMobViewPitchTilt);
  viewMtx.m[1][2] = -iVar1;
  iVar1 =
      LookupCosine(g_anCameraEulerPitchBlock[0] - g_nScriptedMobViewPitchTilt);
  viewMtx.m[2][2] = iVar1;

  FillWord(&outMtx, 0, 0x20);
  iVar1 = LookupCosine(g_nCameraEulerYaw + g_nScriptedMobViewYawSpin);
  outMtx.m[0][0] = iVar1;
  iVar1 = LookupSine(g_nCameraEulerYaw + g_nScriptedMobViewYawSpin);
  outMtx.m[2][0] = -iVar1;
  outMtx.m[1][1] = 0x1000;
  iVar1 = LookupSine(g_nCameraEulerYaw + g_nScriptedMobViewYawSpin);
  outMtx.m[0][2] = iVar1;
  iVar1 = LookupCosine(g_nCameraEulerYaw + g_nScriptedMobViewYawSpin);
  outMtx.m[2][2] = iVar1;
  MulMatrix(&viewMtx, &outMtx);

  FillWord(&outMtx, 0, 0x20);
  iVar1 = LookupCosine(g_nCameraEulerRoll);
  outMtx.m[0][0] = iVar1;
  iVar1 = LookupSine(g_nCameraEulerRoll);
  outMtx.m[1][0] = -iVar1;
  iVar1 = LookupSine(g_nCameraEulerRoll);
  outMtx.m[0][1] = iVar1;
  iVar1 = LookupCosine(g_nCameraEulerRoll);
  outMtx.m[1][1] = iVar1;
  outMtx.m[2][2] = 0x1000;
  MulMatrix(&viewMtx, &outMtx);

  CopyWords(&outMtx, &viewMtx, 0x14);
  iVar1 = viewMtx.m[1][0] * 0x140;
  if (iVar1 < 0) {
    iVar1 += 0x1ff;
  }
  outMtx.m[1][0] = iVar1 >> 9;
  iVar1 = viewMtx.m[1][1] * 0x140;
  if (iVar1 < 0) {
    iVar1 += 0x1ff;
  }
  outMtx.m[1][1] = iVar1 >> 9;
  iVar1 = viewMtx.m[1][2] * 0x140;
  if (iVar1 < 0) {
    iVar1 += 0x1ff;
  }
  outMtx.m[1][2] = iVar1 >> 9;

  for (i = 0; i < g_nScriptedMobCount; i++) {
    if (g_apScriptedMobEntries[i]->nGateIndex < 0 ||
        g_abVisZoneGates[g_apScriptedMobEntries[i]->nGateIndex] != 0) {
      flagY = 1;
      flagX = 1;
      n = LookupCosine(g_nVblankTickCount * 16 + (i << 9));
      color = abs(n) >> 1;
      color = ComputeDepthCuedColor(
          *(unsigned int *)((char *)g_apScriptedMobEntries[i]->pMesh + 0x10),
          0xffffff, color);
      SubtractVector(vDelta, g_apScriptedMobEntries[i]->pts[0], g_anCameraPos);
      maxX =
          CountLeadingZeros(abs(vDelta[0]) + abs(vDelta[1]) + abs(vDelta[2]));
      if (maxX >= 0xf) {
        maxX >>= 1;
        RShiftVector3(vDelta, maxX);
        dist = VectorLength(vDelta, 1) << maxX;
      } else {
        dist = VectorLength(vDelta, 1);
      }
      maxX = CountLeadingZeros(dist) - 0xd;
      if (maxX < 0) {
        maxX = 0;
      }
      if (g_nLevelStormFlag != 0 && dist >= 0x4001) {
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
      for (j = 0; j < g_apScriptedMobEntries[i]->nPoints; j++) {
        ProjectWorldPointGTE((int *)&g_aMobRingScreenPts[j],
                             g_apScriptedMobEntries[i]->pts[j], maxX);
        AddVector(vCenter, vCenter, g_apScriptedMobEntries[i]->pts[j]);
      }
      vCenter[0] /= g_apScriptedMobEntries[i]->nPoints;
      vCenter[1] /= g_apScriptedMobEntries[i]->nPoints;
      vCenter[2] /= g_apScriptedMobEntries[i]->nPoints;
      SubtractVector(vDelta, g_anCameraPos, g_apScriptedMobEntries[i]->pts[1]);
      RShiftVector3(vDelta, maxX);

      facing = -vDelta[0] * g_apScriptedMobEntries[i]->nx -
               vDelta[1] * g_apScriptedMobEntries[i]->ny -
               vDelta[2] * g_apScriptedMobEntries[i]->nz;
      minX = 0x13f8;
      minY = 0xf0;
      maxY = 0;
      flags = 0;
      j = 0;
      nEdges = 0;
      maxX = 0;
      g_nActorMeshOtDepthBin = 0;
      sumX = 0;
      sumY = 0;
      g_pActorDeferredTriStackTop = g_anActorDeferredTriStack;

      for (; j < g_apScriptedMobEntries[i]->nPoints; j++) {
        register int k asm("$5");

        if (facing > 0) {
          k = j + 1;
          if (k == g_apScriptedMobEntries[i]->nPoints) {
            k = 0;
          }
        } else {
          k = j - 1;
          if (k < 0) {
            {
              register int n asm("$2") = g_apScriptedMobEntries[i]->nPoints;

              k = n - 1;
            }
          }
        }
        sumX += g_aMobRingScreenPts[k].x;
        sumY += g_aMobRingScreenPts[k].y;
        g_nActorMeshOtDepthBin += g_aMobRingScreenPts[k].z;
        if ((unsigned int)(g_aMobRingScreenPts[k].x - 1) < 0x1ff &&
            (unsigned int)(g_aMobRingScreenPts[k].y - 1) < 0xef &&
            g_aMobRingScreenPts[k].z > 0) {
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
        if ((g_aMobRingScreenPts[j].x < 0x200 || xk < 0x200) &&
            (g_aMobRingScreenPts[j].x > 0 || xk > 0) &&
            (g_aMobRingScreenPts[j].y < 0xf0 || yk < 0xf0) &&
            (g_aMobRingScreenPts[j].y > 0 || yk > 0)) {
          dx = g_aMobRingScreenPts[j].x - xk;
          if (dx < 0) {
            dx = -dx;
          }
          if (dx >= 3) {
            flagX = 0;
          }
          dy = g_aMobRingScreenPts[j].y - yk;
          __asm__ volatile("" : : "r"(yk));
          if (dy < 0) {
            dy = -dy;
          }
          if (dy >= 3) {
            flagY = 0;
          }
          g_aMobSilhouetteEdges[nEdges].live = 1;
          g_aMobSilhouetteEdges[nEdges].p0 =
              (g_aMobRingScreenPts[j].y << 16) |
              (unsigned short)g_aMobRingScreenPts[j].x;
          g_aMobSilhouetteEdges[nEdges].p1 =
              (KPT.y << 16) | (unsigned short)KPT.x;
          g_aMobSilhouetteEdges[nEdges].nx = g_aMobRingScreenPts[j].y - KPT.y;
          g_aMobSilhouetteEdges[nEdges].ny = KPT.x - g_aMobRingScreenPts[j].x;
          g_aMobSilhouetteEdges[nEdges].c =
              -g_aMobSilhouetteEdges[nEdges].nx * g_aMobRingScreenPts[j].x -
              g_aMobSilhouetteEdges[nEdges].ny * g_aMobRingScreenPts[j].y;
          nEdges++;
        }
      }

      if ((flagX == 0 && flagY == 0) || nEdges == 0) {
        {
          register int np asm("$2") = g_apScriptedMobEntries[i]->nPoints;

          sumX /= np;
          g_nActorMeshOtDepthBin = (g_nActorMeshOtDepthBin / np) >> 7;
          sumY /= np;
        }
        if (g_nActorMeshOtDepthBin >= 0x100) {
          g_nActorMeshOtDepthBin += 0x40;
        }
        if (g_nActorMeshOtDepthBin >= 0x800) {
          g_nActorMeshOtDepthBin = 0x7ff;
        }
        if ((minX < 0x200 && minY < 0xf0 && maxX > 0 && maxY > 0 &&
             g_nActorMeshOtDepthBin > 0 && nEdges != 0) ||
            (minX <= 0 && minY <= 0 && maxX >= 0x200 && maxY >= 0xf0 &&
             g_nActorMeshOtDepthBin > 0)) {
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
              ApplyEulerRotation(euler, &eulerMtx, 0);
              RotateVectorByMatrix(&eulerMtx, vAxis, vAxis);
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
          g_anActorScissorRect[0] = minY << 16;
          g_anActorScissorRect[1] = maxY << 16;
          g_anActorScissorRect[2] = maxX << 16;
          g_anActorScissorRect[3] = minX << 16;
          g_aMobSilhouetteEdges[nEdges].live = 0;
          if (dist < 0x4000) {
            if (dist >= 0x3001) {
              g_nFadeProgress = dist - 0x3000;
              g_dwActorMeshFadeColor = ComputeDepthCuedColor(
                  *(unsigned int *)((char *)g_apScriptedMobEntries[i]->pMesh +
                                    0x10),
                  color, dist - 0x3000);
              EmitDynamicActorMeshFogged(&g_dwActorMeshFadeColor);
            } else {
              EmitDynamicActorMeshFogged(
                  &g_apScriptedMobEntries[i]->pMesh->color);
            }
          } else {
            EmitDynamicActorMeshFogged(&color);
          }
        }

        if (dist < 0x4000) {
          minX = 0x200;
          maxX = 0;
          minY = 0xf0;
          maxY = 0;
          flags = 0;
          g_pActorDeferredTriStackTop = g_anActorDeferredTriStack;
          nEdges = 0;
          for (j = 0; j < g_apScriptedMobEntries[i]->nPoints; j++) {
            int t;

            t = g_aMobRingScreenPts[j].x;
            if (sumX < t) {
              g_aMobRingScreenPts[j].x = t + 2;
            } else if (t < sumX) {
              g_aMobRingScreenPts[j].x = t - 2;
            }
            t = g_aMobRingScreenPts[j].y;
            if (sumY < t) {
              g_aMobRingScreenPts[j].y = t + 2;
            } else if (t < sumY) {
              g_aMobRingScreenPts[j].y = t - 2;
            }
            t = g_aMobRingScreenPts[j].x;
            if (maxX < t) {
              maxX = t;
            }
            if (t < minX) {
              minX = t;
            }
            t = g_aMobRingScreenPts[j].y;
            if (maxY < t) {
              maxY = t;
            }
            if (t < minY) {
              minY = t;
            }
          }

          for (j = 0; j < g_apScriptedMobEntries[i]->nPoints; j++) {
            register int k asm("$5");

            if (facing > 0) {
              k = j + 1;
              if (k == g_apScriptedMobEntries[i]->nPoints) {
                k = 0;
              }
            } else {
              k = j - 1;
              if (k < 0) {
                {
                  register int n asm("$2") = g_apScriptedMobEntries[i]->nPoints;

                  k = n - 1;
                }
              }
            }
            if ((unsigned int)(g_aMobRingScreenPts[k].x - 1) < 0x1ff &&
                (unsigned int)(g_aMobRingScreenPts[k].y - 1) < 0xef &&
                g_aMobRingScreenPts[k].z > 0) {
              flags = 1;
            }
            if ((g_aMobRingScreenPts[j].x < 0x200 ||
                 g_aMobRingScreenPts[k].x < 0x200) &&
                (g_aMobRingScreenPts[j].x > 0 ||
                 g_aMobRingScreenPts[k].x > 0) &&
                (g_aMobRingScreenPts[j].y < 0xf0 ||
                 g_aMobRingScreenPts[k].y < 0xf0) &&
                (g_aMobRingScreenPts[j].y > 0 ||
                 g_aMobRingScreenPts[k].y > 0)) {
              g_aMobSilhouetteEdges[nEdges].live = 1;
              g_aMobSilhouetteEdges[nEdges].p0 =
                  (g_aMobRingScreenPts[j].y << 16) |
                  (unsigned short)g_aMobRingScreenPts[j].x;
              g_aMobSilhouetteEdges[nEdges].p1 =
                  (g_aMobRingScreenPts[k].y << 16) |
                  (unsigned short)g_aMobRingScreenPts[k].x;
              g_aMobSilhouetteEdges[nEdges].nx =
                  g_aMobRingScreenPts[j].y - g_aMobRingScreenPts[k].y;
              g_aMobSilhouetteEdges[nEdges].ny =
                  g_aMobRingScreenPts[k].x - g_aMobRingScreenPts[j].x;
              g_aMobSilhouetteEdges[nEdges].c =
                  -g_aMobSilhouetteEdges[nEdges].nx * g_aMobRingScreenPts[j].x -
                  g_aMobSilhouetteEdges[nEdges].ny * g_aMobRingScreenPts[j].y;
              nEdges++;
            }
          }

          if ((minX < 0x200 && minY < 0xf0 && maxX > 0 && maxY > 0 &&
               g_nActorMeshOtDepthBin > 0 && nEdges != 0) ||
              (minX <= 0 && minY <= 0 && maxX >= 0x200 && maxY >= 0xf0 &&
               g_nActorMeshOtDepthBin > 0)) {
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
            ApplyEulerRotation(euler2, &eulerMtx2, 0);
            RotateVectorByMatrix(&eulerMtx2, vAxis2, vAxis2);
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
            g_anActorScissorRect[0] = minY << 16;
            g_anActorScissorRect[1] = maxY << 16;
            g_anActorScissorRect[2] = maxX << 16;
            g_anActorScissorRect[3] = minX << 16;
            g_aMobSilhouetteEdges[nEdges].live = 0;
            if (dist >= 0x3001) {
              g_nFadeProgress = dist - 0x3000;
              g_dwActorMeshFadeColor = ComputeDepthCuedColor(
                  *(unsigned int *)((char *)g_apScriptedMobEntries[i]->pMesh +
                                    0x10),
                  color, dist - 0x3000);
              EmitDynamicActorMeshFogShaded(
                  (int *)g_apScriptedMobEntries[i]->pMesh, &viewMtx, &outMtx);
            } else {
              EmitDynamicActorMeshScissored(
                  (int *)g_apScriptedMobEntries[i]->pMesh, &viewMtx, &outMtx);
            }
          }
        }
      }
    }
  }

  lod = g_nCameraFloorSurfaceType[0];
  if (lod >= g_nStaticMeshLodCutoff) {
    lod = -1;
  }
  EmitStaticActorMeshList(lod, (MATRIX *)&g_nCameraFloorSurfaceType[-0x10],
                          (MATRIX *)&g_nCameraFloorSurfaceType[-0x15]);
}
