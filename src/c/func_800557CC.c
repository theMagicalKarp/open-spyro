#include "globals.h"
#include "main_shared.h"

extern short g_anCosineLut[]; /* 0x8006cc78 */

extern int LookupSine(unsigned int angle_12_4);
extern int LookupCosine(unsigned int angle_12_4);

/* The per-digit sub-record hung off a digit actor's +0x00 pointer. */
struct Digit {
  char pad00[4];
  int place;  /* 0x04 digit value scaled by its decimal place */
  int dir[3]; /* 0x08 camera-facing unit direction */
};

struct DigitActor {
  struct Digit *digit; /* 0x00 */
  char pad04[8];       /* 0x04 */
  int pos[3];          /* 0x0c */
  char pad18[0x2c];    /* 0x18 */
  unsigned char unk44; /* 0x44 */
  unsigned char unk45; /* 0x45 */
  unsigned char yaw;   /* 0x46 coarse camera yaw */
};

/* func_800557CC - VMA 0x800557cc, 400 bytes.

   Spawn one floating digit actor (id 0x104 + digit) per decimal digit of
   `value`, least significant first, walking away from `src` along the camera
   yaw so the digits read left to right on screen. Each digit carries the
   camera-facing direction (cos/sin of yaw - 0x400, plus a 0x80 z that steps
   down 0x10 per digit) and its own decimal place.

   Matching notes:
   - The overlay init hook takes TWO arguments (m2c reads the divide's
     `mfhi $a2` as a third), and the digit split is `d = value % 10;
     q = value / 10;` in that order, which gives the original's `move s0,s1`.
   - Each LUT load goes through an empty asm copy so combine cannot fuse the
     sign-extend into the shift (B13: `lh; sra 4`, not `lhu; sll; sra`).
   - The yaw store's load goes through a one-field struct view (A400) so it
     depends on the in-struct `rec->pos[2]` store and z lands in v1.
   - Tail: both LUT loads, then the rot[2] RMW, then a volatile barrier, then
     the asm copies. The birthing-boosted rot[2] reload is placed in the
     slot after its addiu's predecessor; with the cos asm copy below the
     barrier that slot goes to the cos LOAD, so the reload precedes it in
     sched1, idx2/cos take a0 and rot[2] v0, and sched2 can hoist the lhu up
     to the yaw lbu. The barrier also keeps the sh above the x/y updates
     (a free store would win the hazard tie and sink to the delay slot). */
void func_800557CC(int value, struct DigitActor *src) {
  short rot[3];
  struct DigitActor *rec;
  struct Digit *st;
  int x;
  int y;
  int place;
  int d;
  int a;
  int z;
  short *cam;
  int sx;
  int cx;
  int prod;
  cam = g_anCameraEulerYawBlock;
  place = 1;
  x = src->pos[0];
  y = src->pos[1];
  rot[0] = LookupCosine(cam[0] - 0x400) >> 8;
  rot[1] = LookupSine(cam[0] - 0x400) >> 8;
  rot[2] = 0x80;
  if (value != 0) {
    do {
      int q;
      d = value % 10;
      q = value / 10;
      prod = d + 0x104;
      rec =
          (*(struct DigitActor * (*)(int, int)) g_pfnLevelSpawnActor)(prod, 0);
      rec->pos[0] = x;
      rec->pos[1] = y;
      z = src->pos[2];
      prod = d * place;
      rec->unk44 = 0;
      rec->unk45 = 0;
      rec->pos[2] = z;
      st = rec->digit;
      rec->yaw = ((struct { unsigned short v; } *)&g_nCameraEulerYaw)->v >> 4;
      st->dir[0] = rot[0];
      st->dir[1] = rot[1];
      st->dir[2] = rot[2];
      value = q;
      place *= 10;
      st->place = prod;
      a = rec->yaw;
      sx = g_anSineLut[a];
      cx = g_anCosineLut[a];
      rot[2] -= 0x10;
      __asm__ volatile("");
      __asm__("" : "=r"(sx) : "0"(sx));
      __asm__("" : "=r"(cx) : "0"(cx));
      x -= sx >> 4;
      y += cx >> 4;
    } while (value != 0);
  }
}
