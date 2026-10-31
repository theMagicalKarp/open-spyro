#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

#define HEAD (g_apWorldUnpackHeadBlock[0])
#define HDR (g_anDragonRescueHeaderEndBlock[0])
#define CHUNK (g_anDragonRescueSpuChunkBlock[0])

#define DRAGON_ASSET_OFFSET(i) ((&g_nDragonAssetOffset)[(i) * 2])
#define DRAGON_ASSET_SIZE(i) ((&g_nDragonAssetSize)[(i) * 2])
#define LEVEL_ARCHIVE_OFFSET(i) ((&g_nLevelArchiveOffset)[(i) * 4])

/* 0x80014b70 (0x800): streams the dragon-rescue (dragon rescue) assets while
   the pickup overlay runs. Each tick, once the drive is idle and the music
   stream table is built, it runs one stage: read the dragon asset (split in
   two when it does not fit the unpack area), relocate the statue/mirror
   models and camera path, upload the voice sample to SPU RAM in 32 KB
   chunks through the spare primitive buffer, then reload the level's
   TIM/audio tail and ext chunk and re-relocate the level's moby meshes. */
void TickDragonRescueAssetStream(void) {
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
  if (CallCdSync(1, 0) != 2) {
    return;
  }
  if (!(g_dwMusicStreamStatus & 0x40)) {
    return;
  }

  switch (g_nDragonRescueStreamState) {
  case 0:
    avail = g_nWorldUnpackLimit - (int)g_pWorldUnpackHead;
    size = DRAGON_ASSET_SIZE(g_nDragonRescueTypeSnapshot);
    if (avail >= size) {
      g_nDragonRescueReadByteCount = size;
      g_nDragonRescueSplitFlag = 0;
    } else {
      g_nDragonRescueReadByteCount = avail / 0x800 * 0x800;
      g_nDragonRescueSplitFlag = 1;
    }
    StartCdReadAsync(g_nCdBaseLba, g_pWorldUnpackHead,
                     g_nDragonRescueReadByteCount,
                     DRAGON_ASSET_OFFSET(g_nDragonRescueTypeSnapshot) +
                         LEVEL_ARCHIVE_OFFSET(g_nLevelIndex),
                     600);
    g_nDragonRescueStreamState++;
    return;
  case 1:
    CopyWords(g_abDragonRescueAssetHeader, HEAD, 0x24);
    if (g_nDragonRescueSplitFlag == 0) {
      g_pDragonRescueCameraPath = HEAD + g_nDragonRescueCameraPathOffset;
      g_nDragonRescueCameraPathCount = g_nDragonRescueCameraPathSize / 24;
      g_pDragonRescueStatueModelData =
          RelocateActorMeshHeader(HEAD + g_nDragonRescueHeaderEndOffset);
      g_pDragonRescueMirrorModelData =
          RelocateActorMeshHeader(HEAD + g_nDragonRescueMirrorMeshOffset);
      g_nDragonRescueSpuChunkIndex = 0;
    } else {
      SetSpuTransferStartAddr(0x80000 - g_nDragonRescueAudioSampleSize);
      WriteSpuRam(HEAD + g_nDragonRescueAudioSampleOffset,
                  g_nDragonRescueAudioSampleSize);
      while (GetSpuTransferStatus(0) == 0) {
      }
      CopyWords(HEAD, HEAD + HDR, g_nDragonRescueReadByteCount - HDR);
      StartCdReadAsync(g_nCdBaseLba, HEAD + g_nDragonRescueReadByteCount - HDR,
                       DRAGON_ASSET_SIZE(g_nDragonRescueTypeSnapshot) -
                           g_nDragonRescueReadByteCount,
                       DRAGON_ASSET_OFFSET(g_nDragonRescueTypeSnapshot) +
                           LEVEL_ARCHIVE_OFFSET(g_nLevelIndex) +
                           g_nDragonRescueReadByteCount,
                       600);
    }
    break;
  case 2:
    if (g_nDragonRescueSubstate < 2) {
      return;
    }
    if (g_nDragonRescueSplitFlag == 0) {
      {
        register int top asm("$2") = 0x80000;
        SetSpuTransferStartAddr(top + (g_nDragonRescueSpuChunkIndex << 15) -
                                g_nDragonRescueAudioSampleSize);
      }
      if (g_pActiveFrameDrawEnv == &g_abFrameDrawEnv0) {
        buf = g_pFramePrimBufferBase1;
      } else {
        buf = g_pFramePrimBufferBase0;
      }
      if (g_nDragonRescueAudioSampleSize <
          (g_nDragonRescueSpuChunkIndex << 15) + 0x8000) {
        n = g_nDragonRescueAudioSampleSize % 0x8000;
      } else {
        n = 0x8000;
      }
      ReadSpuRam(buf, n);
      while (GetSpuTransferStatus(0) == 0) {
      }
      {
        register int top asm("$2") = 0x80000;
        SetSpuTransferStartAddr(top + (CHUNK << 15) -
                                g_nDragonRescueAudioSampleSize);
      }
      WriteSpuRam(g_pWorldUnpackHead + g_nDragonRescueAudioSampleOffset +
                      (CHUNK << 15),
                  n);
      while (GetSpuTransferStatus(0) == 0) {
      }
      CopyWords(g_pWorldUnpackHead + g_nDragonRescueAudioSampleOffset +
                    (g_nDragonRescueSpuChunkIndex << 15),
                buf, n);
      g_nDragonRescueSpuChunkIndex++;
      if ((g_nDragonRescueSpuChunkIndex << 15) <
          g_nDragonRescueAudioSampleSize) {
        return;
      }
      g_nDragonRescueStreamState++;
      return;
    }
    {
      char **h = g_apWorldUnpackHeadBlock;
      register char *p asm("$3");

      p = *h + g_nDragonRescueCameraPathOffset;
      g_pDragonRescueCameraPath = p - g_nDragonRescueHeaderEndOffset;
      g_nDragonRescueCameraPathCount = g_nDragonRescueCameraPathSize / 24;
      g_pDragonRescueStatueModelData = RelocateActorMeshHeader(*h);
      g_pDragonRescueMirrorModelData =
          RelocateActorMeshHeader(*h + g_nDragonRescueMirrorMeshOffset -
                                  g_nDragonRescueHeaderEndOffset);
    }
    g_nDragonRescueStreamState++;
    return;
  case 3:
    if (g_nDragonRescueSubstate < 6) {
      return;
    }
    if (g_nDragonRescueSplitFlag != 1) {
      break;
    }
    t = (g_nDragonRescueAudioSampleSize + 0x180F) / 0x800 * 0x800;
    g_nDragonRescueReadByteCount = t;
    if (0x100000 - g_nWorldTimAudioChunkSize < t) {
      t -= 0x100000;
      {
        int lba = *(volatile int *)&g_nCdBaseLba;
        int intro = *(volatile int *)&g_nLevelIndex;
        void *dst = *(void *volatile *)&g_pWorldUnpackHead;
        int off = *(volatile int *)&g_nWorldTimAudioChunkOffset;

        StartCdReadAsync(lba, dst, g_nWorldTimAudioChunkSize + t,
                         off + LEVEL_ARCHIVE_OFFSET(intro) - t, 600);
      }
    } else {
      g_nDragonRescueSplitFlag = 0;
    }
    break;
  case 4:
    if (g_nDragonRescueSplitFlag == 1) {
      SetSpuTransferStartAddr(0x81010 - g_nDragonRescueReadByteCount);
      {
        int len = g_nDragonRescueReadByteCount - 0x100000;

        WriteSpuRam(g_pWorldUnpackHead, g_nWorldTimAudioChunkSize + len);
      }
      while (GetSpuTransferStatus(0) == 0) {
      }
    }
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&g_pWorldUnpackHead;
      int intro = *(volatile int *)&g_nLevelIndex;
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
    slot = ACTOR_MESH_INDEX_LIST(list) + 1;
    for (; i < 0x40; i++) {
      if (list[i] <= 0) {
        break;
      }
      mesh[*slot++] =
          RelocateActorMeshHeader(g_pWorldUnpackHead + (list[i] - list[-16]));
    }
    break;
  default:
    return;
  }
  g_anDragonRescueStreamStateBlock[0]++;
}
