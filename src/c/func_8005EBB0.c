#include "globals.h"

/* PSY-Q GetTPage: pack a texture-page attribute word from the texture format
   `tp`, semi-transparency rate `abr`, and the page's VRAM (`x`,`y`) origin
   (0x8005ebb0, 60 bytes). Bit 0x100 of `y` selects the lower/upper VRAM half.
 */
int func_8005EBB0(int tp, int abr, int x, int y) {
  return ((tp & 0x3) << 7) | ((abr & 0x3) << 5) | ((y & 0x100) >> 4) |
         ((x & 0x3FF) >> 6) | ((y & 0x200) << 2);
}
