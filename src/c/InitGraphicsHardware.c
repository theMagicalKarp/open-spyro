#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"

/* 0x800122a8 — one-time GPU bring-up: reset the GPU, build both frames'
   draw/disp environments (double-buffered halves of the 512x512 VRAM
   page), set the background-clear/dither flags, clear VRAM, and unmask
   the display. */
void InitGraphicsHardware(void) {
  RECT rect;
  FrameDB *base;

  SetDispMask(0);
  ResetGraph(0);
  SetGraphDebug(0);

  base = FRAME_DB(&g_FrameDrawEnv0);
  InitDefaultDrawEnv(&base[0].draw, 0, DRAW_Y, SCREEN_W, DRAW_H);
  InitDefaultDrawEnv(&base[1].draw, 0, SCREEN_H + DRAW_Y, SCREEN_W, DRAW_H);
  InitDefaultDispEnv(&base[0].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
  InitDefaultDispEnv(&base[1].disp, 0, 0, SCREEN_W, SCREEN_H);

  g_FrameDrawEnv1.ofs[1] = SCREEN_H;
  g_FrameDrawEnv0.isbg = 1;
  g_FrameDrawEnv1.isbg = 1;
  g_FrameDrawEnv0.dtd = 1;
  g_FrameDrawEnv1.dtd = 1;
  g_FrameDrawEnv0.ofs[1] = 0;
  rect.x = 0;
  rect.y = 0;
  rect.w = 0x200;
  rect.h = 0x200;
  ClearImage(&rect, 0, 0, 0);
  DrawSync(0);
  SetDispMask(1);
}
