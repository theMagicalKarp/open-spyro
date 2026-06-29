#include "globals.h"

/* Install `cb` as the VSync (root-counter 4) callback by dispatching the libapi
   system-vtable method at +0x14 with (counter=4, cb) (0x8005de58, 52 bytes). */
void InstallVSyncCallback(void *cb) {
  (*(void (**)(int, void *))((char *)g_pLibapiSysVtable + 0x14))(4, cb);
}
