#include "globals.h"

extern void FUN_8005e8ac(void *table, int count);
extern void FUN_8005ddf8(int slot, void *handler);
extern void func_8005E680(void);
extern void func_8005E804(void);

/* libapi DMA callback-table init: zero the 8-entry DMA callback table, clear
   the DMA interrupt control register (DICR), commit the table to the master
   interrupt vector, and return the DMA master IRQ handler. (0x8005e630,
   80 bytes.) */
void *InitDmaCallbackTable(void) {
  FUN_8005e8ac(g_apfnDmaCallbacks, 8);
  *(volatile int *)g_pDmaDicrReg = 0;
  FUN_8005ddf8(3, func_8005E680);
  return (void *)func_8005E804;
}
