/* native/gate/gate_wasm.c — wasm32 side of the ILP32 gate.
 *
 * Linked with the real src/main/load/level.c. Replays the three loader steps
 * of a level transition (TickLevelTransitionStream) on PS1 RAM placed at its
 * real addresses:
 *
 *   GATE_UNPACK      UnpackWorldDataChunks on the world bundle
 *   GATE_RELOCATE    the LSTREAM_RELOCATE mesh loop (RelocateActorMeshHeader)
 *   GATE_BUILD       BuildLoadedLevelRuntime on the level archive
 *
 * The host copies each blob in between, as the CD stream would. Everything
 * the loader calls that is not under test is a stub here: the gate checks
 * relocations and published globals, not actor spawning or sound. */

#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

enum { GATE_UNPACK = 1, GATE_RELOCATE = 2, GATE_BUILD = 3 };

/* --- real helpers (hand-written asm on PS1; simple C is exact here) ------- */

void FillWord(void *dst, unsigned int value, int byte_count) {
  unsigned int *p = dst;

  for (; byte_count > 0; byte_count -= 4) {
    *p++ = value;
  }
}

void CopyVector(int *dst, int *src) {
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

/* --- stubs: out of the gate's scope ---------------------------------------- */

void StopAllSoundExceptMask(int mask) { (void)mask; }
void InitLightVectorConstants(void) {}
void ResetPadFrameSubstepTable(int *base) { (void)base; }
void ResetSpyroEntity(int full) { (void)full; }
void ChangeSpyroState(int state) { (void)state; }
void ResetCameraStateToTarget(void) {}
void ResetWorldChunkAnimations(void) {}
void srand(unsigned int seed) { (void)seed; }
void InsertActorIntoSpatialGrid(struct Actor *actor) { (void)actor; }
void HideActorRenderRecord(struct Actor *actor) { (void)actor; }
int FindGroundHeightBelow(int *pos, int depth) {
  (void)pos;
  (void)depth;
  return 0;
}
void EncodeHitNormalToActorGroundCode(struct Actor *actor) { (void)actor; }
void DespawnActorRecord(struct Actor *actor) { (void)actor; }
void ResetWorldParticleTables(void) {}
void InitHudCounters(int layout_butterflies) { (void)layout_butterflies; }
void HandleMusicCommand(int track, int cmd) {
  (void)track;
  (void)cmd;
}

/* The LSTREAM_RELOCATE body of TickLevelTransitionStream, minus the Spyro
 * anim stream (it patches Spyro's resident model, not the level blob). */
static void RelocateLevelMeshes(void) {
  int i;
  int *list;
  short *slot;
  void **mesh;

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
}

__attribute__((export_name("gate_run"))) int gate_run(int step) {
  switch (step) {
  case GATE_UNPACK:
    g_pWorldUnpackHead = UnpackWorldDataChunks(g_pLevelDataBuffer, 0);
    return (int)g_pWorldUnpackHead;
  case GATE_RELOCATE:
    RelocateLevelMeshes();
    return (int)g_pLevelArchiveBuffer;
  case GATE_BUILD:
    BuildLoadedLevelRuntime(0);
    return (int)g_pActorListBase;
  }
  return 0;
}

/* KUSEG mirror probe: a store through KSEG0 must read back through KUSEG. */
__attribute__((export_name("gate_mirror"))) int gate_mirror(int addr) {
  volatile int *kseg0 = (volatile int *)addr;
  volatile int *kuseg = (volatile int *)(addr & 0x7FFFFFFF);

  *kseg0 = 0x5350524F;
  if (*kuseg != 0x5350524F) {
    return 0;
  }
  *kuseg = ~0x5350524F;
  return *kseg0 == ~0x5350524F;
}
