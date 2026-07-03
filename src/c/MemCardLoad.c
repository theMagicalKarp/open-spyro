#include "globals.h"

extern void RegisterMemCardEvent(void (*handler)());
extern int WritePrintf();
extern void func_80066634();
extern volatile int D_80075B50[]; /* memcard pending-op block: op code */
extern volatile int D_80075B54;   /*   phase */
extern volatile int D_80075B58;   /*   result */
extern volatile int D_80075B5C;   /*   request arg */
extern char D_80011F54[];         /* "busy" printf format string */

/* Start an async memcard load: if no op is pending, mark op 2 (load), clear
   phase/result, stash the request arg and register func_80066634 as the
   completion handler (returns 1). If an op is already pending, WritePrintf the
   busy message and return 0. (0x800665b8, 124 bytes.) */
int MemCardLoad(int arg) {
  volatile int *op = D_80075B50;
  int ret;
  if (*op == 0) {
    *op = 2;
    D_80075B54 = 0;
    D_80075B58 = 0;
    D_80075B5C = arg;
    RegisterMemCardEvent(func_80066634);
    ret = 1;
  } else {
    WritePrintf(D_80011F54, arg);
    ret = 0;
  }
  return ret;
}
