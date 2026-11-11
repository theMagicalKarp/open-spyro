/* cd_stream.h — g_nCdStreamState, shared by the two CD streaming state
 * machines (never active at once). Each stage runs once the drive is idle
 * (no read in flight, CdSync == 2, music stream ready). */

#ifndef OPEN_SPYRO_CD_STREAM_H
#define OPEN_SPYRO_CD_STREAM_H

/* Every CD read arms the stall watchdog with this many vblanks (10 s);
 * RestartStalledCdRead reissues a read the drive has not finished by then. */
#define CD_STALL_TIMEOUT_TICKS 600

/* A CD-ROM data sector (mode 2 form 1 payload). */
#define CD_SECTOR_BYTES 0x800

/* An archive's (level / cutscene / credits page) header sector opens with
 * its chunk descriptors, copied into g_nWorldTimAudioChunkOffset and on. */
#define ARCHIVE_DESC_BYTES 0x1D0
/* One 512 x 256 16-bit texture page as LoadImage takes it (VRAM x = 512).
 * A level or cutscene TIM/audio chunk opens with two of them; the bytes
 * above the split are the sample bank. */
#define TIM_PAGE_BYTES 0x40000
#define TIM_PAGE_X 0x200
#define TIM_PAGE_W 0x200
#define TIM_PAGE_H 0x100
#define TIM_PAIR_BYTES (2 * TIM_PAGE_BYTES)

/* g_apActorMeshTable's slot count, and the most models one stream installs
 * (g_anActorMeshOffsetList's 64 words). */
#define ACTOR_MESH_TABLE_SLOTS 0x200
#define MESH_STREAM_MAX 0x40

/* TickCutsceneBundleLoadStream: the bundle g_nCutsceneId selects. */
enum CutsceneStreamState {
  CSTREAM_READ_HEADER = 0,     /* kick the 0x800-byte header read */
  CSTREAM_READ_TIM = 1,        /* copy the header descriptors, kick the TIM chunk */
  CSTREAM_UPLOAD_TIM = 2,      /* LoadImage the TIMs at (512, 0) */
  CSTREAM_READ_AUDIO = 3,      /* kick the bytes above the 0x80000 split */
  CSTREAM_UPLOAD_AUDIO = 4,    /* WriteSpuRam the samples, kick the bundle read */
  CSTREAM_UNPACK = 5,          /* unpack the bundle, kick the ext chunk */
  CSTREAM_EXT_WAIT = 6,        /* cutscene 1 only: first half of the ext chunk */
  CSTREAM_EXT_SECOND_HALF = 7, /* cutscene 1 only: kick the second half */
  CSTREAM_RELOCATE = 8,        /* relocate actor meshes, kick the path table */
  CSTREAM_REBASE_PATHS = 9,    /* rebase the path table, seed the actor pool */
  CSTREAM_DONE = 10,
};

/* TickLevelTransitionStream: load g_nNextLevelId's world. */
enum LevelStreamState {
  LSTREAM_DONE = -1,
  LSTREAM_PRESERVE = 0,        /* keep the chunks that survive the swap (portal / exit) */
  LSTREAM_FRESH = 1,           /* nothing to keep */
  LSTREAM_SELECT_LEVEL = 2,    /* adopt the level id, kick the splash read */
  LSTREAM_READ_HEADER = 3,
  LSTREAM_READ_TIM = 4,        /* copy the header descriptors, kick the TIM chunk */
  LSTREAM_UPLOAD_TIM = 5,      /* LoadImage both texture pages, kick the rest */
  LSTREAM_UPLOAD_AUDIO = 6,    /* start the SPU sample upload */
  LSTREAM_WAIT_AUDIO = 7,
  LSTREAM_READ_BUNDLE = 8,
  LSTREAM_UNPACK = 9,          /* unpack the world, kick the ext chunk */
  LSTREAM_RELOCATE = 10,       /* relocate the moby table and meshes */
  LSTREAM_READ_PATHS = 11,     /* kick the path-table chunk */
  LSTREAM_WAIT_SPIN = 12,      /* wait for Spyro to face the scripted heading */
  LSTREAM_FINALIZE = 13,       /* BuildLoadedLevelRuntime, place Spyro and camera */
};

#endif /* OPEN_SPYRO_CD_STREAM_H */
