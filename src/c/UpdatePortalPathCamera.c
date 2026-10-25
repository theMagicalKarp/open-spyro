#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* The path block is re-read from the global on every use (the overlay can
   swap it out under us). */
#define PATH (*(unsigned char *volatile *)&g_pPortalCamPathData)
#define PATHDATA ((unsigned char *)g_pPortalCamPathData)

/* Camera updater for mode 0x8000000b (portal spline path). Walks
   g_anCameraPos along the overlay-provided path at g_pPortalCamPathData:
   count byte [0], current index byte [1], waypoint vec3s at +8 + i*0x10.
   Within 0x200 of the current waypoint the index advances (forward) or
   retreats (g_nPortalCamPathReverseFlag). Speed: while an onward waypoint
   exists, dot(camera->player excess beyond 0x400, segment tangent normalised
   to 0x1000) << 7 >> 12; at the path ends the step is capped by
   g_nPortalCamPathMaxStep. (0x8003740c, 776 bytes.) */
void UpdatePortalPathCamera(void) {
  VECTOR tangent;
  VECTOR toPlayer;
  VECTOR toWaypoint;
  unsigned char *path;
  int idx;
  int dist;
  int span;
  int step;
  int quot;

  idx = PATH[1];
  SubtractVector(&toWaypoint, PATH + (idx * 0x10 + 8), g_anCameraPos);
  dist = VectorLength(&toWaypoint, 1);
  if (dist < 0x200) {
    if (g_nPortalCamPathReverseFlag != 0) {
      path = PATH;
      if (path[1] != 0) {
        path[1] = path[1] - 1;
      }
    } else {
      path = PATH;
      if (path[1] < path[0] - 1) {
        path[1] = path[1] + 1;
      }
    }
  }

  step = 0;
  if (g_nPortalCamPathReverseFlag != 0) {
    step = idx < PATH[0] - 1;
  } else if (idx != 0) {
    step = 1;
  }

  if (step != 0) {
    SubtractVector(&toPlayer, g_anSpyroWorldPos, g_anCameraPos);
    span = VectorLength(&toPlayer, 1);
    step = 0;
    if (span > 0x400) {
      step = (span - 0x400) * 4;
      if (span < step) {
        step = span;
      }
      ScaleVector3ByRatio(&toPlayer, span, step);
      span = step;
      if (g_nPortalCamPathReverseFlag != 0) {
        SubtractVector(&tangent, PATHDATA + (idx * 0x10 + 8),
                       PATHDATA + (idx * 0x10 + 0x18));
      } else {
        SubtractVector(&tangent, PATHDATA + (idx * 0x10 + 8),
                       PATHDATA + (idx * 0x10 - 8));
      }
      SetVector3Magnitude(&tangent, 0x1000);
      quot = (tangent.vx * toPlayer.vx + tangent.vy * toPlayer.vy +
              tangent.vz * toPlayer.vz) /
             span;
      step = abs(quot);
      if (step < 0) {
        step = 0;
      }
      if (step > 0x1000) {
        step = 0x1000;
      }
      step = (step << 7) >> 12;
    }
  } else {
    if (g_nPortalCamPathMaxStep < (step = dist)) {
      step = g_nPortalCamPathMaxStep;
    }
  }

  span = dist;
  ScaleVector3ByRatio(&toWaypoint, span, step * g_nFrameStep);
  AddVector(g_anCameraPos, g_anCameraPos, &toWaypoint);
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
  UpdateCameraEulerAngles();
}
