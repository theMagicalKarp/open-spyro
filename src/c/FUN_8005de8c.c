#include "globals.h"

/* Dispatch the libapi system-vtable method at +0x14 (0x8005de8c, 48 bytes). */
void FUN_8005de8c(void) {
  (*(void (**)(void))((char *)g_pLibapiSysVtable + 0x14))();
}
