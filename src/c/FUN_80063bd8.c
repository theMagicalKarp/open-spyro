#include "globals.h"

extern int CdSync(int mode, unsigned char *result);

/* Sony SDK internal: pure thunk forwarding to the public CdSync(mode, result)
   (0x80063bd8, 32 bytes). */
int FUN_80063bd8(int mode, unsigned char *result) {
  return CdSync(mode, result);
}
