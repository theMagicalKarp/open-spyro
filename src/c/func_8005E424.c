#include "globals.h"

extern void HookEntryInt();
extern void ExitCriticalSection(void);
extern unsigned short g_anLibapiCallbackBlock[]; /* libapi ctx: +0 installed
                                                    flag, +0x38 jmp_buf */

/* PSY-Q RestartCallback equivalent. If the libapi callback subsystem is not
   installed: re-hook the BIOS exception entry (jmp_buf lives at +0x38 from
   the install flag), set the flag, restore the saved I_MASK and DMA DPCR
   hardware registers, leave the critical section, and return the flag's
   address. Returns 0 when already installed. (0x8005e424, 136 bytes.) */
unsigned short *func_8005E424(void) {
  unsigned short *ret;

  if (g_anLibapiCallbackBlock[0] == 0) {
    HookEntryInt(&g_anLibapiCallbackBlock[0x1c]);
    g_anLibapiCallbackBlock[0] = 1;
    *(short *)g_pIMaskReg = g_nLibapiSavedIMask;
    *(volatile unsigned int *)g_pDmaDpcrReg = g_dwLibapiSavedDmaDpcr;
    ExitCriticalSection();
    ret = g_anLibapiCallbackBlock;
  } else {
    ret = 0;
  }
  return ret;
}
