#include "globals.h"

extern volatile int
    D_80075B50[]; /* memcard op-in-progress flag; [2] = op-complete flag */
extern volatile int D_80075B54; /* memcard op result code */
extern volatile int D_80075B58; /* memcard op-complete flag */
extern volatile int
    D_80075B94[]; /* latched op flag from the completion unwinder */
extern volatile int
    D_80075B98[]; /* latched result code from the completion unwinder */

/* libmcrd MemCardSync(mode, cmd, result): mode 0 blocks (spins on the
   op-complete flag), nonzero polls. Returns -1 if no op is pending, 0 if still
   in progress (poll), 1 on completion (reports the latched op/result and
   clears the complete flag). (0x80067628, 240 bytes.) */
int MemCardSync(int mode, int *cmd, int *result) {
  int op;
  int res;

  if (D_80075B50[0] == 0 && D_80075B58 == 0) {
    return -1;
  }
  op = D_80075B50[0];
  res = D_80075B54;
  if (mode == 0) {
    if (D_80075B58 == 0) {
      do {
      } while (D_80075B50[2] == 0);
    }
  } else {
    if (D_80075B58 == 0) {
      if (result != 0) {
        *result = res;
      }
      if (cmd != 0) {
        *cmd = op;
      }
      return 0;
    }
  }
  if (result != 0) {
    *result = D_80075B98[0];
  }
  if (cmd != 0) {
    *cmd = D_80075B94[0];
  }
  D_80075B58 = 0;
  return 1;
}
