#include "globals.h"

extern void EnterCriticalSection(void);
extern void ResetEntryInt(void);

extern unsigned short g_anLibapiCallbackStateBlock[];

/* Tear down the libapi interrupt callback dispatch (0x8005e378): if it is
   installed, mask every interrupt (saving IMASK and the DMA DPCR so
   func_8005E44C can restore them), acknowledge what was pending, drop the DMA
   enable bits, release the BIOS entry-int hook and clear the installed flag.
   Returns the state block (NULL when nothing was installed). */
unsigned short *func_8005E378(void) {
  unsigned short *state = g_anLibapiCallbackStateBlock;
  volatile unsigned short *imask;
  volatile unsigned short *istat;

  if (*state == 0) {
    return 0;
  }
  EnterCriticalSection();
  imask = (volatile unsigned short *)g_pIMaskReg;
  istat = (volatile unsigned short *)g_pIStatReg;
  g_nLibapiSavedIMask = *imask;
  g_dwLibapiSavedDmaDpcr = *(volatile unsigned int *)g_pDmaDpcrReg;
  *imask = 0;
  *istat = *imask;
  *(volatile unsigned int *)g_pDmaDpcrReg &= 0x77777777;
  ResetEntryInt();
  *state = 0;
  return state;
}
