#include "globals.h"

/* PSY-Q DmaCallback: register a per-channel DMA-completion handler by
   dispatching the libapi system-vtable method at +0x4. The channel/callback
   arguments pass straight through in their registers (0x8005de28, 48 bytes). */
void DmaCallback(void) {
  (*(void (**)(void))((char *)g_pLibapiSysVtable + 0x4))();
}
