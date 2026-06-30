#include "globals.h"

extern int D_80078C40; /* layer-2 active-anim cancel flag */
extern int D_80078C44; /* layer-2 permissive-duration frame counter */

/* Layer-2 anim gate timer. The per-anim blockage table at
   g_abSpyroAnimDescTable+0xB8 marks anims that hand the bodypart to the primary
   layer; if neither the previous nor the current Spyro anim blocks layer 2, the
   permissive-duration counter advances, otherwise the active layer-2 anim is
   force-cancelled. (0x80049f3c, 112 bytes.) */
void TickSpyroAnimLayer2GateTimer(void) {
  if (g_abSpyroAnimDescTable[0xB8 + g_bSpyroAnimPrev] ||
      g_abSpyroAnimDescTable[0xB8 + g_bSpyroAnimCurrent]) {
    D_80078C40 = 0;
  } else {
    D_80078C44++;
  }
}
