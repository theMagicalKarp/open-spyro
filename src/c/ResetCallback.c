#include "globals.h"

extern void FUN_8005e4ac(void *base, int words);
extern int setjmp(void *buf);
extern void HandleHardwareInterrupt(void);
extern void HookEntryInt(void *buf);
extern void *InitRootCounterCallbacks(void);
extern void *InitDmaCallbackTable(void);
/* K&R decl on purpose: the vtable pointer is passed to this BIOS _96_remove
   trampoline (which ignores $a0) so the second g_pLibapiSysVtable load gets a
   copy-to-hard-reg suggestion and lands in $a0 as the original has it. */
extern void Bios96Remove();
extern void ExitCriticalSection(void);

extern unsigned short g_anLibapiCallbackBlock[]; /* 0x80073924 */
extern unsigned char g_abLibapiPatchBlock[];     /* 0x80073960, held-base view
                                                    of the exception-patch
                                                    pointer and the state
                                                    block below it */

/* PSY-Q ResetCallback equivalent — libapi master init for the interrupt and
   callback subsystem, guarded by the install-once flag at +0. Masks every IRQ
   (I_MASK = 0, then I_STAT = the read-back), enables all DMA channels at
   priority 3, zeroes the 0x41a-word callback registry, arms the
   unexpected-interrupt longjmp landing, hooks the BIOS exception entry, and
   publishes the root-counter and DMA master handlers into the libapi system
   vtable. Returns the flag's address on the first call, 0 on re-entry.
   (0x8005df60, 220 bytes.) */
unsigned short *ResetCallback(void) {
  unsigned short *ret;

  if (g_anLibapiCallbackBlock[0] == 0) {
    volatile short *istat = (volatile short *)g_pIStatReg;
    volatile unsigned short *imask = (volatile unsigned short *)g_pIMaskReg;

    *imask = 0;
    *istat = *imask;
    *(volatile unsigned int *)g_pDmaDpcrReg = 0x33333333;
    FUN_8005e4ac(g_anLibapiCallbackBlock, 0x41A);
    if (setjmp(&g_anLibapiCallbackBlock[0x1C]) != 0) {
      HandleHardwareInterrupt();
    }
    *(void **)g_abLibapiPatchBlock = &g_abLibapiPatchBlock[0xFDC];
    HookEntryInt(&g_abLibapiPatchBlock[-4]);
    *(short *)&g_abLibapiPatchBlock[-0x3C] = 1;
    ((void **)g_pLibapiSysVtable)[5] = InitRootCounterCallbacks();
    ((void **)g_pLibapiSysVtable)[1] = InitDmaCallbackTable();
    Bios96Remove(g_pLibapiSysVtable);
    ExitCriticalSection();
    ret = (unsigned short *)&g_abLibapiPatchBlock[-0x3C];
  } else {
    ret = 0;
  }
  return ret;
}
