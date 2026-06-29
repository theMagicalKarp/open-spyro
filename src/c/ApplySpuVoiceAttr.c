#include "globals.h"

extern void ApplySpuVoiceAttrRange(void *attr, int lo, int hi, int flags);

/* Apply a full SPU voice-attribute block: forward to ApplySpuVoiceAttrRange
   over the whole 0x17-field range (0x8005c7ac, 40 bytes). */
void ApplySpuVoiceAttr(void *attr) { ApplySpuVoiceAttrRange(attr, 0, 0x17, 0); }
