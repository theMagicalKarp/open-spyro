#include "globals.h"

extern int write(int fd, char *buf, int len);
extern unsigned char D_80074D15[]; /* ctype table, biased base */
extern int D_80075AC8;             /* output column position */

/* PSn00bSDK-style _putchar: writes one byte to fd 1 and tracks the output
   column. Tab recurses on space until the column is 8-aligned; newline
   recurses on CR and resets the column; other printable bytes (ctype mask
   0x97) bump it. (0x80062f10, 180 bytes.) */
void WriteChar(unsigned char c) {
  switch (c) {
  case 10:
    WriteChar(13);
    D_80075AC8 = 0;
    break;
  case 9:
    do {
      WriteChar(' ');
    } while (D_80075AC8 & 7);
    return;
  default:
    if (D_80074D15[c] & 0x97) {
      D_80075AC8 = D_80075AC8 + 1;
    }
    break;
  }
  write(1, (char *)&c, 1);
}
