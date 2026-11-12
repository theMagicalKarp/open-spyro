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
   OnActorAnimQueued. Stores the coarse yaw in rotZ; returns the delta.

   In each anim arm the 0x10 stores are bare literals (a shared carrier costs
   the birthing boost and swaps the `li` with the call-arg copy), and `old` is
   a `$2` register local: as a pseudo it spans the cross-jumped tail, so it is
   a global allocno assigned after local-alloc has already given v0 to the
   constant. */
int SteerActorYawTowardSpyro(Actor *actor, int *dirp, int *yawp, int speed,
                             int thresh, int animId) {
  int bearing;
  int yaw;
  int delta;
  int oppDelta;
  int absDelta;
  int dir;
  int next;
  int newYaw;

  bearing = ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                    g_anSpyroWorldPos[1] - actor->posY, ATAN_ANGLE12);
  yaw = *yawp;
  delta = (bearing - yaw) & 0xFFF;
  bearing -= 0x800;
  oppDelta = (bearing - yaw) & 0xFFF;
  if (delta >= 0x801) {
    delta -= 0x1000;
  }
  if (oppDelta >= 0x801) {
    oppDelta -= 0x1000;
  }
  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }
  absDelta = abs(delta);
  if (absDelta < speed) {
    speed = absDelta;
  }
  dir = *dirp;
  if (dir == 0) {
    if (delta < 0) {
      *dirp = -1;
    } else {
      *dirp = 1;
    }
  } else if (delta < 0) {
    if (dir > 0) {
      if (thresh < abs(oppDelta)) {
        *dirp = -1;
      }
    }
  } else if (dir < 0 && delta > 0) {
    if (thresh < abs(oppDelta)) {
      *dirp = 1;
    }
  }
  if (abs(delta) >= 0x101) {
    next = animId + 1;
    if (*dirp == 1) {
      if (actor->nextAnim != animId) {
        /* Hard-register pins (A374 / F14j): each holds the register the
           original uses for it, which settles the competing temps. */
        register int old asm("$2");
        int oldFrame;
        g_nActorAnimDone = 0;
        old = actor->nextAnim;
        oldFrame = actor->nextFrame;
        actor->frameProgress = ANIM_RATE_TICKS(4);
        actor->frameRate = ANIM_RATE_TICKS(4);
        actor->nextAnim = animId;
        actor->nextFrame = 0;
        actor->anim = old;
        actor->frame = oldFrame;
        OnActorAnimQueued(actor, oldFrame, next);
      }
    } else {
      if (actor->nextAnim != next) {
        register int old asm("$2");
        int oldFrame;
        g_nActorAnimDone = 0;
        old = actor->nextAnim;
        oldFrame = actor->nextFrame;
        actor->frameProgress = ANIM_RATE_TICKS(4);
        actor->frameRate = ANIM_RATE_TICKS(4);
        actor->nextAnim = next;
        actor->nextFrame = 0;
        actor->anim = old;
        actor->frame = oldFrame;
        OnActorAnimQueued(actor, oldFrame, next);
      }
    }
  }
  newYaw = *yawp + speed * *dirp;
  *yawp = newYaw;
  if (newYaw < 0) {
    *yawp = newYaw + 0x1000;
  }
  actor->rotZ = *yawp >> 4;
  return delta;
}
