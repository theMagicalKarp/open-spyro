#include "globals.h"

/* Dispatch the libapi system-vtable method at +0x10. */
void func_8005DEBC(void) {
  (*(void (**)(void))((char *)g_pLibapiSysVtable + 0x10))();
}
