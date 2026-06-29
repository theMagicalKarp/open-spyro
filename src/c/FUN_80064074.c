#include "globals.h"

extern int CdDataSync(int mode);

/* libcd: thin thunk forwarding to CdDataSync (0x80064074, 32 bytes). */
int FUN_80064074(int mode) { return CdDataSync(mode); }
