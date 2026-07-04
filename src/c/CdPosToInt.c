#include "globals.h"

/* libcd CdPosToInt: 3-byte BCD CdlLOC (M:S:F) -> signed LBA. Decodes each
   BCD byte as hi*10 + lo, then minutes*60*75 + seconds*75 + frames - 150
   (150-sector lead-in offset). Inverse of CdIntToPos. (0x80064198, 128 bytes.)
 */
int CdPosToInt(unsigned char *loc) {
  return ((((loc[0] >> 4) * 10 + (loc[0] & 0xF)) * 60 +
           ((loc[1] >> 4) * 10 + (loc[1] & 0xF))) *
              75 +
          ((loc[2] >> 4) * 10 + (loc[2] & 0xF))) -
         150;
}
