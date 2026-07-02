#include "globals.h"

extern void WriteChar(char c);

/* PSn00bSDK-style _print: writes a null-terminated string one WriteChar at a
   time; NULL input prints the "(null)" placeholder. No newline appended.
   (0x8006389c, 80 bytes.) */
void WriteString(char *s) {
  char c;

  if (s == 0) {
    s = g_szNullPlaceholder;
  }
  while ((c = *s++) != 0) {
    WriteChar(c);
  }
}
