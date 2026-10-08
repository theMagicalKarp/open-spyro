#include "globals.h"

extern void RestartStalledCdRead(void);
extern int FUN_80063bd8(); /* CdSync */
extern void StartCdReadAsync(int lbaBase, void *dst, int size, int offset,
                             int marker);
extern void CopyWords(void *dst, void *src, int byte_count);
extern int SetSpuTransferStartAddr(unsigned int addr);
extern unsigned int WriteSpuRam(void *buf, unsigned int size);
extern unsigned int ReadSpuRam(void *buf, unsigned int size);
extern long GetSpuTransferStatus(int mode);
extern void *RelocateActorMeshHeader(void *header);
extern void RelocateMobActorTable(void *table);

extern int g_anCdReadInFlightBlock[]; /* held-base view of g_nCdReadInFlight */
extern char
    *g_apWorldUnpackHeadBlock[]; /* [0] unpack head, [7] g_pWorkAreaTop */
extern int
    g_anGemPickupHeaderEndBlock[]; /* held-base view of the header-end offset */
extern int
    g_anGemPickupStreamStateBlock[]; /* held-base view of the stream state */
extern int
    g_anGemPickupSpuChunkBlock[]; /* held-base view of the SPU chunk index */
extern int g_anActorMeshOffsetBlock[]; /* [-16] is g_nWorldDataExtChunkOffset */
extern void *g_apActorMeshTableBlock[];
extern int D_80076760;    /* end of the unpack work area */
extern char D_80077030[]; /* gem-pickup asset header copy */

#define HEAD (g_apWorldUnpackHeadBlock[0])
#define HDR (g_anGemPickupHeaderEndBlock[0])
#define CHUNK (g_anGemPickupSpuChunkBlock[0])

#define DRAGON_ASSET_OFFSET(i) ((&g_nDragonAssetOffset)[(i) * 2])
#define DRAGON_ASSET_SIZE(i) ((&g_nDragonAssetSize)[(i) * 2])
#define LEVEL_ARCHIVE_OFFSET(i) ((&g_nLevelArchiveOffset)[(i) * 4])

/* 0x80014b70 (0x800): streams the dragon-rescue (gem pickup) assets while the
   pickup overlay runs. Each tick, once the drive is idle and the music
   stream table is built, it runs one stage: read the dragon asset (split in
   two when it does not fit the unpack area), relocate the statue/mirror
   models and camera path, upload the voice sample to SPU RAM in 32 KB
   chunks through the spare primitive buffer, then reload the level's
   TIM/audio tail and ext chunk and re-relocate the level's moby meshes. */
void TickGemPickupAssetStream(void) {
  int pad[10];
  char *buf;
  int n;
  int avail;
  int size;
  int t;
  int i;
  int *list;
  short *slot;
  void **mesh;

  RestartStalledCdRead();
  if (*(volatile int *)g_anCdReadInFlightBlock != 0) {
    return;
  }
  if (FUN_80063bd8(1, 0) != 2) {
    return;
  }
  if (!(g_dwMusicStreamStatus & 0x40)) {
    return;
  }

  switch (g_nGemPickupStreamState) {
  case 0:
    avail = D_80076760 - (int)g_pWorldUnpackHead;
    size = DRAGON_ASSET_SIZE(g_nGemPickupTypeSnapshot);
    if (avail >= size) {
      g_nGemPickupReadByteCount = size;
      g_nGemPickupSplitFlag = 0;
    } else {
      g_nGemPickupReadByteCount = avail / 0x800 * 0x800;
      g_nGemPickupSplitFlag = 1;
    }
    StartCdReadAsync(g_nCdBaseLba, g_pWorldUnpackHead,
                     g_nGemPickupReadByteCount,
                     DRAGON_ASSET_OFFSET(g_nGemPickupTypeSnapshot) +
                         LEVEL_ARCHIVE_OFFSET(g_nLevelIntroIndex),
                     600);
    g_nGemPickupStreamState++;
    return;
  case 1:
    CopyWords(D_80077030, HEAD, 0x24);
    if (g_nGemPickupSplitFlag == 0) {
      g_pGemPickupCameraPath = HEAD + g_nGemPickupCameraPathOffset;
      g_nGemPickupCameraPathCount = g_nGemPickupCameraPathSize / 24;
      g_pGemPickupStatueModelData =
          RelocateActorMeshHeader(HEAD + g_nGemPickupHeaderEndOffset);
      g_pGemPickupMirrorModelData =
          RelocateActorMeshHeader(HEAD + g_nGemPickupMirrorMeshOffset);
      g_nGemPickupSpuChunkIndex = 0;
    } else {
      SetSpuTransferStartAddr(0x80000 - g_nGemPickupAudioSampleSize);
      WriteSpuRam(HEAD + g_nGemPickupAudioSampleOffset,
                  g_nGemPickupAudioSampleSize);
      while (GetSpuTransferStatus(0) == 0) {
      }
      CopyWords(HEAD, HEAD + HDR, g_nGemPickupReadByteCount - HDR);
      StartCdReadAsync(g_nCdBaseLba, HEAD + g_nGemPickupReadByteCount - HDR,
                       DRAGON_ASSET_SIZE(g_nGemPickupTypeSnapshot) -
                           g_nGemPickupReadByteCount,
                       DRAGON_ASSET_OFFSET(g_nGemPickupTypeSnapshot) +
                           LEVEL_ARCHIVE_OFFSET(g_nLevelIntroIndex) +
                           g_nGemPickupReadByteCount,
                       600);
    }
    break;
  case 2:
    if (g_nGemPickupSubstate < 2) {
      return;
    }
    if (g_nGemPickupSplitFlag == 0) {
      {
        register int top asm("$2") = 0x80000;
        SetSpuTransferStartAddr(top + (g_nGemPickupSpuChunkIndex << 15) -
                                g_nGemPickupAudioSampleSize);
      }
      if (g_pActiveFrameDrawEnv == &g_abFrameDrawEnv0) {
        buf = g_pFramePrimBufferBase1;
      } else {
        buf = g_pFramePrimBufferBase0;
      }
      if (g_nGemPickupAudioSampleSize <
          (g_nGemPickupSpuChunkIndex << 15) + 0x8000) {
        n = g_nGemPickupAudioSampleSize % 0x8000;
      } else {
        n = 0x8000;
      }
      ReadSpuRam(buf, n);
      while (GetSpuTransferStatus(0) == 0) {
      }
      {
        register int top asm("$2") = 0x80000;
        SetSpuTransferStartAddr(top + (CHUNK << 15) -
                                g_nGemPickupAudioSampleSize);
      }
      WriteSpuRam((char *)g_pWorldUnpackHead + g_nGemPickupAudioSampleOffset +
                      (CHUNK << 15),
                  n);
      while (GetSpuTransferStatus(0) == 0) {
      }
      CopyWords((char *)g_pWorldUnpackHead + g_nGemPickupAudioSampleOffset +
                    (g_nGemPickupSpuChunkIndex << 15),
                buf, n);
      g_nGemPickupSpuChunkIndex++;
      if ((g_nGemPickupSpuChunkIndex << 15) < g_nGemPickupAudioSampleSize) {
        return;
      }
      g_nGemPickupStreamState++;
      return;
    }
    {
      char **h = g_apWorldUnpackHeadBlock;
      register char *p asm("$3");

      p = *h + g_nGemPickupCameraPathOffset;
      g_pGemPickupCameraPath = p - g_nGemPickupHeaderEndOffset;
      g_nGemPickupCameraPathCount = g_nGemPickupCameraPathSize / 24;
      g_pGemPickupStatueModelData = RelocateActorMeshHeader(*h);
      g_pGemPickupMirrorModelData = RelocateActorMeshHeader(
          *h + g_nGemPickupMirrorMeshOffset - g_nGemPickupHeaderEndOffset);
    }
    g_nGemPickupStreamState++;
    return;
  case 3:
    if (g_nGemPickupSubstate < 6) {
      return;
    }
    if (g_nGemPickupSplitFlag != 1) {
      break;
    }
    t = (g_nGemPickupAudioSampleSize + 0x180F) / 0x800 * 0x800;
    g_nGemPickupReadByteCount = t;
    if (0x100000 - g_nWorldTimAudioChunkSize < t) {
      t -= 0x100000;
      {
        int lba = *(volatile int *)&g_nCdBaseLba;
        int intro = *(volatile int *)&g_nLevelIntroIndex;
        void *dst = *(void *volatile *)&g_pWorldUnpackHead;
        int off = *(volatile int *)&g_nWorldTimAudioChunkOffset;

        StartCdReadAsync(lba, dst, g_nWorldTimAudioChunkSize + t,
                         off + LEVEL_ARCHIVE_OFFSET(intro) - t, 600);
      }
    } else {
      g_nGemPickupSplitFlag = 0;
    }
    break;
  case 4:
    if (g_nGemPickupSplitFlag == 1) {
      SetSpuTransferStartAddr(0x81010 - g_nGemPickupReadByteCount);
      {
        int len = g_nGemPickupReadByteCount - 0x100000;

        WriteSpuRam(g_pWorldUnpackHead, g_nWorldTimAudioChunkSize + len);
      }
      while (GetSpuTransferStatus(0) == 0) {
      }
    }
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&g_pWorldUnpackHead;
      int intro = *(volatile int *)&g_nLevelIntroIndex;
      int size = *(volatile int *)&g_nWorldDataExtChunkSize;
      int off = *(volatile int *)&g_nWorldDataExtChunkOffset;

      StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro), 600);
    }
    break;
  case 5:
    RelocateMobActorTable(
        HEAD + (g_anActorMeshOffsetList - g_nWorldDataExtChunkOffset));
    for (i = 0; i < 0x200; i++) {
      if (((int)g_apActorMeshTableBlock[i] & 0xFFFFFF) <
          ((int)g_apWorldUnpackHeadBlock[7] & 0xFFFFFF)) {
        g_apActorMeshTableBlock[i] = 0;
      }
    }
    i = 1;
    mesh = g_apActorMeshTableBlock;
    list = g_anActorMeshOffsetBlock;
    slot = (short *)(list + 0x40) + 1;
    for (; i < 0x40; i++) {
      if (list[i] <= 0) {
        break;
      }
      mesh[*slot++] = RelocateActorMeshHeader((char *)g_pWorldUnpackHead +
                                              (list[i] - list[-16]));
    }
    break;
  default:
    return;
  }
  g_anGemPickupStreamStateBlock[0]++;
}
