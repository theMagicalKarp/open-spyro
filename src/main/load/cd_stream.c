#include "cd_stream.h"
#include "camera.h"
#include "cd_music.h"
#include "cutscene_id.h"
#include "cutscene_path.h"
#include "dragon_rescue.h"
#include "funcs.h"
#include "globals.h"
#include "libcd.h"
#include "main_shared.h"
#include "match_asm.h"
#include "portal.h"
#include "sky.h"
#include "spu.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* 0x80014564, 1548 bytes — the id-bundle CD streaming state machine.
   Ticked from Initialize, PauseMenu_Update, Cutscene_Update,
   StoryIntro_Update and TickDemoPlayback; each tick services the stall
   watchdog, then gates on the drive being idle (no read in flight,
   CdSync == 2, music stream table built) and runs exactly one stage of
   g_nCdStreamState:

     0  kick the 0x800-byte id header read into g_pLoadStagingBuffer
     1  copy the header to the chunk-descriptor scratch, kick the TIM chunk
     2  DMA the TIM chunk from g_pLoadStagingBuffer into VRAM at (512, 0)
     3  kick the remaining TIM/audio bytes above the 0x80000 split
     4  upload the audio half to SPU RAM, kick the id bundle read
     5  unpack the bundle, snapshot the section-4 header, kick the ext chunk
        (id 1 streams it in two parts, everyone else in one)
     6  waiting for the first half of id 1's ext chunk
     7  kick the second half of id 1's ext chunk
     8  relocate every actor mesh header, kick the path-table chunk
     9  rebase the path table, seed the actor pool defaults, done
    10  idle — the stream stays here until a caller resets the state

   The cutscene id (g_nCutsceneId) indexes the per-cutscene archive table
   (8-byte stride) and the per-cutscene header-size table (4-byte stride). */

/* Register pins (`off` in v1, `pb` in a1) reproduce the original's
   local-alloc choices in arms 7 and 8: a hard reg is outside qty priority,
   so the competing temps fall into the order the original has. */
void TickCutsceneBundleLoadStream(void) {
  RECT tim;
  int pad[4];
  int state;
  int extSize;
  int archiveOff;
  int i;
  int def;
  int *unpacked;
  void *volatile *buf;
  int *ent;
  int *head;
  Actor *actor;
  RestartStalledCdRead();
  if ((*((volatile int *)g_anCdReadInFlightBlock)) != 0) {
    return;
  }
  if (CallCdSync(1, 0) != 2) {
    return;
  }
  if ((g_dwMusicStreamStatus & MUSIC_ST_IDLE) == 0) {
    return;
  }
  state = g_nCdStreamState;
  if (state == CSTREAM_READ_HEADER) {
    int lba = *((volatile int *)(&g_nCdBaseLba));
    int id = *((volatile int *)(&g_nCutsceneId));
    void *dst = *((void *volatile *)(&g_pLoadStagingBuffer));
    archiveOff = (&g_nCutsceneArchiveOffset)[id * 2];
    StartCdReadAsync(lba, dst, CD_SECTOR_BYTES, archiveOff,
                     CD_STALL_TIMEOUT_TICKS);
    g_nCdStreamState = CSTREAM_READ_TIM;
  } else if (state == CSTREAM_READ_TIM) {
    int lba;
    int id;
    void *dst;
    CopyWords(g_anWorldTimAudioChunkOffsetBlock, g_apLoadStagingBufferBlock[0],
              ARCHIVE_DESC_BYTES);
    lba = *((volatile int *)(&g_nCdBaseLba));
    id = *((volatile int *)(&g_nCutsceneId));
    dst = *((void *volatile *)(&g_apLoadStagingBufferBlock[0]));
    StartCdReadAsync(lba, dst, (&g_nCutsceneHeaderChunkSize)[id],
                     g_anWorldTimAudioChunkOffsetBlock[0] +
                         (&g_nCutsceneArchiveOffset)[id * 2],
                     CD_STALL_TIMEOUT_TICKS);
    g_nCdStreamState = CSTREAM_UPLOAD_TIM;
    return;
  } else if (state == CSTREAM_UPLOAD_TIM) {
    tim.x = TIM_PAGE_X;
    tim.y = 0;
    tim.w = TIM_PAGE_W;
    /* one 512-pixel 16-bit row per 0x400 bytes */
    tim.h = (&g_nCutsceneHeaderChunkSize)[g_nCutsceneId] >> 10;
    LoadImage(&tim, g_pLoadStagingBuffer);
    g_nCdStreamState = CSTREAM_READ_AUDIO;
  } else if (state == CSTREAM_READ_AUDIO) {
    int lba = *((volatile int *)(&g_nCdBaseLba));
    void *dst = *((void *volatile *)(&g_pLoadStagingBuffer));
    int size =
        (*((volatile int *)(&g_nWorldTimAudioChunkSize))) - TIM_PAIR_BYTES;
    int id = *((volatile int *)(&g_nCutsceneId));
    int off = *((volatile int *)(&g_nWorldTimAudioChunkOffset));
    StartCdReadAsync(lba, dst, size,
                     (off + (&g_nCutsceneArchiveOffset)[id * 2]) +
                         TIM_PAIR_BYTES,
                     CD_STALL_TIMEOUT_TICKS);
    g_nCdStreamState = CSTREAM_UPLOAD_AUDIO;
  } else if (state == CSTREAM_UPLOAD_AUDIO) {
    SetSpuTransferStartAddr(SPU_SAMPLE_BASE);
    WriteSpuRam(g_pLoadStagingBuffer, SPU_RAM_SIZE - SPU_SAMPLE_BASE);
    while (GetSpuTransferStatus(0) == 0) {
    }

    {
      int lba = *((volatile int *)(&g_nCdBaseLba));
      void *dst = *((void *volatile *)(&g_pLevelDataBuffer));
      int id = *((volatile int *)(&g_nCutsceneId));
      int size = *((volatile int *)(&g_nWorldBundleChunkSize));
      int off = *((volatile int *)(&g_nWorldBundleChunkOffset));
      StartCdReadAsync(lba, dst, size,
                       off + (&g_nCutsceneArchiveOffset)[id * 2],
                       CD_STALL_TIMEOUT_TICKS);
    }
    g_nCdStreamState = CSTREAM_UNPACK;
  } else if (state == CSTREAM_UNPACK) {
    unpacked = UnpackWorldDataChunks(g_pLevelDataBuffer, 1);
    *((SkyHeader *)(&g_nSkyChunkCount)) =
        *((SkyHeader *)(&g_nLevelSkyChunkCount));
    g_pWorldUnpackHead = (char *)unpacked;
    if (g_nCutsceneId == CUTSCENE_OPENING) {
      g_nCdStreamState = CSTREAM_EXT_WAIT;
      extSize = g_nWorldDataExtChunkSize - 0x60000;
    } else {
      extSize = g_nWorldDataExtChunkSize;
      g_nCdStreamState = CSTREAM_RELOCATE;
    }
    {
      int lba = *((volatile int *)(&g_nCdBaseLba));
      int id = *((volatile int *)(&g_nCutsceneId));
      void *dst = *((void *volatile *)(&g_pWorldUnpackHead));
      /* Hard-register pins (A374 / F14j): each holds the register the
         original uses for it, which settles the competing temps. */
      register int off asm("$3") =
          *((volatile int *)(&g_nWorldDataExtChunkOffset));
      StartCdReadAsync(lba, dst, extSize,
                       off + (&g_nCutsceneArchiveOffset)[id * 2],
                       CD_STALL_TIMEOUT_TICKS);
    }
    return;
  } else if (state == CSTREAM_EXT_WAIT) {
    g_nCdStreamState = CSTREAM_EXT_SECOND_HALF;
  } else if (state == CSTREAM_EXT_SECOND_HALF) {
    void *head2 = *((void *volatile *)(&g_pWorldUnpackHead));
    int size = *((volatile int *)(&g_nWorldDataExtChunkSize));
    int lba = *((volatile int *)(&g_nCdBaseLba));
    int id = *((volatile int *)(&g_nCutsceneId));
    register int off asm("$3") =
        *((volatile int *)(&g_nWorldDataExtChunkOffset));
    int arch = (&g_nCutsceneArchiveOffset)[id * 2] - 0x60000;
    StartCdReadAsync(lba, (((char *)head2) + size) - 0x60000, 0x60000,
                     (off + size) + arch, 0x258);
    g_nCdStreamState = CSTREAM_RELOCATE;
  } else if (state == CSTREAM_RELOCATE) {
    if (g_anActorMeshOffsetBlock[0] > 0) {
      i = 0;
      do {
        g_apActorMeshTableBlock[i + 1] = RelocateActorMeshHeader(
            (int *)((g_pWorldUnpackHead) + (g_anActorMeshOffsetBlock[i] -
                                            g_anActorMeshOffsetBlock[-16])));
        i++;
      } while (g_anActorMeshOffsetBlock[i] > 0);
    }
    {
      register void *volatile *pb asm("$5") = g_apLevelArchiveBufferBlock;
      void *head2 = *((void *volatile *)(&g_pWorldUnpackHead));
      int extsz = *((volatile int *)(&g_nWorldDataExtChunkSize));
      int lba = *((volatile int *)(&g_nCdBaseLba));
      int size;
      int off;
      int count;
      int id;
      void *dst;
      pb[0] = ((char *)head2) + extsz;
      size = *((volatile int *)(&g_nLevelArchiveChunkSize));
      dst = pb[0];
      off = *((volatile int *)(&g_nLevelArchiveChunkOffset));
      g_nLevelArchiveByteSize = size;
      count = *((volatile int *)(&g_nLevelArchiveByteSize));
      id = *((volatile int *)(&g_nCutsceneId));
      g_nLevelArchiveBaseLba = off;
      StartCdReadAsync(lba, dst, count,
                       off + (&g_nCutsceneArchiveOffset)[id * 2],
                       CD_STALL_TIMEOUT_TICKS);
    }
    g_nCdStreamState = CSTREAM_REBASE_PATHS;
  } else if (state == CSTREAM_REBASE_PATHS) {
    i = 0;
    buf = g_apLevelArchiveBufferBlock;
    ent = buf[0];
    g_pPathTableHead = (CutscenePath *)ent;
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
    g_pActorListBase =
        (Actor *)(((char *)g_pLevelArchiveBuffer) + g_nLevelArchiveByteSize);
    i = 0;
    if (g_pPathTableHead->actorCount > 0) {
      def = 0x20;
    actor_loop:
      InitActorRenderDefaults(&g_pActorListBase[i]);

      /* int sum, not &g_pActorListBase[i]: that swaps the addu operands */
      actor = (Actor *)((i * ACTOR_SIZE) + (int)g_pActorListBase);
      actor->cullRadius = def;
      actor->type = i + 1;
      i += 1;
      if (i < g_pPathTableHead->actorCount) {
        goto actor_loop;
      }
    }
    g_pActorListBase[g_pPathTableHead->actorCount].mode = ACTOR_MODE_END;
    g_nCdStreamState = CSTREAM_DONE;
  }
}

#define HEAD (g_apWorldUnpackHeadBlock[0])
#define HDR (g_anDragonRescueHeaderEndBlock[0])
#define CHUNK (g_anDragonRescueSpuChunkBlock[0])

#define DRAGON_ASSET_OFFSET(i) ((&g_nDragonAssetOffset)[(i) * 2])
#define DRAGON_ASSET_SIZE(i) ((&g_nDragonAssetSize)[(i) * 2])
#define LEVEL_ARCHIVE_OFFSET(i) ((&g_nLevelArchiveOffset)[(i) * 4])

/* 0x80014b70 (0x800): streams the dragon-rescue assets while
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
  if (!(g_dwMusicStreamStatus & MUSIC_ST_IDLE)) {
    return;
  }

  switch (g_nDragonRescueStreamState) {
  case RESCUE_STREAM_READ:
    avail = (int)g_apActorMeshTable_FA -
            (int)g_pWorldUnpackHead; /* up to the statue's model */
    size = DRAGON_ASSET_SIZE(g_nDragonRescueTypeSnapshot);
    if (avail >= size) {
      g_nDragonRescueReadByteCount = size;
      g_nDragonRescueSplitFlag = 0;
    } else {
      g_nDragonRescueReadByteCount = avail / CD_SECTOR_BYTES * CD_SECTOR_BYTES;
      g_nDragonRescueSplitFlag = 1;
    }
    StartCdReadAsync(g_nCdBaseLba, g_pWorldUnpackHead,
                     g_nDragonRescueReadByteCount,
                     DRAGON_ASSET_OFFSET(g_nDragonRescueTypeSnapshot) +
                         LEVEL_ARCHIVE_OFFSET(g_nLevelIndex),
                     CD_STALL_TIMEOUT_TICKS);
    g_nDragonRescueStreamState++;
    return;
  case RESCUE_STREAM_RELOCATE:
    CopyWords(g_abDragonRescueAssetHeader, HEAD, 0x24);
    if (g_nDragonRescueSplitFlag == 0) {
      g_pDragonRescueCameraPath = HEAD + g_nDragonRescueCameraPathOffset;
      g_nDragonRescueCameraPathCount = g_nDragonRescueCameraPathSize / 24;
      g_pDragonRescueStatueModelData = RelocateActorMeshHeader(
          (int *)(HEAD + g_nDragonRescueHeaderEndOffset));
      g_pDragonRescueMirrorModelData = RelocateActorMeshHeader(
          (int *)(HEAD + g_nDragonRescueMirrorMeshOffset));
      g_nDragonRescueSpuChunkIndex = 0;
    } else {
      SetSpuTransferStartAddr(SPU_RAM_SIZE - g_nDragonRescueAudioSampleSize);
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
                       CD_STALL_TIMEOUT_TICKS);
    }
    break;
  case RESCUE_STREAM_UPLOAD_VOICE:
    if (g_nDragonRescueSubstate < DRAGON_RESCUE_LOAD_WAIT) {
      return;
    }
    if (g_nDragonRescueSplitFlag == 0) {
      {
        register int top asm("$2") =
            SPU_RAM_SIZE; /* v0 as in the original (A374) */
        SetSpuTransferStartAddr(top + (g_nDragonRescueSpuChunkIndex << 15) -
                                g_nDragonRescueAudioSampleSize);
      }
      if (g_pActiveFrameDrawEnv == &g_FrameDrawEnv0) {
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
        register int top asm("$2") =
            SPU_RAM_SIZE; /* v0 as in the original (A374) */
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
      register char *p asm("$3"); /* v1, as in the original (A374) */

      p = *h + g_nDragonRescueCameraPathOffset;
      g_pDragonRescueCameraPath = p - g_nDragonRescueHeaderEndOffset;
      g_nDragonRescueCameraPathCount = g_nDragonRescueCameraPathSize / 24;
      g_pDragonRescueStatueModelData = RelocateActorMeshHeader((int *)*h);
      g_pDragonRescueMirrorModelData =
          RelocateActorMeshHeader((int *)(*h + g_nDragonRescueMirrorMeshOffset -
                                          g_nDragonRescueHeaderEndOffset));
    }
    g_nDragonRescueStreamState++;
    return;
  case RESCUE_STREAM_RELOAD_LEVEL:
    if (g_nDragonRescueSubstate < DRAGON_RESCUE_COUNT) {
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
                         off + LEVEL_ARCHIVE_OFFSET(intro) - t,
                         CD_STALL_TIMEOUT_TICKS);
      }
    } else {
      g_nDragonRescueSplitFlag = 0;
    }
    break;
  case RESCUE_STREAM_LEVEL_AUDIO:
    if (g_nDragonRescueSplitFlag == 1) {
      SetSpuTransferStartAddr(SPU_RAM_SIZE + SPU_SAMPLE_BASE -
                              g_nDragonRescueReadByteCount);
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

      StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro),
                       CD_STALL_TIMEOUT_TICKS);
    }
    break;
  case RESCUE_STREAM_LEVEL_MESHES:
    InstallSpyroAnimStream(
        HEAD + (g_anActorMeshOffsetList - g_nWorldDataExtChunkOffset));
    for (i = 0; i < ACTOR_MESH_TABLE_SLOTS; i++) {
      if (((int)g_apActorMeshTableBlock[i] & 0xFFFFFF) <
          ((int)g_apWorldUnpackHeadBlock[7] & 0xFFFFFF)) {
        g_apActorMeshTableBlock[i] = 0;
      }
    }
    i = 1;
    mesh = g_apActorMeshTableBlock;
    list = g_anActorMeshOffsetBlock;
    slot = ACTOR_MESH_INDEX_LIST(list) + 1;
    for (; i < MESH_STREAM_MAX; i++) {
      if (list[i] <= 0) {
        break;
      }
      mesh[*slot++] = RelocateActorMeshHeader(
          (int *)(g_pWorldUnpackHead + (list[i] - list[-16])));
    }
    break;
  default:
    return;
  }
  g_anDragonRescueStreamStateBlock[0]++;
}

#undef HDR
#undef CHUNK
#undef DRAGON_ASSET_OFFSET
#undef DRAGON_ASSET_SIZE
#undef LEVEL_ARCHIVE_OFFSET

#define SKY g_anSkyBlock /* the active sky header, word by word (SkyHeader) */
#define CAM g_anCameraPosBlock
#define SPYB ((unsigned char *)g_anSpyroWorldPosBlock)

/* Offsets from the held Spyro pointers below: pe is the euler-Z byte, pyaw
   the body-yaw word; both reach the rest of the block relative to themselves.
 */
#define PE_TO(word) ((word) * 4 - SPYB_EULER_Z)
#define PE_WORD(word) (*(int *)(pe + PE_TO(word)))
#define PYAW_BACK_TO(byte) (SPY_BODY_YAW * 4 - (byte))
#define LEVEL_DATA_BUF (g_apLevelDataBufferBlock[0])
#define LEVEL_SPLASH_OFFSET(i) ((&g_nLevelSplashOffset)[(i) * 4])
#define LEVEL_SPLASH_SIZE(i) ((&g_nLevelSplashByteSize)[(i) * 4])
#define LEVEL_ARCHIVE_OFFSET(i) ((&g_nLevelArchiveOffset)[(i) * 4])

/* 0x80015370 (0x1074): the level-transition loader. Each tick (once the
   drive is idle past the first two stages) it runs one stage: preserve the
   mesh chunks that must survive the swap, pick the new level's intro index,
   read the splash, the archive header, the TIM/audio chunk (uploading the
   TIMs to VRAM and the sample bank to SPU RAM), the world bundle and its ext
   chunk, relocate the moby meshes and the path table, wait for Spyro to face
   the scripted heading, then build the level runtime and place Spyro and the
   camera at the spawn point. */
void TickLevelTransitionStream(int full) {
  RECT rect;
  int pad[8];
  int size;
  int i;

  if (g_nCdStreamState > LSTREAM_FRESH) {
    RestartStalledCdRead();
    if (*(volatile int *)g_anCdReadInFlightBlock != 0) {
      return;
    }
    if (CallCdSync(1, 0) != 2) {
      return;
    }
    if (!(g_dwMusicStreamStatus & MUSIC_ST_IDLE)) {
      return;
    }
  }

  switch (g_nCdStreamState) {
  case LSTREAM_PRESERVE: {
    int *p;
    char *base;
    int n;
    int k;
    char *src;

    StopAllSoundExceptMask(0);
    if (g_nEnteredPortalIdx >= 0) {
      char *rec;

      rec = (char *)g_apPortals[g_nEnteredPortalIdx]->pSky;
      src = rec + 0x14;
      size = *(int *)src + 0x400;
      p = (int *)((char *)g_pRenderScratchRegionBase - size);
      CopyWords(p, rec, size);
      p += 5;
      base = (char *)p;
      p++;
      SKY[4] = *p++;
      n = *p++;
      SKY[1] = (int)p;
      SKY[0] = n;
      for (k = 0; k < SKY[0]; k++) {
        *p = (base + *p) - src;
        p++;
      }
    } else {
      src = (char *)SKY[1] - 0xC;
      size = *(int *)src + 0x400;
      p = (int *)((char *)g_pRenderScratchRegionBase - size);
      CopyWords(p, src, size);
      base = (char *)p;
      p += 2;
      n = *p++;
      SKY[1] = (int)p;
      SKY[0] = n;
      for (k = 0; k < SKY[0]; k++) {
        *p = (base + *p) - src;
        p++;
      }
    }
    p = (int *)((char *)g_pRenderScratchRegionBase - size -
                g_nLevelSampleBankCount);
    CopyWords(p, g_pLevelSampleBankHeader, g_nLevelSampleBankCount);
    RelocateLevelSampleBank(p, 0);
    g_nCameraVisZone = -1;
    g_nCdStreamState = LSTREAM_SELECT_LEVEL;
  } break;
  case LSTREAM_FRESH:
    StopAllSoundExceptMask(0);
    g_nSkyChunkCount = 0;
    g_nCameraVisZone = -1;
    g_nCdStreamState++;
  case LSTREAM_SELECT_LEVEL: {
    int world;
    int intro;

    g_nActiveLevelId = g_nNextLevelId;
    ApplyPerLevelGlobalsTable();
    g_nPrevLevelIndex = g_nLevelIndex;
    world = g_nActiveLevelId / 10 - 1;
    g_nActiveWorldIndex = world;
    intro = world * 6 + g_nActiveLevelId % 10;
    g_nLevelIndex = intro;
    if (full) {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&g_pSharedLoadBuffer;

      int size = LEVEL_SPLASH_SIZE(intro);
      int off = LEVEL_SPLASH_OFFSET(intro);

      EBB_BREAK();
      StartCdReadAsync(lba, dst, size, off, CD_STALL_TIMEOUT_TICKS);
    }
    g_nCdStreamState++;
  } break;
  case LSTREAM_READ_HEADER: {
    int lba = *(volatile int *)&g_nCdBaseLba;
    int intro = *(volatile int *)&g_nLevelIndex;
    void *dst = *(void *volatile *)&g_pLevelDataBuffer;
    int off = LEVEL_ARCHIVE_OFFSET(intro);

    StartCdReadAsync(lba, dst, CD_SECTOR_BYTES, off, CD_STALL_TIMEOUT_TICKS);
  }
    g_nCdStreamState++;
    break;
  case LSTREAM_READ_TIM:
    CopyWords(g_anWorldTimAudioChunkOffsetBlock, LEVEL_DATA_BUF,
              ARCHIVE_DESC_BYTES);
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      int intro = *(volatile int *)&g_nLevelIndex;
      void *dst = *(void *volatile *)g_apLevelDataBufferBlock;
      int off = *(volatile int *)g_anWorldTimAudioChunkOffsetBlock;

      StartCdReadAsync(lba, dst, TIM_PAIR_BYTES,
                       off + LEVEL_ARCHIVE_OFFSET(intro),
                       CD_STALL_TIMEOUT_TICKS);
    }
    g_nEnteredPortalIdx = 0;
    g_nCdStreamState++;
    break;
  case LSTREAM_UPLOAD_TIM:
    rect.x = TIM_PAGE_X;
    rect.w = TIM_PAGE_W;
    rect.h = TIM_PAGE_H;
    rect.y = g_nEnteredPortalIdx * TIM_PAGE_H;
    LoadImage(&rect,
              (char *)LEVEL_DATA_BUF + g_nEnteredPortalIdx * TIM_PAGE_BYTES);
    if (++g_nEnteredPortalIdx == 2) {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = LEVEL_DATA_BUF;
      int size = *(volatile int *)&g_nWorldTimAudioChunkSize;
      int intro = *(volatile int *)&g_nLevelIndex;
      int off = *(volatile int *)&g_nWorldTimAudioChunkOffset;

      g_nCdStreamState++;

      StartCdReadAsync(lba, dst, size - TIM_PAIR_BYTES,
                       off + LEVEL_ARCHIVE_OFFSET(intro) + TIM_PAIR_BYTES,
                       CD_STALL_TIMEOUT_TICKS);
    }
    break;
  case LSTREAM_UPLOAD_AUDIO:
    SetSpuTransferStartAddr(SPU_SAMPLE_BASE);
    WriteSpuRam(g_pLevelDataBuffer, SPU_RAM_SIZE - SPU_SAMPLE_BASE);
    g_nCdStreamState++;
    break;
  case LSTREAM_WAIT_AUDIO:
    if (GetSpuTransferStatus(0) != 0) {
      g_nCdStreamState++;
    }
    break;
  case LSTREAM_READ_BUNDLE: {
    int lba = *(volatile int *)&g_nCdBaseLba;
    void *dst = *(void *volatile *)&g_pLevelDataBuffer;
    int intro = *(volatile int *)&g_nLevelIndex;
    int size = *(volatile int *)&g_nWorldBundleChunkSize;
    int off = *(volatile int *)&g_nWorldBundleChunkOffset;

    StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro),
                     CD_STALL_TIMEOUT_TICKS);
  }
    g_nCdStreamState++;
    break;
  case LSTREAM_UNPACK:
    g_pWorldUnpackHead = UnpackWorldDataChunks(g_pLevelDataBuffer, 0);
    if (SKY[0] != 0 && g_nGamestate != GS_GAME_OVER &&
        g_nGamestate != GS_ACTOR_CUTSCENE) {
      if (g_nReturnPortalLevelId != 0) {
        for (i = 0; i < g_nPortalCount; i++) {
          Portal *e = g_apPortals[i];

          if (e->levelId == g_nReturnPortalLevelId) {
            *(SkyHeader *)SKY = *(SkyHeader *)e->pSky;
            break;
          }
        }
      } else {
        *(SkyHeader *)SKY = *(SkyHeader *)&g_nLevelSkyChunkCount;
      }
    }
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      void *dst = *(void *volatile *)&g_pWorldUnpackHead;
      int intro = *(volatile int *)&g_nLevelIndex;
      int size = *(volatile int *)&g_nWorldDataExtChunkSize;
      int off = *(volatile int *)&g_nWorldDataExtChunkOffset;

      StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro),
                       CD_STALL_TIMEOUT_TICKS);
    }
    g_nCdStreamState++;
    break;
  case LSTREAM_RELOCATE: {
    int *list;
    short *slot;
    void **mesh;

    if (full) {
      InstallSpyroAnimStream(g_pWorldUnpackHead + (g_anActorMeshOffsetList -
                                                   g_nWorldDataExtChunkOffset));
    }
    for (i = 0; i < ACTOR_MESH_TABLE_SLOTS; i++) {
      if (((int)g_apActorMeshTableBlock[i] & 0xFFFFFF) <
          ((int)g_apWorldUnpackHeadBlock[7] & 0xFFFFFF)) {
        g_apActorMeshTableBlock[i] = 0;
      }
    }
    i = 1;
    mesh = g_apActorMeshTableBlock;
    list = g_anActorMeshOffsetBlock;
    slot = ACTOR_MESH_INDEX_LIST(list) + 1;
    for (; i < MESH_STREAM_MAX; i++) {
      if (list[i] <= 0) {
        break;
      }
      mesh[*slot++] = RelocateActorMeshHeader(
          (int *)(g_pWorldUnpackHead + (list[i] - list[-16])));
    }
    g_pLevelArchiveBuffer = g_pWorldUnpackHead + list[-15];
    g_nLevelArchiveByteSize = list[-13];
    g_nLevelArchiveBaseLba = list[-14];
    g_nCdStreamState++;
  } break;
  case LSTREAM_READ_PATHS: {
    int lba = *(volatile int *)&g_nCdBaseLba;
    void *dst = *(void *volatile *)&g_pLevelArchiveBuffer;
    int intro = *(volatile int *)&g_nLevelIndex;
    int size = *(volatile int *)&g_nLevelArchiveByteSize;
    int off = *(volatile int *)&g_nLevelArchiveBaseLba;

    StartCdReadAsync(lba, dst, size, off + LEVEL_ARCHIVE_OFFSET(intro),
                     CD_STALL_TIMEOUT_TICKS);
  }
    g_nCdStreamState++;
    break;
  case LSTREAM_WAIT_SPIN: {
    int d;

    if (g_nPortalTransitionActive == 0) {
      g_nCdStreamState++;
      break;
    }
    if (g_nTransitionSkyPitchTilt != 0) {
      break;
    }
    if (g_nReturnPortalLevelId != 0) {
      d = (g_nSpyroBodyYaw + g_nCameraYaw) & ANGLE12_MASK;
      if (d > ANGLE12_HALF) {
        d -= ANGLE12_TURN;
      }
      if (abs(d) < 0x80) {
        g_nCdStreamState++;
      }
    } else {
      d = (g_abSpyroPersistentEuler[2] - g_abSpyroRequestEuler[2]) &
          ANGLE8_MASK;
      if (d > ANGLE8_HALF) {
        d -= ANGLE8_TURN;
      }
      if (abs(d) < 0x10) {
        g_nCdStreamState++;
      }
    }
  } break;
  case LSTREAM_FINALIZE: {
    if (g_abLevelVisitedFlag[g_nLevelIndex] == 0) {
      g_nCdMusicStreamTimeout = MUSIC_REPICK_FIRST_VISIT_TICKS;
    } else {
      g_nCdMusicStreamTimeout = MUSIC_REPICK_TICKS;
    }
    g_nCurrentMusicTrack = (&g_nLevelMusicTrack)[g_nLevelIndex];
    g_abLevelVisitedFlag[g_nLevelIndex] = 1;
    FillWord(g_anSpawnAnchorBlock, 0, 0x68);
    g_nLevelKeyState = KEYSTATE_NONE;
    g_nGemTallyIconCount = 0;
    g_nLevelGemsAtEntry = g_anLevelGemsCollected[g_nLevelIndex];
    BuildLoadedLevelRuntime(full);
    if (g_nPortalTransitionActive != 0) {
      if (g_nFlightLevelActive != 0) {
        g_anSpyroSubstateBlock[0] = 10;
        ComputeCameraToTargetAngle(g_anCameraPos);
        g_nCameraYaw += g_nSpyroBodyYaw;
        CopyVector(&g_anSpyroSubstateBlock[SPY_POS - SPY_SUBSTATE],
                   &g_anSpawnAnchorBlock[0x14] /* g_anLevelSpawnPos */);
        *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
        g_abSpyroPersistentEuler[2] = g_nLevelSpawnYaw;
        g_nSpyroBodyYaw = (g_nLevelSpawnYaw & ANGLE8_MASK) << 4;
        g_nCameraSpringYaw -= SPYB[SPYB_EULER_Z] << 4;
        ComputeCameraOrbitOffset(g_anCameraPos);
        AddVector(g_anCameraPos, g_anCameraPos,
                  &g_anSpyroSubstateBlock[SPY_POS - SPY_SUBSTATE]);
        UpdateCameraEulerAngles();
        CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPos);
        ActivateScriptedCameraView(
            SCRIPTED_CAM_VIEW_AT_POS(g_anSpyroDeathCamRecordBlock));
        g_nGamestate = GS_ARRIVAL_CAMERA;
      } else if (g_nReturnPortalLevelId != 0) {
        for (i = 0; i < g_nPortalCount; i++) {
          Portal *e = g_apPortals[i];
          unsigned char *pe = SPYB + SPYB_EULER_Z;
          int *pyaw = (int *)(SPYB + SPY_BODY_YAW * 4);
          int *cam = CAM;
          int *spring = CAM + 0x14;

          if (e->levelId == g_nReturnPortalLevelId) {
            unsigned char yaw;
            PortalState *guide;

            yaw = *pe;
            guide = g_pActorListBase[e->actor].state;
            EBB_BREAK();
            ChangeSpyroState(SPYRO_ST_GLIDE);
            PE_WORD(SPY_SUBSTATE) = SPYRO_SUB_GLIDE_TO_TARGET;
            ComputeCameraToTargetAngle(cam);
            g_anCameraSpringYawBlock[6] /* g_nCameraYaw */ =
                g_nCameraYaw + PE_WORD(SPY_BODY_YAW);
            if (guide->returnReversed != 0) {
              CopyVector((int *)(pe - SPYB_EULER_Z), guide->desc->point[0].pos);
              CopyVector((int *)(pe + PE_TO(SPY_REQUEST_TARGET)),
                         guide->desc->point[1].pos);
              *pe = ArcTan2(
                  guide->desc->point[1].pos[0] - guide->desc->point[0].pos[0],
                  guide->desc->point[1].pos[1] - guide->desc->point[0].pos[1],
                  ATAN_ANGLE8);
            } else {
              CopyVector((int *)(pe - SPYB_EULER_Z), guide->desc->point[1].pos);
              CopyVector((int *)(pe + PE_TO(SPY_REQUEST_TARGET)),
                         guide->desc->point[0].pos);
              *pe = ArcTan2(
                  guide->desc->point[0].pos[0] - guide->desc->point[1].pos[0],
                  guide->desc->point[0].pos[1] - guide->desc->point[1].pos[1],
                  ATAN_ANGLE8);
            }
            *(CameraAngles *)spring = ((CameraAngles *)spring)[1];
            *pyaw = *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_EULER_Z)) << 4;
            *spring -= *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_EULER_Z))
                       << 4;
            ComputeCameraOrbitOffset(cam);
            AddVector(cam, cam, (int *)((char *)pyaw - PYAW_BACK_TO(0)));
            UpdateCameraEulerAngles();
            *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_OT_BIN_BIAS)) = 0x7F;
            g_nPortalSkyPitchTilt = 0;
            g_nPortalSkyYawSpin =
                (yaw - *((unsigned char *)pyaw - PYAW_BACK_TO(SPYB_EULER_Z)))
                << 4;
            break;
          }
        }
      } else {
        int *yawp;
        int *pos;

        ChangeSpyroState(SPYRO_ST_GLIDE);
        ComputeCameraToTargetAngle(g_anCameraPos);
        yawp = (int *)(SPYB + SPY_BODY_YAW * 4);
        pos = (int *)SPYB;
        g_nCameraYaw += *yawp;
        *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
        g_abSpyroPersistentEuler[2] = g_nLevelSpawnYaw;
        *yawp = (g_nLevelSpawnYaw & 0xFF) << 4;
        g_nCameraSpringYaw -= SPYB[SPYB_EULER_Z] << 4;
        CopyVector(pos, &g_anSpawnAnchorBlock[0x14] /* g_anLevelSpawnPos */);
        ComputeCameraOrbitOffset(g_anCameraPos);
        ZeroVector(yawp - 4);
        if (g_pCameraTargetParams == g_anCameraParamTransitionGlideIn) {
          g_anSpyroWorldPosZBlock[0] = g_nSpyroWorldPosZ - 0x1600;
          pos[0] -= (g_anCosineLut[g_abSpyroPersistentEuler[2]] * 5) >> 1;
          g_nSpyroWorldPosY -=
              (g_anSineLut[g_abSpyroPersistentEuler[2]] * 5) >> 1;
          AddVector(g_anCameraPos, g_anCameraPos, pos);
          UpdateCameraEulerAngles();
          g_nCameraNextMode = g_abSpyroStateCameraModeTable[g_nSpyroState];
          g_nSpyroSubstate = SPYRO_SUB_GLIDE_ENTER;
          g_nGamestate = GS_GAMEPLAY;
          g_bSpyroOtBinBias = 4;
        } else {
          AddVector(g_anCameraPos, g_anCameraPos, pos);
          UpdateCameraEulerAngles();
          CopyVector(g_anSpyroDeathCamRecordBlock, g_anCameraPos);
          ActivateScriptedCameraView(
              SCRIPTED_CAM_VIEW_AT_POS(g_anSpyroDeathCamRecordBlock));
          g_nSpyroSubstate = SPYRO_SUB_GLIDE_ARRIVE;
          g_nGamestate = GS_ARRIVAL_CAMERA;
        }
      }
    } else if (g_nFlightLevelActive != 0) {
      g_nSpyroSubstate = 0;
    } else if (g_nGamestate != GS_ACTOR_CUTSCENE) {
      ChangeSpyroState(SPYRO_ST_IDLE);
    }
    *(SkyHeader *)&g_nSkyChunkCount = *(SkyHeader *)&g_nLevelSkyChunkCount;
    g_nPortalTransitionActive = 0;
    g_nReturnPortalLevelId = 0;
    g_anLevelRunnerArrived = 0;
    g_nLevelRunnerArrivedGoal1 = 0;
    g_anLevelEscapeRunCount = 0;
    g_nLevelEscapeRunCountLane1 = 0;
    g_nLevelVortexSeen1 = 0;
    g_nLevelVortexSeen2 = 0;
    g_bLevelUseShortReleaseAnim = 0;
    g_bLevelBossDefeated = 0;
    g_bLevelPhasedBossKilled = 0;
    g_nCreditsReturnLevelId = -1;
    g_nCdStreamState = LSTREAM_DONE;
  } break;
  }
}

#undef SKY
#undef SPYB
#undef PE_TO
#undef PE_WORD
#undef PYAW_BACK_TO
#undef LEVEL_DATA_BUF
#undef LEVEL_SPLASH_OFFSET
#undef LEVEL_SPLASH_SIZE
#undef LEVEL_ARCHIVE_OFFSET

/* 0x800163e4: watchdog-driven CD stall recovery. If a read armed a stall
   timeout (g_nCdStallTimeoutTicks) and the vblank watchdog has passed it,
   resets the CD subsystem,
   reinstalls the read callback, resets the music-stream state, busy-waits
   for the drive to reach state 2, then reissues the pending sector read. */
void RestartStalledCdRead(void) {
  int timeout;

  timeout = g_nCdStallTimeoutTicks;
  if (timeout != 0) {
    if (timeout < g_nCdStallWatchdogTicks) {
      CdInit();
      CdReadCallback(CdReadCompleteCallback);
      g_dwMusicStreamStatus = MUSIC_ST_IDLE;
      g_nLastCdMusicCommand = 0;
      g_nPendingMusicCommand = 0;

      do {
      } while (CallCdSync(1, 0) != 2);

      g_nCdStallWatchdogTicks = 0;
      CdRead(g_nCdReadSectorCount, g_pCdReadDstBuffer, CdlModeSpeed);
    }
  }
}

/* CD read-completion callback (registered via CdReadCallback). On CdlComplete
   (intr == 2) the read finished cleanly, so clear the in-flight flag and the
   stall-watchdog state; on any other interrupt the read stalled, so re-seek
   (CdControl) and re-issue the original CdRead. (0x80016490, 112 bytes.) */
void CdReadCompleteCallback(unsigned char intr) {
  if (intr == 2) {
    g_nCdReadInFlight = 0;
    g_nCdStallWatchdogTicks = 0;
    g_nCdStallTimeoutTicks = 0;
  } else {
    CdControl(CdlSetloc, &g_abCdlLocScratch, 0);
    CdRead(g_nCdReadSectorCount, g_pCdReadDstBuffer, CdlModeSpeed);
  }
}

/* 0x80016500 — blocking CD sector read, StartCdReadAsync's synchronous twin.
   Spins until the drive is idle (no read in flight, CdSync == 2, music
   stream table built) while servicing the stall watchdog, sound voices and
   the music stream; then sets 2x speed (CdlSetmode 0x80), seeks to
   lbaBase + offset/0x800, kicks CdRead of (size+0x7ff)/0x800 sectors to dst
   with the in-flight bookkeeping raised, and spins on the CdlLOC scratch
   completion word + CdSync until the read drains. */
void CdReadSyncSectors(int lbaBase, void *dst, int size, int offset,
                       int stallTicks) {
  unsigned char mode[4];
  byte *loc;
  int pos;
  int sectors;
  int *flag;
  int *inflight;
  int idle;

  inflight = &g_nCdReadInFlight;
  idle = 2;
  mode[0] = CdlModeSpeed;
  for (;;) {
    if (*inflight == 0 && CallCdSync(1, 0) == idle &&
        (g_dwMusicStreamStatus & MUSIC_ST_IDLE)) {
      break;
    }
    RestartStalledCdRead();
    TickActiveSoundVoices();
    TickCdMusicStream();
  }

  CdControl(CdlSetmode, mode, 0);
  g_nCdStallTimeoutTicks = stallTicks;
  g_nCdStallWatchdogTicks = 0;

  pos = lbaBase + offset / CD_SECTOR_BYTES;
  loc = g_abCdlLocScratchBlock;
  CdIntToPos(pos, loc);
  CdControl(CdlSetloc, loc, 0);

  sectors = (size + CD_SECTOR_BYTES - 1) / CD_SECTOR_BYTES;
  g_nCdReadSectorCount = sectors;
  g_pCdReadDstBuffer = dst;
  g_nCdReadInFlight = 1;
  CdRead(sectors, dst, CdlModeSpeed);

  flag = (int *)(loc + 0x20); /* g_nCdReadInFlight */
  for (;;) {
    if (*flag == 0 && CallCdSync(1, 0) == 2) {
      break;
    }
    RestartStalledCdRead();
  }
}

/* 0x80016698 — fire an async CD sector read without waiting: set 2x speed
   (CdlSetmode 0x80), seek to lbaBase + offset/0x800, then CdRead
   (size+0x7ff)/0x800 sectors to dst. Stashes the in-flight bookkeeping
   (sector count / dst / stallTicks for RestartStalledCdRead) and raises the
   in-flight flag; callers poll it themselves. */
void StartCdReadAsync(int lbaBase, void *dst, int size, int offset,
                      int stallTicks) {
  unsigned char mode[4];
  byte *loc;
  int pos;
  int sectors;

  mode[0] = CdlModeSpeed;
  CdControl(CdlSetmode, mode, 0);
  g_nCdStallWatchdogTicks = 0;
  g_nCdStallTimeoutTicks = stallTicks;

  pos = lbaBase + offset / CD_SECTOR_BYTES;
  loc = &g_abCdlLocScratch;
  CdIntToPos(pos, loc);
  CdControl(CdlSetloc, loc, 0);

  sectors = (size + CD_SECTOR_BYTES - 1) / CD_SECTOR_BYTES;
  g_nCdReadSectorCount = sectors;
  g_pCdReadDstBuffer = dst;
  g_nCdReadInFlight = 1;
  CdRead(sectors, dst, CdlModeSpeed);
}
