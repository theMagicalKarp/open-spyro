#include "globals.h"

extern void *CdSyncCallback(void *func);
extern void *CdReadyCallback(void *func);
extern void *FUN_80064050(void *func);
extern int VSync(int mode);
extern int CdStatus(void);
extern int CdControlBlocking(int cmd, void *p1, void *p2);
extern int CdReadStart(int retry);

extern volatile int D_80075154;   /* libcd read mode */
extern volatile int D_80075158;   /* bytes per sector */
extern int D_80075148[];          /* sectors to read */
extern void *volatile D_8007514C; /* destination buffer */
extern void *volatile D_8007516C; /* saved sync callback */
extern void *volatile D_80075170; /* saved ready callback */
extern volatile int D_80075178;   /* driver flags (bit 0 = DMA delivery) */
extern void *D_80075174;          /* saved DMA completion handler */
extern int D_80075164;            /* VSync stamp at read kick */

/* libcd CdRead (0x8006606c, 260 bytes): latch the read mode, sector size,
   count and buffer, park the sync/ready (and DMA) callbacks, pause the drive
   if it is busy, and start the read. Returns 1 when the read started.
   The libcd driver state is volatile: its stores keep their source order
   around the mode read-modify-write, and the count store goes through an
   inline volatile cast of the block alias (B16 register form). */
int CdRead(int sectors, void *buf, int mode) {
  int size;

  D_80075154 = mode;
  switch (D_80075154 & 0x30) {
  case 0:
    size = 0x200;
    break;
  case 0x20:
    size = 0x249;
    break;
  default:
    size = 0x246;
    break;
  }
  D_80075158 = size;
  D_80075154 |= 0x20;
  D_8007514C = buf;
  *(volatile int *)D_80075148 = sectors;
  D_8007516C = CdSyncCallback(0);
  D_80075170 = CdReadyCallback(0);
  if (D_80075178 & 1) {
    D_80075174 = FUN_80064050(0);
  }
  D_80075164 = VSync(-1);
  if (CdStatus() & 0xE0) {
    CdControlBlocking(9, 0, 0);
  }
  return CdReadStart(0) > 0;
}
