#include "globals.h"

/* Dispatch the libapi system-vtable method at +0xC (0x8005ddc8, 48 bytes). */
void FUN_8005ddc8(void) {
  (*(void (**)(void))((char *)g_pLibapiSysVtable + 0xC))();
}
