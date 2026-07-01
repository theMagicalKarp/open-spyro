#include "globals.h"

extern volatile unsigned char *volatile D_800750FC; /* CDIO index register */
extern volatile unsigned char *volatile D_80075100; /* CDIO data register 1 */
extern volatile unsigned char *volatile D_80075104; /* CDIO data register 2 */
extern volatile unsigned char *volatile D_80075108; /* CDIO data register 3 */

/* libcd CD-DA volume writer (used by CdMix): pokes the 4 mixer values
   (L->L, L->R, R->L, R->R) via CDIO index-2/3 register switching.
   Always returns 0. (0x80065108, 136 bytes.) */
int FUN_80065108(unsigned char *vol) {
  *D_800750FC = 2;
  *D_80075104 = vol[0];
  *D_80075108 = vol[1];
  *D_800750FC = 3;
  *D_80075100 = vol[2];
  *D_80075104 = vol[3];
  *D_80075108 = 0x20;
  return 0;
}
