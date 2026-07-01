#include "globals.h"

extern int WritePrintf(char *fmt, ...);
extern void FormatMemCardPath(int chan, char *path);
extern char *strcat(char *dst, char *src);
extern void RegisterMemCardEvent(void (*handler)());
extern void func_80066F34();
extern char D_80012080[];         /* "un-aligned size" error string */
extern char D_800120A4[];         /* "card busy" error string */
extern volatile int D_80075B50[]; /* memcard op state; +0x20 = path buffer */
extern volatile int D_80075B54;   /* memcard op result code */
extern volatile int D_80075B58;   /* memcard op-complete flag */
extern volatile int D_80075B5C;   /* pending-op file channel */
extern volatile int D_80075B64;   /* pending-op file offset */
extern volatile int D_80075B68;   /* pending-op byte count */
extern volatile int D_80075B6C;   /* pending-op memory address */

/* libmcrd _card_read: rejects if an op is pending or size is not a multiple
   of 0x80 (one BIOS sector); otherwise builds the device path, latches the
   request into the pending-op block (state=3), and registers func_80066F34 as
   the event handler. Returns 1 on accept, 0 on reject. Caller polls via
   MemCardSync. (0x80066e28, 268 bytes.) */
int MemCardRead(int chan, char *file, int adrs, int ofs, unsigned int bytes) {
  if (D_80075B50[0] != 0) {
    WritePrintf(D_800120A4);
    return 0;
  }
  if ((bytes & 0x7F) != 0) {
    WritePrintf(D_80012080);
    return 0;
  }
  FormatMemCardPath(chan, (char *)D_80075B50 + 0x20);
  strcat((char *)D_80075B50 + 0x20, file);
  D_80075B50[0] = 3;
  D_80075B54 = 0;
  D_80075B58 = 0;
  D_80075B64 = ofs;
  D_80075B6C = adrs;
  D_80075B68 = bytes;
  D_80075B5C = chan;
  RegisterMemCardEvent(func_80066F34);
  return 1;
}
