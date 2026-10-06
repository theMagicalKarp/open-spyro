#include "globals.h"

extern int WritePrintf();
extern void FormatMemCardPath(int chan, char *path);
extern char *strcat(char *dst, char *src);
extern void RegisterMemCardEvent(void (*handler)());
extern void PollMemCardEvents(void);
extern int GetMemCardEventStatusSlot1(void);
extern int WaitMemCardEventSlot1(void);
extern int GetMemCardEventStatusSlot2(void);
extern int WaitMemCardEventSlot2(void);
extern int MapMemCardAccessCode(int code);
extern void MemCardFormatBlocks(int chan);
extern int _card_load(int chan);
extern int MemCardSync(int mode, int *cmd, int *result);
extern int func_80067614(int val);
extern int open(char *path, unsigned int flags);
extern int lseek(int fd, int offset, int whence);
extern int read(int fd, void *buf, int bytes);
extern int write(int fd, void *buf, int bytes);
extern int close(int fd);
extern void D_800663D8();

extern int D_80075188;          /* data-read retry count */
extern int D_8007518C;          /* data-write retry count */
extern int D_80075B00;          /* load retry count */
extern int D_80075B04;          /* last load event result */
extern int D_80075B08;          /* card was (re)formatted during the load */
extern volatile int D_80075B50; /* op in progress */
extern volatile int D_80075B54; /* op result code */
extern int D_80075B54_blk[];    /* held-base view of D_80075B54 */
extern int D_80075B58;          /* op-complete flag */
extern int D_80075B5C;          /* port of the current op */
extern int D_80075B60[]; /* [0] fd, [1] offset, [2] byte count, [3] buffer,
                            +0x10 path */
extern int D_80075B64;
extern int D_80075B68;
extern int D_80075B6C;
extern int D_80075BA0;    /* saved callback-suspend state */
extern char D_80011F54[]; /* "already busy" trace */
extern char D_80011FFC[]; /* "file already open" trace */
extern char D_80012024[]; /* "file not open" trace */
extern char D_80012048[]; /* read: "un-aligned size" trace */
extern char D_80012080[]; /* write: "un-aligned size" trace */

int func_80066AC8(int *state);
int func_80066CD8(int *state);

/* libmcrd _card_load event handler (0x80066634, 524 bytes), registered by
   MemCardLoad. This slot also holds the six unlabelled file-op functions that
   follow it. Steps: 0 arm the BIOS card events, 10 format an unformatted card
   (op result 3) or go straight to 30, 30 issue _card_load, 31 wait for it;
   result 1 retries the load up to 16 times, 0 reports 3 if the card had to be
   formatted, 4 is reported as-is, anything else is mapped. */
int func_80066634(int *state) {
  int res;

  switch (*state) {
  case 0:
    D_80075B08 = 0;
    D_80075B04 = 0;
    D_80075B00 = 0;
    RegisterMemCardEvent(D_800663D8);
    *state = 10;
    break;
  case 10:
    res = *(volatile int *)D_80075B54_blk;
    if (res != 0) {
      if (res != 3) {
        return 1;
      }
      D_80075B08 = 1;
      PollMemCardEvents();
      MemCardFormatBlocks(D_80075B5C);
      *state = 21;
      break;
    }
    *state = 30;
    break;
  case 21:
    if (GetMemCardEventStatusSlot2() == 0) {
      return 0;
    }
    WaitMemCardEventSlot2();
    *state = 30;
  case 30:
    PollMemCardEvents();
    _card_load(D_80075B5C);
    (*state)++;
    break;
  case 31:
    if (GetMemCardEventStatusSlot1() == 0) {
      return 0;
    }
    switch (D_80075B04 = WaitMemCardEventSlot1()) {
    case 4:
      D_80075B54 = D_80075B04;
      return 1;
    case 0:
      D_80075B54 = D_80075B08 ? 3 : 0;
      return 1;
    case 1:
      if (++D_80075B00 < 16) {
        *state = 0;
        break;
      }
    default:
      D_80075B54 = MapMemCardAccessCode(D_80075B04);
      return 1;
    }
    break;
  }
  return 0;
}

/* libmcrd file open (0x80066840): builds "bu<port>:<name>" in the op block's
   path buffer and opens it, forcing a card load (busy-checked inline) and
   retrying when the open fails: result 3 retries at once, 2 up to 16 times.
   Returns 0 when open, -1 if a file is already open, else the load result
   (5 when the load succeeded but the open still failed). */
int func_80066840(int port, char *name, int mode) {
  int *fd = D_80075B60;
  int tries = 0;
  char *path;
  int result;

  if (*fd >= 0) {
    WritePrintf(D_80011FFC);
    return -1;
  }
  FormatMemCardPath(port, (char *)D_80075B60 + 0x10);
  strcat(path = (char *)D_80075B60 + 0x10, name);
  D_80075B5C = port;
  for (;;) {
    if ((D_80075B60[0] = open(path, mode | 0x8000)) >= 0) {
      return 0;
    }
    D_80075BA0 = func_80067614(0);
    if (D_80075B50 != 0) {
      WritePrintf(D_80011F54);
    } else {
      D_80075B50 = 2;
      D_80075B54 = 0;
      D_80075B58 = 0;
      D_80075B5C = port;
      RegisterMemCardEvent(func_80066634);
    }
    MemCardSync(0, 0, &result);
    func_80067614(D_80075BA0);
    if (result == 3) {
      continue;
    }
    if (result != 2 || ++tries >= 16) {
      break;
    }
  }
  if (result == 0) {
    result = 5;
  }
  return result;
}

/* libmcrd file close (0x800669c0). */
void func_800669C0(void) {
  int *fd = D_80075B60;

  if (fd[0] >= 0) {
    close(fd[0]);
    fd[0] = -1;
  }
}

/* libmcrd file read start (0x80066a08): latches a sector-aligned read of the
   open file into the op block (state 5) and registers func_80066AC8. */
int func_80066A08(void *buf, int ofs, int bytes) {
  if (D_80075B60[0] < 0) {
    WritePrintf(D_80012024);
    return 0;
  }
  if (D_80075B50 != 0) {
    WritePrintf(D_80011F54);
    return 0;
  }
  if (bytes & 0x7F) {
    WritePrintf(D_80012048);
    return 0;
  }
  D_80075B50 = 5;
  D_80075B54 = 0;
  D_80075B58 = 0;
  D_80075B64 = ofs;
  D_80075B6C = (int)buf;
  D_80075B68 = bytes;
  RegisterMemCardEvent(func_80066AC8);
  return 1;
}

/* Event handler for func_80066A08 (0x80066ac8): seek, read, then wait with up
   to 16 re-seeks on a retryable result. Returns 1 when finished. */
int func_80066AC8(int *state) {
  int *f;
  int code;

  switch (*state) {
  case 0:
    D_80075188 = 0;
    *state = 10;
  case 10:
    f = D_80075B60;
    while (lseek(f[0], f[1], 0) != f[1]) {
    }
    *state = 20;
    break;
  case 20:
    PollMemCardEvents();
    f = D_80075B60;
    while (read(f[0], (void *)f[3], f[2]) != 0) {
    }
    *state = 30;
    break;
  case 30:
    if (GetMemCardEventStatusSlot1() == 0) {
      return 0;
    }
    code = WaitMemCardEventSlot1();
    if (code == 1 && ++D_80075188 < 16) {
      *state = 10;
      break;
    }
    D_80075B54 = MapMemCardAccessCode(code);
    return 1;
  default:
    return 0;
  }
  return 0;
}

/* libmcrd file write start (0x80066c18): as func_80066A08 with state 6 and
   handler func_80066CD8. */
int func_80066C18(void *buf, int ofs, int bytes) {
  if (D_80075B60[0] < 0) {
    WritePrintf(D_80012024);
    return 0;
  }
  if (D_80075B50 != 0) {
    WritePrintf(D_80011F54);
    return 0;
  }
  if (bytes & 0x7F) {
    WritePrintf(D_80012080);
    return 0;
  }
  D_80075B50 = 6;
  D_80075B54 = 0;
  D_80075B58 = 0;
  D_80075B64 = ofs;
  D_80075B6C = (int)buf;
  D_80075B68 = bytes;
  RegisterMemCardEvent(func_80066CD8);
  return 1;
}

/* Event handler for func_80066C18 (0x80066cd8): seek, write, wait. */
int func_80066CD8(int *state) {
  int *f;
  int code;

  switch (*state) {
  case 0:
    D_8007518C = 0;
    *state = 10;
  case 10:
    f = D_80075B60;
    while (lseek(f[0], f[1], 0) != f[1]) {
    }
    *state = 20;
    break;
  case 20:
    PollMemCardEvents();
    f = D_80075B60;
    while (write(f[0], (void *)f[3], f[2]) != 0) {
    }
    *state = 30;
    break;
  case 30:
    if (GetMemCardEventStatusSlot1() == 0) {
      return 0;
    }
    code = WaitMemCardEventSlot1();
    if (code == 1 && ++D_8007518C < 16) {
      *state = 10;
      break;
    }
    D_80075B54 = MapMemCardAccessCode(code);
    return 1;
  default:
    return 0;
  }
  return 0;
}
