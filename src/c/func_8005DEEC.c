#include "globals.h"

/* Dispatch the libapi system-vtable method at +0x18. */
void func_8005DEEC(void) {
  (*(void (**)(void))((char *)g_pLibapiSysVtable + 0x18))();
}
