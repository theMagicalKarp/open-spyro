#include "globals.h"

extern unsigned int IsMemCardEventStackEmpty(void);
extern void PopMemCardEventHandler(void);
extern volatile int D_80075B50[];             /* memcard op-in-progress flag */
extern volatile int D_80075B54;               /* memcard op result code */
extern volatile int D_80075B58;               /* memcard op-complete flag */
extern void (*volatile D_80075B90)(int, int); /* memcard completion callback */
extern volatile int D_80075B94[]; /* latched op flag for the callback */
extern volatile int D_80075B98;   /* latched result code for the callback */

/* libmcrd event-stack unwinder: pops one handler; when the stack drains,
   latches the op flag/result, clears them, and fires the completion callback.
   (0x80067cd4, 160 bytes.) */
void func_80067CD4(void) {
  void (*fn)(int, int);
  int result;

  if (IsMemCardEventStackEmpty() == 0) {
    PopMemCardEventHandler();
    if (IsMemCardEventStackEmpty() != 0) {
      D_80075B58 = 1;
      D_80075B94[0] = D_80075B50[0];
      result = D_80075B54;
      fn = D_80075B90;
      D_80075B98 = result;
      D_80075B50[0] = 0;
      D_80075B54 = 0;
      if (fn != 0) {
        int arg0 = D_80075B94[0];
        int arg1 = D_80075B98;
        fn(arg0, arg1);
      }
    }
  }
}
