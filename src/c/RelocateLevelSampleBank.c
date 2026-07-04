#include "globals.h"

/* Publish the loaded level sample-bank header/entry pointers, and (when
   relocate != 0) patch each entry's leading file-relative sample offset to
   absolute by adding the 0x1010 SPU RAM base. Entries are 0x14 bytes; the
   count sits at header+0x100, the entry array at header+0x104. Walks the
   entries back-to-front. (0x80012cf0, 104 bytes.) */
void RelocateLevelSampleBank(int *header, int relocate) {
  int i;
  int off;
  int *entry;
  g_pLevelSampleBankHeader = header;
  header += 0x40;
  i = *header;
  header += 1;
  g_pLevelSampleBankEntries = header;
  if (relocate != 0) {
    i -= 1;
    if (i >= 0) {
      off = i * 0x14;
      do {
        entry = (int *)(off + (int)g_pLevelSampleBankEntries);
        i -= 1;
        *entry = *entry + 0x1010;
        off -= 0x14;
      } while (i >= 0);
    }
  }
}
