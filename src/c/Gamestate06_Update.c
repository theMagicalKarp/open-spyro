#include "funcs.h"
#include "globals.h"

/* GS 6 per-frame update: just forwards to the substate no-op stub. */
void Gamestate06_Update(void) { Gamestate06SubstateStub_Noop(); }
