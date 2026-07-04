#include "globals.h"

extern void DeliverEvent(unsigned long class, unsigned long spec);

/* libcd CD callback trampolines installed by CdInit (sync/ready/read).
   Each simply delivers the completion event on the CD event class
   0xF0000003 with its own spec. (0x80063a14, 0x78 bytes across 3 funcs.) */
void func_80063A14(void) { DeliverEvent(0xF0000003, 0x20); }

void func_80063A3C(void) { DeliverEvent(0xF0000003, 0x40); }

void func_80063A64(void) { DeliverEvent(0xF0000003, 0x40); }
