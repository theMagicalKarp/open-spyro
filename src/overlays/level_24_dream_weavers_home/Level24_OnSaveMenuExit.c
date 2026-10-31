/* Level24_OnSaveMenuExit (0x8007B9D4, level_24_dream_weavers_home overlay).
 *
 * Level hook run when leaving the save menu back to the world: forwards to
 * EndSaveMenuToWorld, zeroes the second swing-euler spring component
 * (g_nSpyroSwingEulerPrev1 / g_bSpyroSwingByteEuler1, which EndSaveMenuToWorld
 * had just seeded), and, when Sparx is present (g_pSparxActor), restores its
 * cull radius to 0x10.
 * Identical body in ten level overlays.
 */

#include "level_globals.h"
#include "level_funcs.h"
#include "level_actors.h"
#include "actor_type.h"


void Level24_OnSaveMenuExit(void) {
  Actor *actor;

  EndSaveMenuToWorld();
  actor = g_pSparxActor;
  g_nSpyroSwingEulerPrev1 = 0;
  g_bSpyroSwingByteEuler1 = 0;
  if (actor != 0) {
    actor->cullRadius = 0x10;
  }
}
