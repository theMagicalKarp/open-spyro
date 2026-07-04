#include "globals.h"

extern short *D_80073554; /* SPU voice register shadow base */
extern void DelaySpuRegisterWrite(void);

/* libspu voice L/R volume: write the masked (15-bit) left/right volumes into
   the voice register shadow at base + voice*0x10, then flush with the SPU
   register-write fence. (0x8005da74, 64 bytes.) */
void SetSpuVoiceVolumeLR(int voice, unsigned short left, unsigned short right) {
  volatile short *reg;
  left &= 0x7FFF;
  right &= 0x7FFF;
  reg = (volatile short *)(voice * 0x10 + (int)D_80073554);
  reg[0] = left;
  reg[1] = right;
  DelaySpuRegisterWrite();
}
