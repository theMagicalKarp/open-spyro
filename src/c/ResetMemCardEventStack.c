#include "globals.h"

extern int D_800751B0;

/* Resets the memory-card event-stack top to empty (-1). (0x80068f30, 16 bytes.)
 */
void ResetMemCardEventStack(void) { D_800751B0 = -1; }
