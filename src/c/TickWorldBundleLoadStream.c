#include "globals.h"

/* 0x80014564, 1548 bytes — the world-bundle CD streaming state machine.
   Ticked from Initialize, PauseMenu_Update, GemCutscene_Update,
   Gamestate0D_Update and CheckDeathPlane; each tick services the stall
   watchdog, then gates on the drive being idle (no read in flight,
   CdSync == 2, music stream table built) and runs exactly one stage of
   g_nCdStreamState:

     0  kick the 0x800-byte world header read into g_pDrawBufA
     1  copy the header to the chunk-descriptor scratch, kick the TIM chunk
     2  DMA the TIM chunk from g_pDrawBufA into VRAM at (512, 0)
     3  kick the remaining TIM/audio bytes above the 0x80000 split
     4  upload the audio half to SPU RAM, kick the world bundle read
     5  unpack the bundle, snapshot the section-4 header, kick the ext chunk
        (world 1 streams it in two parts, everyone else in one)
     6  waiting for the first half of world 1's ext chunk
     7  kick the second half of world 1's ext chunk
     8  relocate every actor mesh header, kick the path-table chunk
     9  rebase the path table, seed the actor pool defaults, done
    10  idle — the stream stays here until a caller resets the state

   World id indexes the per-world archive table (8-byte stride) and the
   per-world header-size table (4-byte stride). */

extern void RestartStalledCdRead(void);
extern int FUN_80063bd8(); /* CdSync */
extern void StartCdReadAsync(int lbaBase, void *dst, int size, int offset,
                             int marker);
extern void CopyWords(void *dst, void *src, int byte_count);
extern void LoadImage(RECT *rect, void *data);
extern int SetSpuTransferStartAddr(unsigned int addr);
extern unsigned int WriteSpuRam(void *buf, unsigned int size);
extern long GetSpuTransferStatus(int mode);
extern int *UnpackWorldDataChunks(int *src, int mode);
extern int *RelocateActorMeshHeader(int *header);
extern void InitActorRenderDefaults(int actor);

extern int
    g_anWorldTimAudioChunkOffsetBlock[]; /* held-base view of the chunk
                                   scratch at g_nWorldTimAudioChunkOffset */
extern int
    g_anActorMeshOffsetBlock[]; /* held-base view of the mesh-offset list;
                                   [-16] is g_nWorldDataExtChunkOffset */
extern void *g_apActorMeshTableBlock[];
extern int g_anCdReadInFlightBlock[]; /* held-base view of g_nCdReadInFlight */
extern void *g_apDrawBufBlock[];      /* held-base view of g_pDrawBufA */
extern void *volatile g_apPathTableBufferBlock[]; /* g_pPathTableBuffer, stored
                                          then reloaded through the same base */

struct SECTION4HDR {
  int a;
  int b;
  int c;
  int d;
  int e;
};

/* Register pins (`off` in v1, `pb` in a1) reproduce the original's
   local-alloc choices in arms 7 and 8: a hard reg is outside qty priority,
   so the competing temps fall into the order the original has. */
void TickWorldBundleLoadStream(void) {
  RECT tim;
  int pad[4];
  int state;
  int extSize;
  int new_var;
  int i;
  int def;
  int *unpacked;
  void *volatile *buf;
  int *ent;
  int *head;
  unsigned char *actor;
  RestartStalledCdRead();
  if ((*((volatile int *)g_anCdReadInFlightBlock)) != 0) {
    return;
  }
  if (FUN_80063bd8(1, 0) != 2) {
    return;
  }
  if ((g_dwMusicStreamStatus & 0x40) == 0) {
    return;
  }
  state = g_nCdStreamState;
  if (state == 0) {
    int lba = *((volatile int *)(&g_nCdBaseLba));
    int world = *((volatile int *)(&g_nCurrentWorldId));
    void *dst = *((void *volatile *)(&g_pDrawBufA));
    new_var = (&g_nWorldArchiveOffset)[world * 2];
    StartCdReadAsync(lba, dst, 0x800, new_var, 0x258);
    g_nCdStreamState = 1;
  } else if (state == 1) {
    int lba;
    int world;
    void *dst;
    CopyWords(g_anWorldTimAudioChunkOffsetBlock, g_apDrawBufBlock[0], 0x1D0);
    lba = *((volatile int *)(&g_nCdBaseLba));
    world = *((volatile int *)(&g_nCurrentWorldId));
    dst = *((void *volatile *)(&g_apDrawBufBlock[0]));
    StartCdReadAsync(lba, dst, (&g_nWorldHeaderChunkSize)[world],
                     g_anWorldTimAudioChunkOffsetBlock[0] +
                         (&g_nWorldArchiveOffset)[world * 2],
                     0x258);
    g_nCdStreamState = 2;
    return;
  } else if (state == 2) {
    tim.x = 0x200;
    tim.y = 0;
    tim.w = 0x200;
    tim.h = (&g_nWorldHeaderChunkSize)[g_nCurrentWorldId] >> 10;
    LoadImage(&tim, g_pDrawBufA);
    g_nCdStreamState = 3;
  } else if (state == 3) {
    int lba = *((volatile int *)(&g_nCdBaseLba));
    void *dst = *((void *volatile *)(&g_pDrawBufA));
    int size = (*((volatile int *)(&g_nWorldTimAudioChunkSize))) - 0x80000;
    int world = *((volatile int *)(&g_nCurrentWorldId));
    int off = *((volatile int *)(&g_nWorldTimAudioChunkOffset));
    StartCdReadAsync(lba, dst, size,
                     (off + (&g_nWorldArchiveOffset)[world * 2]) + 0x80000,
                     0x258);
    g_nCdStreamState = 4;
  } else if (state == 4) {
    SetSpuTransferStartAddr(0x1010);
    WriteSpuRam(g_pDrawBufA, 0x7EFF0);
    while (GetSpuTransferStatus(0) == 0) {
    }

    {
      int lba = *((volatile int *)(&g_nCdBaseLba));
      void *dst = *((void *volatile *)(&g_pDrawBufB));
      int world = *((volatile int *)(&g_nCurrentWorldId));
      int size = *((volatile int *)(&g_nWorldBundleChunkSize));
      int off = *((volatile int *)(&g_nWorldBundleChunkOffset));
      StartCdReadAsync(lba, dst, size,
                       off + (&g_nWorldArchiveOffset)[world * 2], 0x258);
    }
    g_nCdStreamState = 5;
  } else if (state == 5) {
    unpacked = UnpackWorldDataChunks(g_pDrawBufB, 1);
    *((struct SECTION4HDR *)(&g_nWorldRenderMeshChunkCount)) =
        *((struct SECTION4HDR *)(&g_nWorldCollisionTriCount));
    g_pWorldUnpackHead = unpacked;
    if (g_nCurrentWorldId == 1) {
      g_nCdStreamState = 6;
      extSize = g_nWorldDataExtChunkSize - 0x60000;
    } else {
      extSize = g_nWorldDataExtChunkSize;
      g_nCdStreamState = 8;
    }
    {
      int lba = *((volatile int *)(&g_nCdBaseLba));
      int world = *((volatile int *)(&g_nCurrentWorldId));
      void *dst = *((void *volatile *)(&g_pWorldUnpackHead));
      register int off asm("$3") =
          *((volatile int *)(&g_nWorldDataExtChunkOffset));
      StartCdReadAsync(lba, dst, extSize,
                       off + (&g_nWorldArchiveOffset)[world * 2], 0x258);
    }
    return;
  } else if (state == 6) {
    g_nCdStreamState = 7;
  } else if (state == 7) {
    void *head2 = *((void *volatile *)(&g_pWorldUnpackHead));
    int size = *((volatile int *)(&g_nWorldDataExtChunkSize));
    int lba = *((volatile int *)(&g_nCdBaseLba));
    int world = *((volatile int *)(&g_nCurrentWorldId));
    register int off asm("$3") =
        *((volatile int *)(&g_nWorldDataExtChunkOffset));
    int arch = (&g_nWorldArchiveOffset)[world * 2] - 0x60000;
    StartCdReadAsync(lba, (((char *)head2) + size) - 0x60000, 0x60000,
                     (off + size) + arch, 0x258);
    g_nCdStreamState = 8;
  } else if (state == 8) {
    if (g_anActorMeshOffsetBlock[0] > 0) {
      i = 0;
      do {
        g_apActorMeshTableBlock[i + 1] =
            RelocateActorMeshHeader((int *)(((char *)g_pWorldUnpackHead) +
                                            (g_anActorMeshOffsetBlock[i] -
                                             g_anActorMeshOffsetBlock[-16])));
        i++;
      } while (g_anActorMeshOffsetBlock[i] > 0);
    }
    {
      register void *volatile *pb asm("$5") = g_apPathTableBufferBlock;
      void *head2 = *((void *volatile *)(&g_pWorldUnpackHead));
      int extsz = *((volatile int *)(&g_nWorldDataExtChunkSize));
      int lba = *((volatile int *)(&g_nCdBaseLba));
      int size;
      int off;
      int count;
      int world;
      void *dst;
      pb[0] = ((char *)head2) + extsz;
      size = *((volatile int *)(&g_nLevelArchiveChunkSize));
      dst = pb[0];
      off = *((volatile int *)(&g_nLevelArchiveChunkOffset));
      g_nLevelArchiveByteSize = size;
      count = *((volatile int *)(&g_nLevelArchiveByteSize));
      world = *((volatile int *)(&g_nCurrentWorldId));
      g_nLevelArchiveBaseLba = off;
      StartCdReadAsync(lba, dst, count,
                       off + (&g_nWorldArchiveOffset)[world * 2], 0x258);
    }
    g_nCdStreamState = 9;
  } else if (state == 9) {
    i = 0;
    buf = g_apPathTableBufferBlock;
    ent = buf[0];
    g_pPathTableHead = ent;
    ent[4] = ((int)ent) + ent[4];
    if (ent[3] > 0) {
      void *volatile *bp = buf;
      head = ent;
    entry_loop:
      i += 1;

      ent[5] = (int)bp[0] + ent[5];
      ent += 1;
      if (i < head[3]) {
        goto entry_loop;
      }
    }
    g_pActorListBase = ((char *)g_pPathTableBuffer) + g_nLevelArchiveByteSize;
    i = 0;
    if (((int *)g_pPathTableHead)[3] > 0) {
      def = 0x20;
    actor_loop:
      InitActorRenderDefaults(((int)g_pActorListBase) + (i * 0x58));

      actor = (unsigned char *)((i * 0x58) + ((int)g_pActorListBase));
      actor[0x50] = def;
      ((short *)actor)[0x1B] = i + 1;
      i += 1;
      if (i < ((int *)g_pPathTableHead)[3]) {
        goto actor_loop;
      }
    }
    (((unsigned char *)g_pActorListBase) +
     (((int *)g_pPathTableHead)[3] * 0x58))[0x48] = 0xFF;
    g_nCdStreamState = 0xA;
  }
}
