#include "globals.h"

extern int FormatAndWrite(int fd, char *fmt, void *args);

/* PSn00bSDK-style printf: formats to fd 1 (console/tty write), passing the
   address of the first vararg's homed register slot as the arg list.
   (0x8006279c, 60 bytes.) */
int WritePrintf(char *fmt, int arg1, int arg2, int arg3) {
  (void)&fmt;
  (void)&arg2;
  (void)&arg3;
  return FormatAndWrite(1, fmt, &arg1);
}
