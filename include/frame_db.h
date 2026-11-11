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
/* The regions above them, from g_pWorkAreaTop down: the emit list (256
 * 0x20-byte records), the depth-bin OT shared by both frames, and the
 * 8-byte merged-chain slot. */
#define EMIT_LIST_BYTES 0x2000
#define OT_BIN_COUNT 0x800 /* depth bins, 8 bytes each (LinkOTPrimitives walks them all) */
#define OT_BIN_BYTES (OT_BIN_COUNT * 8)
#define OT_CHAIN_SLOT_BYTES 8

/* The pause / inventory / flight-results menus freeze the gameplay frame
 * (StoreImage) into the bytes just below g_pRenderScratchPrimTop: one
 * 0x100 x 0xE1 16-bit frame. */
#define MENU_SNAPSHOT_W 0x100
#define MENU_SNAPSHOT_H 0xE1
#define MENU_SNAPSHOT_BYTES (MENU_SNAPSHOT_W * MENU_SNAPSHOT_H * 2) /* 0x1C200 */
/* ...because the frozen frame overwrites that VRAM: MENU_BACKDROP_STRIPS
 * strips of the drawing area, MENU_BACKDROP_STRIP_W wide, are read back,
 * packed to 8-bit luma (MENU_BACKDROP_LUMA_PIXELS each, half as many VRAM
 * columns) and reloaded from MENU_BACKDROP_X; the grey CLUT row goes to
 * (MENU_BACKDROP_X, MENU_BACKDROP_CLUT_Y). The backdrop quads draw them as
 * 8-bit pages MENU_BACKDROP_TPAGE + i through MENU_BACKDROP_CLUT. */
#define MENU_BACKDROP_STRIPS 4
#define MENU_BACKDROP_STRIP_W 0x80
#define MENU_BACKDROP_LUMA_PIXELS 0x7000 /* MENU_BACKDROP_STRIP_W x 0xE0 */
#define MENU_BACKDROP_X 0x200
#define MENU_BACKDROP_CLUT_Y 0xE0
#define MENU_BACKDROP_CLUT 0x3820  /* the CLUT id of (MENU_BACKDROP_X, MENU_BACKDROP_CLUT_Y) */
#define MENU_BACKDROP_TPAGE 0x88   /* 8-bit texture page at x = MENU_BACKDROP_X */

#endif /* OPEN_SPYRO_FRAME_DB_H */
