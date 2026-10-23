#include "funcs.h"
#include "globals.h"

/* End-of-frame pad snapshot: toggles the reverse-input flag, copies the 0xA4
   bytes of committed pad state into the snapshot buffer, then resets the
   per-substep distribution table. (0x80053708, 84 bytes.) */
void SnapshotPadInputState(void *param_1, void *param_2) {
  g_bSpyroPadSnapshotReverseFlag = 1 - g_bSpyroPadSnapshotReverseFlag;
  CopyWords(param_2, param_1, 0xA4);
  ResetPadFrameSubstepTable(param_1);
}
