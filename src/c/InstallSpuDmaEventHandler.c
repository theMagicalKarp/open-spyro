#include "globals.h"

extern void EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern long OpenEvent(unsigned long desc, long spec, long mode,
                      void (*handler)());
extern long EnableEvent(long ev);
extern void InstallSpuDmaCallback();
extern void func_8005C054();

/* One-shot SPU DMA completion plumbing: installs the channel-4 DMA callback
   and opens/enables the SPU-DMA event (0xF0000009) whose handle
   SpuIsTransferCompleted polls. Guarded by an installed flag so re-init is a
   no-op. (0x8005bb78, 124 bytes.) */
void InstallSpuDmaEventHandler(void) {
  if (g_nSpuDmaEventInstalledFlag == 0) {
    g_nSpuDmaEventInstalledFlag = 1;
    EnterCriticalSection();
    InstallSpuDmaCallback(func_8005C054);
    g_nSpuDmaEventHandle = OpenEvent(0xF0000009, 0x20, 0x2000, 0);
    EnableEvent(g_nSpuDmaEventHandle);
    ExitCriticalSection();
  }
}
