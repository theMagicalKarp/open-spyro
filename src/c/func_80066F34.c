#include "globals.h"

extern void RegisterMemCardEvent(void (*handler)());
extern void PollMemCardEvents(void);
extern int GetMemCardEventStatusSlot1(void);
extern int WaitMemCardEventSlot1(void);
extern int MapMemCardAccessCode(int code);
extern int open(char *path, unsigned int flags);
extern int lseek(int fd, int offset, int whence);
extern int read(int fd, void *buf, int bytes);
extern int close(int fd);
extern void D_800663D8();

extern int D_80075190;   /* read retry count */
extern int D_80075B54[]; /* [0] op result code, +0x1c path buffer */
extern int D_80075B60;   /* fd; then file offset, byte count, dst */

/* libmcrd _card_read event handler (0x80066f34, 432 bytes), registered by
   MemCardRead. Steps the read op: 0 arm the BIOS card events, 10 open the file
   (result 5 if it cannot), 11 seek to the offset, 20 issue the read, 30 wait
   for completion; a retryable result (1) re-seeks up to 16 times, anything
   else maps to the op result code and closes the file. Returns 1 when the op
   is finished. */
int func_80066F34(int *state) {
  int *res;
  int *f;
  int fd;
  int code;

  switch (*state) {
  case 0:
    D_80075190 = 0;
    RegisterMemCardEvent(D_800663D8);
    *state = 10;
    break;
  case 10:
    res = D_80075B54;
    if (res[0] != 0) {
      return 1;
    }
    fd = open((char *)(res + 7), 0x8001);
    D_80075B60 = fd;
    if (fd < 0) {
      res[0] = 5;
      return 1;
    }
  case 11:
    f = &D_80075B60;
    while (lseek(f[0], f[1], 0) != f[1]) {
    }
    *state = 20;
    break;
  case 20:
    PollMemCardEvents();
    f = &D_80075B60;
    while (read(f[0], (void *)f[3], f[2]) != 0) {
    }
    *state = 30;
    break;
  case 30:
    if (GetMemCardEventStatusSlot1() == 0) {
      return 0;
    }
    code = WaitMemCardEventSlot1();
    if (code == 1 && ++D_80075190 < 16) {
      *state = 11;
      break;
    }
    D_80075B54[0] = MapMemCardAccessCode(code);
    close(D_80075B60);
    D_80075B60 = -1;
    return 1;
  }
  return 0;
}
