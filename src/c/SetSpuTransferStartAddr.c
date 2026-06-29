#include "globals.h"

extern unsigned int WriteSpuVoiceAddrRegister(int voice, unsigned int addr);
extern unsigned short D_8007356C;

/* libspu SpuSetTransferStartAddr: clamp `addr` (SPU sound-RAM byte address) to
   [0x1010, 0x80000), encode it to register units via the no-write sentinel
   WriteSpuVoiceAddrRegister(-1, addr), stash the encoded short in the transfer
   shadow (D_8007356C), and return the byte address re-decoded from it; an
   out-of-range address returns 0 (0x8005cb24, 88 bytes). */
unsigned int SetSpuTransferStartAddr(unsigned int addr) {
  unsigned int enc;
  if (addr - 0x1010 > 0x7EFE8) {
    return 0;
  }
  enc = WriteSpuVoiceAddrRegister(-1, addr);
  D_8007356C = enc;
  return (enc & 0xFFFF) << g_nSpuAddrShift;
}
