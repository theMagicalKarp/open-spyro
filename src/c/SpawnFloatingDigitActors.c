#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"

/* The per-digit sub-record hung off a digit actor's ->state pointer. */
struct Digit {
  char pad00[4];
  int place;  /* 0x04 digit value scaled by its decimal place */
  int dir[3]; /* 0x08 camera-facing unit direction */
};

/* SpawnFloatingDigitActors - VMA 0x800557cc, 400 bytes.

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
     depends on the in-struct `rec->posZ` store and z lands in v1.
   - Tail: both LUT loads, then the rot[2] RMW, then a volatile barrier, then
     the asm copies. The birthing-boosted rot[2] reload is placed in the
     slot after its addiu's predecessor; with the cos asm copy below the
     barrier that slot goes to the cos LOAD, so the reload precedes it in
     sched1, idx2/cos take a0 and rot[2] v0, and sched2 can hoist the lhu up
     to the yaw lbu. The barrier also keeps the sh above the x/y updates
     (a free store would win the hazard tie and sink to the delay slot). */
void SpawnFloatingDigitActors(int value, Actor *src) {
  short rot[3];
  Actor *rec;
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
  x = src->posX;
  y = src->posY;
  rot[0] = LookupCosine(cam[0] - 0x400) >> 8;
  rot[1] = LookupSine(cam[0] - 0x400) >> 8;
  rot[2] = 0x80;
  if (value != 0) {
    do {
      int q;
      d = value % 10;
      q = value / 10;
      prod = d + 0x104;
      rec = (*(Actor * (*)(int, int)) g_pfnLevelSpawnActor)(prod, 0);
      rec->posX = x;
      rec->posY = y;
      z = src->posZ;
      prod = d * place;
      rec->rotX = 0;
      rec->rotY = 0;
      rec->posZ = z;
      st = (struct Digit *)rec->state;
      rec->rotZ = ((struct { unsigned short v; } *)&g_nCameraEulerYaw)->v >> 4;
      st->dir[0] = rot[0];
      st->dir[1] = rot[1];
      st->dir[2] = rot[2];
      value = q;
      place *= 10;
      st->place = prod;
      a = rec->rotZ;
      sx = g_anSineLut[a];
      cx = g_anCosineLut[a];
      rot[2] -= 0x10;
      SCHED_FENCE();
      OPAQUE(sx);
      OPAQUE(cx);
      x -= sx >> 4;
      y += cx >> 4;
    } while (value != 0);
  }
}
