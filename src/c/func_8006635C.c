#include "globals.h"

extern void RegisterMemCardEvent(void (*handler)());
extern int WritePrintf();
extern void PollMemCardEvents(void);
extern int _card_info();
extern int GetMemCardEventStatusSlot1(void);
extern int WaitMemCardEventSlot1(void);
extern int MapMemCardAccessCode();
extern volatile int D_80075B50[]; /* memcard pending-op block: op code */
extern volatile int D_80075B54;   /*   phase */
extern volatile int D_80075B58;   /*   result */
extern volatile int D_80075B5C;   /*   request arg (port) */
extern int D_80075AF8;            /* card-info busy retry counter */
extern int D_80075AFC;            /* card-info event status */
extern volatile int D_80075B4C;   /* per-port new-card bitmask */
extern char D_80011F54[];         /* "busy" printf format string */

static int func_800663D8(int *state);

/* Start an async memcard card-info query: if no op is pending, mark op 1,
   clear phase/result, stash the port and register the local state-machine
   handler (returns 1). If an op is already pending, WritePrintf the busy
   message and return 0. Mirror of MemCardLoad. (0x8006635c, 124 bytes.) */
int func_8006635C(int arg) {
  volatile int *op = D_80075B50;
  int ret;
  if (*op == 0) {
    *op = 1;
    D_80075B54 = 0;
    D_80075B58 = 0;
    D_80075B5C = arg;
    RegisterMemCardEvent(func_800663D8);
    ret = 1;
  } else {
    WritePrintf(D_80011F54, arg);
    ret = 0;
  }
  return ret;
}

/* Per-frame handler for the card-info op: state 0 arms the memcard events
   and issues _card_info (state 10 -> 11), state 11 waits for the event and
   maps the result — status 1 (busy) retries up to 16 times, status 0/4
   maintain the per-port new-card bitmask. Returns 1 when the op completes.
   (0x800663d8, 480 bytes.) */
static int func_800663D8(int *state) {
  int status;
  unsigned int bit;

  switch (*state) {
  case 0:
    D_80075AFC = 0;
    D_80075AF8 = 0;
    PollMemCardEvents();
    *state = 10;
    /* fallthrough */
  case 10:
    _card_info(D_80075B5C);
    *state += 1;
    break;
  case 11:
    if (GetMemCardEventStatusSlot1() == 0) {
      return 0;
    }
    status = WaitMemCardEventSlot1();
    D_80075AFC = status;
    switch (status) {
    case 4:
      D_80075B4C = D_80075B4C | (1 << D_80075B5C);
      D_80075B54 = MapMemCardAccessCode(4);
      return 1;
    case 0:
      bit = 1 << D_80075B5C;
      if ((D_80075B4C & bit) == 0) {
        D_80075AFC = 4;
        D_80075B4C = D_80075B4C | bit;
      }
      D_80075B54 = MapMemCardAccessCode(D_80075AFC);
      return 1;
    case 1:
      D_80075AF8 += 1;
      if (D_80075AF8 < 16) {
        *state = 10;
        return 0;
      }
      /* fallthrough */
    default:
      D_80075B4C = D_80075B4C & ~(1 << D_80075B5C);
      D_80075B54 = MapMemCardAccessCode(D_80075AFC);
      return 1;
    }
  }
  return 0;
}
