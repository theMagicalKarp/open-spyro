#include "globals.h"
#include "spu.h"

/* Scan the 0x18 SPU voice records (0x1C bytes each) for the one owned by
   `owner` playing sample `sampleId`. Returns 2 if that voice's flag word has
   bit 0x100 set, 1 if matched without the flag, or 0 if no
   voice matches. (0x80056dc4, 120 bytes.) */
int GetSoundVoiceStatusByOwner(int owner, unsigned int sampleId) {
  int i;

  for (i = 0; i < 0x18; i++) {
    int off = i * 0x1C;
    if (owner == VOICE_ROW_RAW(off, int, pOwner) &&
        VOICE_ROW_RAW(off, unsigned char, nSampleId) == sampleId) {
      if (VOICE_ROW_RAW(off, unsigned short, nFlags) & 0x100) {
        return 2;
      }
      return 1;
    }
  }
  return 0;
}
