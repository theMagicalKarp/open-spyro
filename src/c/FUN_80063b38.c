#include "globals.h"

extern void CdResetController(void);

/* Sony SDK internal: pure thunk to CdResetController (0x80063b38, 32 bytes). */
void FUN_80063b38(void) { CdResetController(); }
