#include "globals.h"
#include "pad.h"

/* Reset a PadState's per-frame input (seen as an int array): pressed /
   released / held cleared, the analog word centred, both idle flags raised;
   then each of the four ring entries gets the pad type, cleared buttons and a
   centred analog word (its stickActive is left alone). */
void ResetPadFrameSubstepTable(int *base) {
  int i;

  base[PAD_STATE_WORD(analog)] = PAD_ANALOG_CENTERED;
  base[PAD_STATE_WORD(held)] = 0;
  base[PAD_STATE_WORD(pressed)] = 0;
  base[PAD_STATE_WORD(released)] = 0;
  base[PAD_STATE_WORD(allInputIdle)] = 1;
  base[PAD_STATE_WORD(directionalIdle)] = 1;
  for (i = 0; i < 4; i++) {
    int t = base[PAD_STATE_WORD(padType)];
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(held)] = 0;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(pressed)] = 0;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(released)] = 0;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(analog)] = PAD_ANALOG_CENTERED;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(padType)] = t;
  }
}
