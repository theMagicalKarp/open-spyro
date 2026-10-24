#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Wander/patrol behaviour step for an actor (0x80039AA8, 0x3ec).

   `wb` is the actor's WanderBlock (actor_type.h): home point, speed/turn
   rate, re-pick/jitter ranges, leash and chase radii, weave and cooldowns.

   Each frame: age the re-pick countdown and, on expiry, jitter the heading by
   a random signed amount and re-arm the timer; add the weave offset (bouncing
   the weave direction at the amplitude limit); turn toward the resulting
   heading and take a guided-move step. On a blocked step, pick a fresh
   backwards-ish heading. Otherwise chase Spyro when he is inside 0x44C (or
   inside chaseRadius), else walk back home once past the leash, else just
   tick the two cooldowns down. */
void UpdateActorWander(Actor *actor, WanderBlock *wb) {
  int unused[2];
  int *spyro;
  int turn;
  int speed;
  int aligned;
  unsigned char timer;
  int jitter;
  int heading;
  int weave;
  int amp;
  int dist;
  int home;
  int hit;

  turn = wb->turnRate;
  speed = wb->speed;
  aligned = 1;
  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
    turn += turn >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
    turn <<= 1;
  }

  timer = wb->repickTimer;
  wb->repickTimer = timer + 0xFF; /* wrapping decrement */
  if (timer == 0) {
    jitter = RandomIntInRange(wb->jitterMin, wb->jitterMax);
    if (GetRandomU32() & 1) {
      jitter = -jitter;
    }
    wb->heading = (wb->heading + jitter + 0x100) % 0x100;
    wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
    wb->weave = 0;
    wb->chasing = 0;
  }

  heading = wb->heading;
  if (wb->weaveAmp != 0) {
    weave = wb->weave;
    heading += weave;
    weave += wb->weaveRate * wb->weaveDir;
    wb->weave = weave;
    amp = wb->weaveAmp;
    weave = (short)weave;
    if (weave < 0) {
      weave = -weave;
    }
    if (weave >= amp) {
      wb->weaveDir = -(int)wb->weaveDir;
    }
  }
  if (TurnActorYawTowardAngle(actor, heading, turn, 5, 1) == 0) {
    aligned = 0;
  }

  if (wb->contactCooldown != 0) {
    hit = MoveActorAlongHeading(actor, speed, 0, wb->contact, 0x55);
  } else {
    hit = MoveActorAlongHeading(actor, speed, wb->contact, wb->contact, 0x55);
  }

  if (hit != 0) {
    if (aligned != 0) {
      wb->heading = WrapAddAngle8(wb->heading, RandomIntInRange(0x40, 0xC0));
      wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
      wb->chasing = 0;
      if (hit & 2) {
        wb->chaseCooldown = 0x14;
      } else {
        wb->contactCooldown = 6;
      }
    }
  } else {
    spyro = g_anSpyroWorldPos;
    dist = ApproxDist2D(&actor->posX, spyro);
    if (dist < 0x44C && wb->chasing == 0) {
      wb->heading =
          ArcTan2(actor->posX - spyro[0], actor->posY - g_nSpyroWorldPosY, 0);
      wb->heading = WrapAddAngle8(wb->heading, RandomIntInRange(-0x3C, 0x3C));
      wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
      wb->chasing = 1;
      return;
    }
    if (wb->chaseRadius != 0 && wb->chasing == 0 && wb->chaseCooldown == 0 &&
        dist < (int)(wb->chaseRadius << 10)) {
      wb->heading = ArcTan2(actor->posX - g_anSpyroWorldPos[0],
                            actor->posY - g_nSpyroWorldPosY, 0);
      wb->heading = WrapAddAngle8(wb->heading, RandomIntInRange(-0x30, 0x30));
      wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
      wb->chasing = 1;
      return;
    }
    home = ApproxDist2D(&actor->posX, &wb->homeX);
    if ((int)(wb->leash << 10) < home) {
      wb->heading =
          ArcTan2(wb->homeX - actor->posX, wb->homeY - actor->posY, 0);
      wb->chasing = 0;
      wb->chaseCooldown = 0x14;
      wb->repickTimer = home / (int)wb->speed;
    } else {
      if (wb->chaseCooldown != 0) {
        wb->chaseCooldown = wb->chaseCooldown - 1;
      }
      if (wb->contactCooldown != 0) {
        wb->contactCooldown = wb->contactCooldown - 1;
      }
    }
  }
}
