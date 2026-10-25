/* frame_db.h — the two 0x84-byte double-buffer frame records at
 * g_abFrameDrawEnv0 (0x80076ee0) and g_abFrameDrawEnv1 (0x80076f64).
 * InitGraphicsHardware fills the DRAWENV/DISPENV pair of each, and
 * GamestateDraw flips g_pActiveFrameDrawEnv between them every frame and
 * reloads the primitive buffer and OT pointers from the record's tail. */

#ifndef OPEN_SPYRO_FRAME_DB_H
#define OPEN_SPYRO_FRAME_DB_H

#include "types.h"

typedef struct FrameDB {
  DRAWENV draw;            /* 0x00 g_abFrameDrawEnv0/1 */
  DISPENV disp;            /* 0x5C g_abFrameDispEnv0/1 */
  void *primBufferBase;    /* 0x70 g_pFramePrimBufferBase0/1 */
  void *otDepthBinBase;    /* 0x74 g_pFrameOtDepthBinBase0/1 */
  void *otMergedChainSlot; /* 0x78 g_pFrameOtMergedChainSlot0/1 */
  int pad7C[2];            /* 0x7C */
} FrameDB;                 /* 0x84 */

/* A frame record seen from its DRAWENV (g_pActiveFrameDrawEnv, &g_abFrameDrawEnv0). */
#define FRAME_DB(env) ((FrameDB *)(env))

#endif /* OPEN_SPYRO_FRAME_DB_H */
