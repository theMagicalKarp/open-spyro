#include "globals.h"

/* libcd CdIntToPos: signed sector LBA -> 3-byte BCD CdlLOC (minute:second:
   frame). +0x96 is the 150-sector lead-in offset, /75 frames-per-second,
   /60 seconds-per-minute; each MSF component is BCD-encoded as
   (x/10)*0x10 + x%10. Inverse of CdPosToInt. (0x80064094, 260 bytes.) */
char *CdIntToPos(int i, char *loc) {
  int n = i + 0x96;
  int q = n / 75;
  int frame = n % 75;
  int minute = q / 60;
  int second = q - minute * 60;
  loc[2] = (frame / 10) * 0x10 + frame % 10;
  loc[1] = (second / 10) * 0x10 + second % 10;
  loc[0] = (minute / 10) * 0x10 + minute % 10;
  return loc;
}
