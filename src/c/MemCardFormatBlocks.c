#include "globals.h"

extern void _new_card(void);
extern long _card_write(long chan, long block, unsigned char *buf);

/* libmcrd block-format helper: BIOS card format/zero, then write 63 (= max
   blocks) sectors of nulls to the given channel. (0x80068920, 56 bytes.) */
int MemCardFormatBlocks(int chan) {
  _new_card();
  return _card_write(chan, 0x3F, 0);
}
