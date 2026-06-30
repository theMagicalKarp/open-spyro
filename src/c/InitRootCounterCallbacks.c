#include "globals.h"

extern void FUN_8005e604(void *table, int count);
extern void FUN_8005ddf8(int slot, void *handler);
extern void func_8005E560(void);
extern void func_8005E5D8(void);

/* libapi RCnt callback-table init: arm the HRETRACE timer (one-shot, IRQ on),
   clear the aux state, zero the 8-entry root-counter callback table, commit it
   to the master interrupt vector, and return the RCnt master IRQ handler.
   (0x8005e508, 88 bytes.) */
void *InitRootCounterCallbacks(void) {
  *(volatile int *)g_pTmrHretraceModeReg = 0x107;
  g_nRcntAuxState = 0;
  FUN_8005e604(g_apfnRcntCallbacks, 8);
  FUN_8005ddf8(0, func_8005E560);
  return (void *)func_8005E5D8;
}
