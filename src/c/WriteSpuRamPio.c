#include "globals.h"

extern void DelaySpuRegisterWrite(void);
extern int WritePrintf();

extern char *D_80073554;          /* SPU register block base */
extern unsigned short D_8007356C; /* SPU transfer-addr shadow */
extern char D_80011534[];         /* timeout format string */
extern char D_80011554[];
extern char D_80011568[];

#define SPUREG(off) (*(volatile unsigned short *)(D_80073554 + (off)))

/* SPU RAM PIO upload (0x8005be88, 460 bytes): latch the transfer address,
   then push the buffer through the manual-write FIFO (+0x1A8) in 0x40-byte
   bursts — kick each burst via SPUCNT mode 0x10, spin (max 0xF00) on the
   busy bit — and finally clear the transfer mode and wait for the
   controller's address counter to land on the latched target. */
void WriteSpuRamPio(unsigned short *srcArg, unsigned int lenArg) {
  unsigned int len;
  unsigned short *src;
  unsigned int stat;
  unsigned short saved;
  unsigned int cnt;
  unsigned int t;
  int chunk;
  int i;
  unsigned int count;
  int pad[2];

  len = lenArg;
  src = srcArg;
  stat = SPUREG(0x1AE);
  SPUREG(0x1A6) = D_8007356C;
  saved = stat & 0x7FF;
  DelaySpuRegisterWrite();
  if (len != 0) {
    do {
      chunk = 0x40;
      if (len < 0x41) {
        chunk = len;
      }
      i = 0;
      if (chunk > 0) {
        char *fifo = D_80073554;
        do {
          *(volatile unsigned short *)(fifo + 0x1A8) = *src++;
          i += 2;
        } while (i < chunk);
      }
      cnt = *(unsigned short *)(D_80073554 + 0x1AA);
      t = cnt & 0xFFCF;
      cnt = t | 0x10;
      SPUREG(0x1AA) = cnt;
      DelaySpuRegisterWrite();
      count = 0;
      while (SPUREG(0x1AE) & 0x400) {
        count += 1;
        if (count >= 0xF01) {
          WritePrintf(D_80011534, D_80011554);
          break;
        }
      }
      DelaySpuRegisterWrite();
      len -= chunk;
      DelaySpuRegisterWrite();
    } while (len != 0);
  }
  count = 0;
  cnt = *(unsigned short *)(D_80073554 + 0x1AA);
  cnt &= 0xFFCF;
  SPUREG(0x1AA) = cnt;
  while ((SPUREG(0x1AE) & 0x7FF) != saved) {
    count += 1;
    if (count >= 0xF01) {
      WritePrintf(D_80011534, D_80011568);
      break;
    }
  }
}
