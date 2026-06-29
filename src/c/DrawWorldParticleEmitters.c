#include "globals.h"

extern void DrawWorldSparkleParticles(void);
extern void DrawWorldRotatingBillboards(int frameStep);

/* Draws the per-level world particle effects: the sparkle particles followed by
   the rotating billboards (advanced by g_nFrameStep). (0x80058ba8, 48 bytes.)
 */
void DrawWorldParticleEmitters(void) {
  DrawWorldSparkleParticles();
  DrawWorldRotatingBillboards(g_nFrameStep);
}
