#include "globals.h"

extern volatile unsigned short *D_80075110; /* SPU register base */
extern volatile unsigned char *D_800750FC;  /* CDIO index register */
extern volatile unsigned char *D_80075100;  /* CDIO data register 1 */
extern volatile unsigned char *D_80075104;  /* CDIO data register 2 */
extern volatile unsigned char *D_80075108;  /* CDIO data register 3 */

/* libcd audio path init: restore SPU main volume to 0x3FFF (only if both
   halves are zero), set CD-input volume 0x3FFF and SPUCNT to 0xC001
   (enable + unmute + CD audio), then program the default CD attenuation
   matrix (L->L / R->R 0x80, cross terms 0) and commit it (0x20).
   (0x80065270, 244 bytes.) */
int FUN_80065270(void) {
  unsigned char buf[4];

  if (D_80075110[0xDC] == 0 && D_80075110[0xDD] == 0) {
    D_80075110[0xC0] = 0x3FFF;
    D_80075110[0xC1] = 0x3FFF;
  }
  D_80075110[0xD8] = 0x3FFF;
  D_80075110[0xD9] = 0x3FFF;
  D_80075110[0xD5] = 0xC001;

  buf[2] = 0x80;
  buf[0] = 0x80;
  buf[3] = 0;
  buf[1] = 0;
  *D_800750FC = 2;
  *D_80075104 = buf[0];
  *D_80075108 = buf[1];
  *D_800750FC = 3;
  *D_80075100 = buf[2];
  *D_80075104 = buf[3];
  *D_80075108 = 0x20;
  return 0;
}
