#include "globals.h"

/* Maps a memcard access-mode code to its libmcrd request code: 0->0, 1->2,
   2->1, 4->3; any other value is passed through with the 0x8000 error bit set.
   (0x80067c7c, 88 bytes.) */
int MapMemCardAccessCode(int code) {
  int result = 0;
  if (code != 1) {
    if (code < 2) {
      if (code != 0) {
        result = code | 0x8000;
      }
    } else if (code != 2) {
      result = 3;
      if (code != 4) {
        result = code | 0x8000;
      }
    } else {
      result = 1;
    }
  } else {
    result = 2;
  }
  return result;
}
