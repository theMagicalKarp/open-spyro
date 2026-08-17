#include "globals.h"

/* 0x8005b7d8, 264 bytes — load the actor-mesh directory from CD into the top of
   the boot-load region. CdReadSyncSectors pulls D_8007A714 bytes from sector
   D_8007A710 into a buffer just under 0x80200000 - g_nCrt0StackSize; the first
   word gives the payload size, which is copied down to a fresh g_pWorkAreaTop.
   Up to 64 directory entries are then walked: each non-empty entry relocates a
   mesh header and stores the resolved pointer at g_apActorMeshTable[slot]. */

extern int D_8007A710;
extern int D_8007A714[4]; /* sized alias: keeps the directory-header address in
                             a register instead of folding to lui/lw %lo */

extern void *g_apActorMeshTableBlock[]; /* incomplete-array view of
                                           g_apActorMeshTable */

extern void CdReadSyncSectors(int lba, void *buf, int size, int sector,
                              int marker);
extern void CopyWords(void *dst, void *src, int byte_count);
extern int *RelocateActorMeshHeader(int *header);
extern void InitActorMeshScratchRegions(int arg);

void LoadActorMeshTables(void) {
  int *dir;
  int top;
  int nbytes;
  int size;
  int i;
  int lba;
  int *hdr;
  int chunk;
  void **table;

  i = 0;
  table = g_apActorMeshTableBlock;
  top = 0x80200000;
  hdr = D_8007A714;
  lba = *(volatile int *)&g_nCdBaseLba;
  nbytes = *(volatile int *)hdr;
  size = *hdr;
  dir = (int *)(top - size - size - g_nCrt0StackSize);
  CdReadSyncSectors(lba, dir, nbytes, D_8007A710, 0x258);

  chunk = dir[0];
  g_pWorkAreaTop = (void *)(top - g_nCrt0StackSize - chunk);
  nbytes = *(volatile int *)dir;
  CopyWords(g_pWorkAreaTop, dir + 0x200, nbytes);

  for (; i < 0x40; i++) {
    if (dir[i * 2 + 1] == 0) {
      break;
    }
    table[dir[i * 2 + 2]] = RelocateActorMeshHeader(
        (int *)((char *)g_pWorkAreaTop + dir[i * 2 + 1] - 0x800));
  }

  InitActorMeshScratchRegions(0);
}
