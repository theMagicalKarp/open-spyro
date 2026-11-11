#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "match_asm.h"

#define VREAD(sym) (*(void *volatile *)&(sym))

/* 0x8005b6f8 (0xe0) — carve the render scratch regions down from
   g_pWorkAreaTop (emit list, OT, primitive top, two primitive buffers of
   0x13000 or 0x1c000 bytes depending on `split`), publish them to both
   frame slots and clear the merged-chain head and the OT. The empty volatile
   asm keeps the first FillWord's `a1 = 0` below the RegionBase store. */
void InitActorMeshScratchRegions(int split) {
  register char *top asm("$2");
  int size;

  top = g_pWorkAreaTop;
  g_pEmitListBuffer = top - EMIT_LIST_BYTES;
  EBB_BREAK();
  g_pRenderScratchOtBase = top - (EMIT_LIST_BYTES + OT_BIN_BYTES);
  top -= EMIT_LIST_BYTES + OT_BIN_BYTES + OT_CHAIN_SLOT_BYTES;
  g_pRenderScratchPrimTop = top;
  if (split) {
    size = -PRIM_BUFFER_BYTES_SPLIT;
    top += size;
  } else {
    size = -PRIM_BUFFER_BYTES;
    top += size;
  }
  g_pRenderScratchPrimBase1 = top;
  top += size;
  g_pRenderScratchRegionBase = top;
  SCHED_FENCE();
  g_pFramePrimBufferBase0 = VREAD(g_pRenderScratchRegionBase);
  g_pFramePrimBufferBase1 = VREAD(g_pRenderScratchPrimBase1);
  g_pFrameOtMergedChainSlot1 = g_pFrameOtMergedChainSlot0 =
      VREAD(g_pRenderScratchPrimTop);
  g_pFrameOtDepthBinBase1 = g_pFrameOtDepthBinBase0 =
      VREAD(g_pRenderScratchOtBase);
  FillWord(g_pFrameOtMergedChainSlot0, 0, OT_CHAIN_SLOT_BYTES);
  FillWord(g_pRenderScratchOtBase, 0, OT_BIN_BYTES);
}
