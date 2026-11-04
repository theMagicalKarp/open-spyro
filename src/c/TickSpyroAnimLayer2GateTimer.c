#include "globals.h"
#include "main_shared.h"
#include "spyro_state.h"

/* Layer-2 anim gate timer. SPYRO_ANIM_NO_OVERLAY (the per-anim bytes after
   the g_abSpyroAnimDescTable records) marks anims that keep the overlay
   layers off; if neither the previous nor the current Spyro anim blocks layer
   2, the permissive-duration counter advances, otherwise the active layer-2
   anim is force-cancelled. (0x80049f3c, 112 bytes.) */
void TickSpyroAnimLayer2GateTimer(void) {
  if (SPYRO_ANIM_NO_OVERLAY(g_bSpyroAnimPrev) ||
      SPYRO_ANIM_NO_OVERLAY(g_bSpyroAnimCurrent)) {
    g_nSpyroAnimLayer2Request = 0;
  } else {
    g_nSpyroAnimLayer2GateFrames++;
  }
}
