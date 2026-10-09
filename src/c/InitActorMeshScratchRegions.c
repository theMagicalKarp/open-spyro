#include "globals.h"

extern void FillWord(void *dst, unsigned int value, int byte_count);

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
  g_pEmitListBuffer = top - 0x2000;
  do {
  } while (0);
  g_pRenderScratchOtBase = top - 0x6000;
  top -= 0x6008;
  g_pRenderScratchPrimTop = top;
  if (split) {
    size = -0x13000;
    top += size;
  } else {
    size = -0x1c000;
    top += size;
  }
  g_pRenderScratchPrimBase1 = top;
  top += size;
  g_pRenderScratchRegionBase = top;
  __asm__ volatile("");
  g_pFramePrimBufferBase0 = VREAD(g_pRenderScratchRegionBase);
  g_pFramePrimBufferBase1 = VREAD(g_pRenderScratchPrimBase1);
  g_pFrameOtMergedChainSlot1 = g_pFrameOtMergedChainSlot0 =
      VREAD(g_pRenderScratchPrimTop);
  g_pFrameOtDepthBinBase1 = g_pFrameOtDepthBinBase0 =
      VREAD(g_pRenderScratchOtBase);
  FillWord(g_pFrameOtMergedChainSlot0, 0, 8);
  FillWord(g_pRenderScratchOtBase, 0, 0x4000);
}
