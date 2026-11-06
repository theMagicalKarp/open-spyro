/* frame_db.h — the two 0x84-byte double-buffer frame records at
 * g_FrameDrawEnv0 (0x80076ee0) and g_FrameDrawEnv1 (0x80076f64).
 * InitGraphicsHardware fills the DRAWENV/DISPENV pair of each, and
 * GamestateDraw flips g_pActiveFrameDrawEnv between them every frame and
 * reloads the primitive buffer and OT pointers from the record's tail. */

#ifndef OPEN_SPYRO_FRAME_DB_H
#define OPEN_SPYRO_FRAME_DB_H

#include "frame_arena.h"
#include "types.h"

typedef struct FrameDB {
  DRAWENV draw;            /* 0x00 g_FrameDrawEnv0/1 */
  DISPENV disp;            /* 0x5C g_FrameDispEnv0/1 */
  void *primBufferBase;    /* 0x70 g_pFramePrimBufferBase0/1 */
  void *otDepthBinBase;    /* 0x74 g_pFrameOtDepthBinBase0/1 */
  void *otMergedChainSlot; /* 0x78 g_pFrameOtMergedChainSlot0/1 */
  int pad7C[2];            /* 0x7C */
} FrameDB;                 /* 0x84 */

/* A frame record seen from its DRAWENV (g_pActiveFrameDrawEnv, &g_FrameDrawEnv0). */
#define FRAME_DB(env) ((FrameDB *)(env))

/* Each frame's primitive buffer, carved below g_pRenderScratchPrimTop by
 * InitActorMeshScratchRegions: 0x1C000 bytes normally, 0x13000 in the split
 * (cutscene) layout. */
#define PRIM_BUFFER_BYTES 0x1C000
#define PRIM_BUFFER_BYTES_SPLIT 0x13000

/* The pause / inventory / flight-results menus freeze the gameplay frame
 * (StoreImage) into the bytes just below g_pRenderScratchPrimTop: one
 * 0x100 x 0xE1 16-bit frame. */
#define MENU_SNAPSHOT_BYTES (0x100 * 0xE1 * 2) /* 0x1C200 */

#endif /* OPEN_SPYRO_FRAME_DB_H */
