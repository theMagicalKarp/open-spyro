#include "globals.h"

/* Queue a GPU op with no extra word (param3 = 0). */
void func_800617FC(undefined *p1, undefined4 *p2, undefined4 p4) {
  EnqueueGpuOp(p1, p2, 0, p4);
}
