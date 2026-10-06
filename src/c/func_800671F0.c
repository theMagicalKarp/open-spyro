#include "globals.h"

extern int WritePrintf();
extern void FormatMemCardPath(int chan, char *path);
extern char *strcat(char *dst, char *src);
extern void RegisterMemCardEvent(void (*handler)());
extern void PollMemCardEvents(void);
extern int GetMemCardEventStatusSlot1(void);
extern int WaitMemCardEventSlot1(void);
extern int MapMemCardAccessCode(int code);
extern int MemCardSync(int mode, int *cmd, int *result);
extern int func_80067614(int val);
extern void func_80066634();
extern int open(char *path, unsigned int flags);
extern int lseek(int fd, int offset, int whence);
extern int write(int fd, void *buf, int bytes);
extern int close(int fd);
extern void D_800663D8();

/* BIOS directory entry (40 bytes). */
typedef struct {
  int w[10];
} McDirEntry;

extern McDirEntry *firstfile(char *path, McDirEntry *dir);
extern McDirEntry *nextfile(McDirEntry *dir);

extern int D_80075194;            /* write retry count */
extern int D_80075B4C;            /* per-port card-acknowledged mask */
extern volatile int D_80075B50[]; /* [0] op in progress */
extern int D_80075B54[];          /* [0] op result code, +0x1c path buffer */
extern int D_80075B58;            /* op-complete flag */
extern int D_80075B5C;            /* port of the current op */
extern int D_80075B60;            /* fd; then file offset, byte count, src */
extern int D_80075BA0;            /* saved callback-suspend state */
extern char D_80011F54[];         /* "already busy" trace */
extern char D_800120A4[];         /* "card busy" trace */

/* libmcrd _card_write event handler (0x800671f0, 432 bytes), registered by
   MemCardWrite. Same steps as the read handler func_80066F34 (arm events,
   open, seek, write, wait with up to 16 retries on result 1), issuing a
   write() instead. Returns 1 when the op is finished. */
int func_800671F0(int *state) {
  int *res;
  int *f;
  int fd;
  int code;

  switch (*state) {
  case 0:
    D_80075194 = 0;
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
    while (write(f[0], (void *)f[3], f[2]) != 0) {
    }
    *state = 30;
    break;
  case 30:
    if (GetMemCardEventStatusSlot1() == 0) {
      return 0;
    }
    code = WaitMemCardEventSlot1();
    if (code == 1 && ++D_80075194 < 16) {
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

/* libmcrd MemCardGetDirentry (0x800673a0, 628 bytes; unlabelled in the asm,
   so it shares this slot). Lists "bu<port>:<name>" entries, skipping the
   first `ofs` and copying up to `max` more into `dir`; if the first lookup
   finds nothing, a card load is forced (busy-checked inline) and the scan
   retried up to 16 times. Stores the copied count in *files; returns 0, -1
   if an op is already pending, or the failing load result. */
int func_800673A0(int port, char *name, McDirEntry *dir, int *files, int ofs,
                  int max) {
  char path[0x20];
  McDirEntry de;
  int result;
  int tries;
  int i;
  int n;
  McDirEntry *ent;

  if (D_80075B50[0] != 0) {
    WritePrintf(D_800120A4);
    return -1;
  }
  FormatMemCardPath(port, path);
  strcat(path, name);
  tries = 0;
  i = 0;
  D_80075B4C |= 1 << D_80075B5C;
  result = 0;
  n = 0;
  for (; i < ofs + max; i++) {
    if (i == 0) {
      while ((ent = firstfile(path, &de)) == 0) {
        D_80075BA0 = func_80067614(0);
        if (D_80075B50[0] != 0) {
          WritePrintf(D_80011F54);
        } else {
          D_80075B50[0] = 2;
          D_80075B54[0] = 0;
          D_80075B58 = 0;
          D_80075B5C = port;
          RegisterMemCardEvent(func_80066634);
        }
        MemCardSync(0, 0, &result);
        func_80067614(D_80075BA0);
        if (result == 0) {
          break;
        }
        if (++tries >= 16) {
          return result;
        }
      }
      if (ent == 0) {
        break;
      }
    } else if (nextfile(&de) == 0) {
      break;
    }
    if (i >= ofs && dir != 0) {
      dir[n] = de;
      n++;
    }
  }
  if (files != 0) {
    *files = n;
  }
  return 0;
}
