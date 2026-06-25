#include "globals.h"

/* Empty substate handler for gamestate 6 — a do-nothing slot in the substate
   dispatch table. Compiles to the bare `jr $ra; nop` at 0x8002C91C. */
void Gamestate06SubstateStub_Noop(void) {}
