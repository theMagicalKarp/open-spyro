#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Steer an actor's yaw toward Spyro (0x80038fc8, 0x260). Computes the
   signed wrap-around delta between the ArcTan2 bearing to Spyro and the
   tracked yaw at *yawp, picks/holds a turn direction in *dirp (dir 0
   seeds from the delta sign; an established direction only flips once
   the opposite-side delta exceeds `thresh`), and advances *yawp by the
   gamestate-scaled `speed` clamped to the remaining delta. While more
   than 0x101 off-bearing it swaps the actor's anim pair (anim..frameRate)
   (animId when turning positive, animId+1 otherwise) through
   StubActorHook. Stores the coarse yaw in rotZ; returns the delta.

   In each anim arm the 0x10 stores are bare literals (a shared carrier costs
   the birthing boost and swaps the `li` with the call-arg copy), and `old` is
   a `$2` register local: as a pseudo it spans the cross-jumped tail, so it is
   a global allocno assigned after local-alloc has already given v0 to the
   constant. */
int SteerActorYawTowardSpyro(Actor *actor, int *dirp, int *yawp, int speed,
                             int thresh, int animId) {
  int ret;
  int yaw;
  int d;
  int d2;
  int ad;
  int dir;
  int next;
  int t;

  ret = ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                g_anSpyroWorldPos[1] - actor->posY, 1);
  yaw = *yawp;
  d = (ret - yaw) & 0xFFF;
  ret -= 0x800;
  d2 = (ret - yaw) & 0xFFF;
  if (d >= 0x801) {
    d -= 0x1000;
  }
  if (d2 >= 0x801) {
    d2 -= 0x1000;
  }
  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }
  ad = abs(d);
  if (ad < speed) {
    speed = ad;
  }
  dir = *dirp;
  if (dir == 0) {
    if (d < 0) {
      *dirp = -1;
    } else {
      *dirp = 1;
    }
  } else if (d < 0) {
    if (dir > 0) {
      if (thresh < abs(d2)) {
        *dirp = -1;
      }
    }
  } else if (dir < 0 && d > 0) {
    if (thresh < abs(d2)) {
      *dirp = 1;
    }
  }
  if (abs(d) >= 0x101) {
    next = animId + 1;
    if (*dirp == 1) {
      if (actor->nextAnim != animId) {
        register int old asm("$2");
        int p3F;
        g_nActorAnimDone = 0;
        old = actor->nextAnim;
        p3F = actor->nextFrame;
        actor->frameProgress = 0x10;
        actor->frameRate = 0x10;
        actor->nextAnim = animId;
        actor->nextFrame = 0;
        actor->anim = old;
        actor->frame = p3F;
        StubActorHook(actor, p3F, next);
      }
    } else {
      if (actor->nextAnim != next) {
        register int old asm("$2");
        int p3F;
        g_nActorAnimDone = 0;
        old = actor->nextAnim;
        p3F = actor->nextFrame;
        actor->frameProgress = 0x10;
        actor->frameRate = 0x10;
        actor->nextAnim = next;
        actor->nextFrame = 0;
        actor->anim = old;
        actor->frame = p3F;
        StubActorHook(actor, p3F, next);
      }
    }
  }
  t = *yawp + speed * *dirp;
  *yawp = t;
  if (t < 0) {
    *yawp = t + 0x1000;
  }
  actor->rotZ = *yawp >> 4;
  return d;
}
