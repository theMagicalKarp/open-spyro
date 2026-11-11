#include "cd_music.h"
#include "cd_stream.h"
#include "cutscene_id.h"
#include "cutscene_path.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sky.h"
#include "spu.h"

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
