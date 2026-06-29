#include "globals.h"

extern void InitSpuSubsystem(int mode);

/* PSY-Q SpuInit shim: bring up the SPU subsystem with the default (0) flags.
   (0x8005ba60, 32 bytes.) */
void InitSpu(void) { InitSpuSubsystem(0); }
