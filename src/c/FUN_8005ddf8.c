#include "globals.h"

/* Dispatch the libapi system-vtable method at +0x8 (0x8005ddf8, 48 bytes). */
void FUN_8005ddf8(void) {
  (*(void (**)(void))((char *)g_pLibapiSysVtable + 0x8))();
}
