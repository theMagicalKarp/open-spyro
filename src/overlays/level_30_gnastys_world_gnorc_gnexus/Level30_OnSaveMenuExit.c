/* Level30_OnSaveMenuExit (0x8007AF4C, level_30_gnastys_world_gnorc_gnexus overlay).
 *
 * Level hook run when leaving the save menu back to the world: forwards to
 * EndSaveMenuToWorld (EndSaveMenuToWorld), clears the level's pending-action word
 * (g_nSpyroSwingEulerPrev1) and flag byte (g_bSpyroSwingByteEuler1), then if the tracked actor
 * (g_pLevelOverlayInitRecord) exists, resets its +0x50 mode byte to 0x10.
 * Identical body in ten level overlays.
 */

#include "level_globals.h"
#include "level_funcs.h"
#include "level_actors.h"


void Level30_OnSaveMenuExit(void) {
  char *actor;

  EndSaveMenuToWorld();
  actor = g_pLevelOverlayInitRecord;
  g_nSpyroSwingEulerPrev1 = 0;
  g_bSpyroSwingByteEuler1 = 0;
  if (actor != 0) {
    actor[0x50] = 0x10;
  }
}
