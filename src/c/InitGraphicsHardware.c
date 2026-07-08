#include "globals.h"

extern void SetDispMask();
extern void ResetGraph();
extern void SetGraphDebug();
extern void InitDefaultDrawEnv();
extern void InitDefaultDispEnv();
extern void ClearImage();
extern void DrawSync();

/* 0x800122a8 — one-time GPU bring-up: reset the GPU, build both frames'
   draw/disp environments (double-buffered halves of the 512x512 VRAM
   page), set the background-clear/dither flags, clear VRAM, and unmask
   the display. */
void InitGraphicsHardware(void) {
  RECT rect;
  char *base;

  SetDispMask(0);
  ResetGraph(0);
  SetGraphDebug(0);

  base = (char *)&g_abFrameDrawEnv0;
  InitDefaultDrawEnv(base, 0, 8, 0x200, 0xE0);
  InitDefaultDrawEnv(base + 0x84, 0, 0xF8, 0x200, 0xE0);
  InitDefaultDispEnv(base + 0x5C, 0, 0xF0, 0x200, 0xF0);
  InitDefaultDispEnv(base + 0xE0, 0, 0, 0x200, 0xF0);

  g_abFrameDrawEnv1.ofs[1] = 0xF0;
  g_abFrameDrawEnv0.isbg = 1;
  g_abFrameDrawEnv1.isbg = 1;
  g_abFrameDrawEnv0.dtd = 1;
  g_abFrameDrawEnv1.dtd = 1;
  g_abFrameDrawEnv0.ofs[1] = 0;
  rect.x = 0;
  rect.y = 0;
  rect.w = 0x200;
  rect.h = 0x200;
  ClearImage(&rect, 0, 0, 0);
  DrawSync(0);
  SetDispMask(1);
}
