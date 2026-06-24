#include "globals.h"

/* Zero the int field at +0x8 of the record pointed at by a0. A single-store
   leaf: the `sw $zero` rides the jr delay slot. (Unnamed in the Ghidra seed;
   the surrounding cluster at 0x80017xxx is small actor/record helpers.) */
void func_80017420(int *rec) { rec[2] = 0; }
