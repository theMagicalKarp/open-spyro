#include "globals.h"

/* Set the low 24 bits of the word at *p (OR in 0xFFFFFF) and return the result.
 */
int func_8005ED78(int *p) { return *p |= 0xFFFFFF; }
