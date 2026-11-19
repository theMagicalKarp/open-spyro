/* src/native/wasm/stub_main.c — the disc-free smoke test (`spyro --stub`,
 * the CI run): no game code, just PsyZ driven the way the game drives the
 * PSY-Q SDK. Opens the display (double-buffered 512x240, the game's mode) and
 * draws a spinning Gouraud triangle through an ordering table for `frames`
 * frames (0 = until the window is closed). The game itself starts from
 * game_main.c and needs the user's disc. */

#include <psyz.h>
#include <libetc.h>
#include <libgpu.h>

#define SCREEN_W 512
#define SCREEN_H 240
#define OT_LEN 8

typedef struct {
  DRAWENV draw;
  DISPENV disp;
  OT_TYPE ot[OT_LEN];
  POLY_G3 tri;
  TILE bg;
} Buffer;

static Buffer db[2];

/* 64-entry quarter-free sine table (x 4096) so the stub needs no libm. */
static int Sin12(int a) {
  static const short q[17] = {0,    401,  799,  1189, 1567, 1931,
                              2276, 2598, 2896, 3166, 3406, 3612,
                              3784, 3920, 4017, 4076, 4096};
  int i = (a >> 4) & 63; /* 64 steps per turn, from a 1024-unit angle */

  if (i < 16) {
    return q[i];
  }
  if (i < 32) {
    return q[32 - i];
  }
  if (i < 48) {
    return -q[i - 32];
  }
  return -q[64 - i];
}

__attribute__((export_name("native_stub_main"))) int
native_stub_main(int frames) {
  int frame;

  ResetGraph(0);
  SetGraphDebug(0);
  PadInit(0);
  SetDefDrawEnv(&db[0].draw, 0, 0, SCREEN_W, SCREEN_H);
  SetDefDispEnv(&db[0].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
  SetDefDrawEnv(&db[1].draw, 0, SCREEN_H, SCREEN_W, SCREEN_H);
  SetDefDispEnv(&db[1].disp, 0, 0, SCREEN_W, SCREEN_H);
  for (int i = 0; i < 2; i++) {
    SetPolyG3(&db[i].tri);
    setRGB0(&db[i].tri, 0xFF, 0x40, 0x40);
    setRGB1(&db[i].tri, 0x40, 0xFF, 0x40);
    setRGB2(&db[i].tri, 0x40, 0x40, 0xFF);
    SetTile(&db[i].bg);
    setRGB0(&db[i].bg, 0x20, 0x10, 0x40);
    setXY0(&db[i].bg, 0, 0);
    setWH(&db[i].bg, SCREEN_W, SCREEN_H);
  }
  SetDispMask(1);

  for (frame = 0; frames == 0 || frame < frames; frame++) {
    Buffer *b = &db[frame & 1];
    int a = frame * 8;
    int cx = SCREEN_W / 2;
    int cy = SCREEN_H / 2;
    int r = 90;

    ClearOTag(b->ot, OT_LEN);
    setXY3(&b->tri, cx + r * Sin12(a) / 4096, cy + r * Sin12(a + 256) / 4096,
           cx + r * Sin12(a + 341) / 4096, cy + r * Sin12(a + 597) / 4096,
           cx + r * Sin12(a + 683) / 4096, cy + r * Sin12(a + 939) / 4096);
    AddPrim(&b->ot[0], &b->bg); /* forward OT: slot 0 draws first */
    AddPrim(&b->ot[1], &b->tri);

    DrawSync(0);
    VSync(0);
    PutDrawEnv(&b->draw);
    PutDispEnv(&b->disp);
    DrawOTag(b->ot);
  }
  return frame;
}
