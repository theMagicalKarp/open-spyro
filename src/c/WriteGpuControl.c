#include "globals.h"

/* PSY-Q GPU control write (0x800616f4, 40 bytes): push the command word to the
   GP1 control register and mirror its high byte into the per-command status
   shadow indexed by the command opcode (cmd >> 24). */
void WriteGpuControl(unsigned int cmd) {
  *(unsigned int *)g_pGpuStatReg = cmd;
  g_abGpuStatusShadow[cmd >> 24] = cmd;
}
