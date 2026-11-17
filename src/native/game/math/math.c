/* Native replacements for src/main/math/math.c definitions. */

#include "../../wasm/host_imports.h"

/* 0x80017fd4: root counter 2 (0x1F801120) is not emulated natively. Its only
 * use is a varying tag for new saves (titlescreen), so a free-running counter
 * from host time serves. */
unsigned short ReadRootCounter2(void) { return (unsigned short)host_now_us(); }
