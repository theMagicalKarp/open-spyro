/* Level18_OnSaveMenuExit (0x8007B7A0, level_18_beast_makers_home overlay).
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


void Level18_OnSaveMenuExit(void) {
  char *actor;

  EndSaveMenuToWorld();
  actor = g_pSparxActor;
  g_nSpyroSwingEulerPrev1 = 0;
  g_bSpyroSwingByteEuler1 = 0;
  if (actor != 0) {
    ((Actor *)actor)->cullRadius = 0x10;
  }
}
