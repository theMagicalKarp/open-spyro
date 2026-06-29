#include "globals.h"

/* PSY-Q GetGraphType: report the cached video-standard / graph type word
   (0x8005eba0, 16 bytes). */
int GetGraphType(void) { return g_nGraphType; }
