#include "globals.h"

extern int CdReadSyncSectors();
extern void CopyWords();
extern int *D_800113A0;                   /* shared CD sector buffer pointer */
extern int D_8007A6D0[];                  /* boot asset directory table */
extern volatile int g_anCdBaseLbaBlock[]; /* alias of g_nCdBaseLba */

/* Boot-time asset directory load: sector 0x25 read synchronously into the
   shared CD buffer, then 0x330 words copied out to the asset directory that
   Initialize consults for the follow-up art/world/texture/CLUT loads. The
   LBA global is volatile-reloaded for the call argument, reusing the
   store's base register. (0x8001250c, 96 bytes.) */
void LoadAssetDirectoryFromCd(void) {
  volatile int *lba = g_anCdBaseLbaBlock;

  *lba = 0x25;
  CdReadSyncSectors(*lba, D_800113A0, 0x800, 0, 600);
  CopyWords(D_8007A6D0, D_800113A0, 0x330);
}
