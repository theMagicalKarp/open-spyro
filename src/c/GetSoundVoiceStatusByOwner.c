#include "globals.h"
#include "spu.h"

/* Scan the 0x18 SPU voice records (0x1C bytes each) for the one owned by
   `owner` playing sample `sampleId`. Returns VOICE_OWNER_LOOPING if that voice
   is VOICE_ROW_LOOPING, VOICE_OWNER_PLAYING if matched without the flag, or
   VOICE_OWNER_NONE. (0x80056dc4, 120 bytes.) */
int GetSoundVoiceStatusByOwner(int owner, unsigned int sampleId) {
  int i;

  for (i = 0; i < SPU_VOICE_COUNT; i++) {
    int off = i * VOICE_ROW_STRIDE;
    if (owner == VOICE_ROW_RAW(off, int, pOwner) &&
        VOICE_ROW_RAW(off, unsigned char, nSampleId) == sampleId) {
      if (VOICE_ROW_RAW(off, unsigned short, nFlags) & VOICE_ROW_LOOPING) {
        return VOICE_OWNER_LOOPING;
      }
      return VOICE_OWNER_PLAYING;
    }
  }
  return VOICE_OWNER_NONE;
}
