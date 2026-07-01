#include "globals.h"

extern void MemCardInitCard(void);
extern void MemCardStartCard(void);
extern void _bu_init(void);

/* libmcrd one-shot card-driver init: InitCARD2 (MemCardInitCard) + StartCARD2
   (MemCardStartCard) + BIOS memcard filesystem (_bu_init). Returns 0.
   (0x80067e70, 48 bytes.) */
int MemCardBuInit(void) {
  MemCardInitCard();
  MemCardStartCard();
  _bu_init();
  return 0;
}
