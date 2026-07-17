#include "globals.h"

/* SPU hardware bring-up (0x8005bbf4, 0x294). Unmasks the SPU DMA channel,
   zeroes the volume/control shadow block, waits (max 0xF00 spins) for the
   SPUSTAT busy bits to clear, then programs the address-alignment policy
   and clears the reverb/CD-input volumes. On a cold init (mode == 0) it
   also silences the key-on/noise/FM latches, uploads a 16-byte silent
   sample via WriteSpuRamPio, resets all 24 voice channels, and pulses the
   key-off mask twice. Leaves the controller in mode 0xC000 (enable +
   unmute) with the transfer-done flag set. */
extern void DelaySpuRegisterWrite(void);
extern int WritePrintf();
extern void WriteSpuRamPio(unsigned short *src, unsigned int len);

extern char *D_80073554;          /* SPU register block base */
extern int *D_80073564;           /* DMA interrupt-control register */
extern unsigned short D_8007356C; /* SPU transfer-addr shadow */
extern int D_80073570;
extern int D_80073574;
extern int D_8007358C;
extern int D_80073590;
extern unsigned short D_80073594; /* 16-byte silent sample block */
extern short D_800777A8[];        /* per-voice pending-pitch table */
extern char D_80011534[];         /* timeout format string */
extern char D_80011544[];

#define SPUREG(off) (*(volatile unsigned short *)(D_80073554 + (off)))

int InitSpuHardware(int mode) {
  int i;
  unsigned int count;
  short *p;
  int ffff;
  int ff;

  *D_80073564 |= 0xB0000;
  D_80073570 = 0;
  D_80073574 = 0;
  D_8007356C = 0;
  SPUREG(0x180) = 0;
  SPUREG(0x182) = 0;
  SPUREG(0x1AA) = 0;
  DelaySpuRegisterWrite();
  SPUREG(0x180) = 0;
  SPUREG(0x182) = 0;
  count = 0;
  while ((SPUREG(0x1AE) & 0x7FF) != 0) {
    count += 1;
    if (count >= 0xF01) {
      WritePrintf(D_80011534, D_80011544);
      break;
    }
  }
  i = 0;
  p = D_800777A8;
  g_nSpuAddrAlignEnable = 2;
  g_nSpuAddrShift = 3;
  g_nSpuAddrAlignUnit = 8;
  g_dwSpuAddrAlignMask = 7;
  SPUREG(0x1AC) = 4;
  SPUREG(0x184) = 0;
  SPUREG(0x186) = 0;
  SPUREG(0x18C) = 0xFFFF;
  SPUREG(0x18E) = 0xFFFF;
  SPUREG(0x198) = 0;
  SPUREG(0x19A) = 0;
fill:
  *p = 0;
  i += 1;
  p += 1;
  if (i < 0xA) {
    goto fill;
  }
  if (mode == 0) {
    volatile unsigned short *v;
    int cutoff;
    int mid;
    D_8007356C = 0x200;
    SPUREG(0x190) = 0;
    SPUREG(0x192) = 0;
    SPUREG(0x194) = 0;
    SPUREG(0x196) = 0;
    SPUREG(0x1B0) = 0;
    SPUREG(0x1B2) = 0;
    SPUREG(0x1B4) = 0;
    SPUREG(0x1B6) = 0;
    WriteSpuRamPio(&D_80073594, 0x10);
    i = 0;
    cutoff = 0x3FFF;
    mid = 0x200;
    v = (volatile unsigned short *)D_80073554;
    for (; i < 0x18; i += 1) {
      v[0] = 0;
      v[1] = 0;
      v[2] = cutoff;
      v[3] = mid;
      v[4] = 0;
      v[5] = 0;
      v += 8;
    }
    ffff = 0xFFFF;
    ff = 0xFF;
    SPUREG(0x188) = ffff;
    SPUREG(0x18A) = ff;
    DelaySpuRegisterWrite();
    DelaySpuRegisterWrite();
    DelaySpuRegisterWrite();
    DelaySpuRegisterWrite();
    SPUREG(0x18C) = ffff;
    SPUREG(0x18E) = ff;
    DelaySpuRegisterWrite();
    DelaySpuRegisterWrite();
    DelaySpuRegisterWrite();
    DelaySpuRegisterWrite();
  }
  g_nSpuTransferDoneFlag = 1;
  SPUREG(0x1AA) = 0xC000;
  D_8007358C = 0;
  D_80073590 = 0;
  return 0;
}
