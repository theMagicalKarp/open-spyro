#include "globals.h"

/* Wander/patrol behaviour step for an actor (0x80039AA8, 0x3ec).

   `home` is the actor's behaviour block: the home point in words 0/1, then a
   byte parameter set — +0xC move speed, +0xD turn rate (both boosted 1.5x/2x
   in camera modes 3/4), +0xE contact group & radius, +0xF/+0x10 re-pick timer
   range, +0x11/+0x12 heading-jitter range, +0x13 leash radius, +0x14/+0x15
   weave amplitude & rate, +0x17 re-pick countdown, +0x18 heading, +0x19 weave
   direction, +0x1A chase radius, +0x1B chasing flag, +0x1C/+0x1D cooldowns
   (all <<10 for the radii).

   Each frame: age the re-pick countdown and, on expiry, jitter the heading by
   a random signed amount and re-arm the timer; add the weave offset (bouncing
   the weave direction at the amplitude limit); turn toward the resulting
   heading and take a guided-move step. On a blocked step, pick a fresh
   backwards-ish heading. Otherwise chase Spyro when he is inside 0x44C (or
   inside the +0x1A radius), else walk back home once past the leash, else just
   tick the two cooldowns down. */
extern int func_80037EA0(int lo, int hi);
extern int func_80038074(int angle, int step);
extern int func_80038EE0(int *actor, int target, int step, int lim, int flag);
extern int func_80039398(int *actor, int speed, int group, int radius,
                         int flags);
extern unsigned int GetRandomU32(void);
extern int ApproxDist2D(int *a, int *b);
extern int ArcTan2(int y, int x, int mode);

extern int D_800756C4;
extern int g_nSpyroWorldPosY; /* g_anSpyroWorldPos[1] */

void func_80039AA8(int *actor, unsigned char *p) {
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

  turn = p[0xD];
  speed = p[0xC];
  aligned = 1;
  if (D_800756C4 == 3) {
    speed += speed >> 1;
    turn += turn >> 1;
  } else if (D_800756C4 == 4) {
    speed <<= 1;
    turn <<= 1;
  }

  timer = p[0x17];
  p[0x17] = timer + 0xFF; /* wrapping decrement */
  if (timer == 0) {
    jitter = func_80037EA0(p[0x11], p[0x12]);
    if (GetRandomU32() & 1) {
      jitter = -jitter;
    }
    p[0x18] = (p[0x18] + jitter + 0x100) % 0x100;
    p[0x17] = func_80037EA0(p[0xF], p[0x10]);
    *(short *)(p + 0x1E) = 0;
    p[0x1B] = 0;
  }

  heading = p[0x18];
  if (p[0x14] != 0) {
    weave = *(short *)(p + 0x1E);
    heading += weave;
    weave += p[0x15] * p[0x19];
    *(short *)(p + 0x1E) = weave;
    amp = p[0x14];
    weave = (short)weave;
    if (weave < 0) {
      weave = -weave;
    }
    if (weave >= amp) {
      p[0x19] = -(int)p[0x19];
    }
  }
  if (func_80038EE0(actor, heading, turn, 5, 1) == 0) {
    aligned = 0;
  }

  if (p[0x1D] != 0) {
    hit = func_80039398(actor, speed, 0, p[0xE], 0x55);
  } else {
    hit = func_80039398(actor, speed, p[0xE], p[0xE], 0x55);
  }

  if (hit != 0) {
    if (aligned != 0) {
      p[0x18] = func_80038074(p[0x18], func_80037EA0(0x40, 0xC0));
      p[0x17] = func_80037EA0(p[0xF], p[0x10]);
      p[0x1B] = 0;
      if (hit & 2) {
        p[0x1C] = 0x14;
      } else {
        p[0x1D] = 6;
      }
    }
  } else {
    spyro = g_anSpyroWorldPos;
    dist = ApproxDist2D(actor + 3, spyro);
    if (dist < 0x44C && p[0x1B] == 0) {
      p[0x18] = ArcTan2(actor[3] - spyro[0], actor[4] - g_nSpyroWorldPosY, 0);
      p[0x18] = func_80038074(p[0x18], func_80037EA0(-0x3C, 0x3C));
      p[0x17] = func_80037EA0(p[0xF], p[0x10]);
      p[0x1B] = 1;
      return;
    }
    if (p[0x1A] != 0 && p[0x1B] == 0 && p[0x1C] == 0 &&
        dist < (int)(p[0x1A] << 10)) {
      p[0x18] = ArcTan2(actor[3] - g_anSpyroWorldPos[0],
                        actor[4] - g_nSpyroWorldPosY, 0);
      p[0x18] = func_80038074(p[0x18], func_80037EA0(-0x30, 0x30));
      p[0x17] = func_80037EA0(p[0xF], p[0x10]);
      p[0x1B] = 1;
      return;
    }
    home = ApproxDist2D(actor + 3, (int *)p);
    if ((int)(p[0x13] << 10) < home) {
      p[0x18] = ArcTan2(((int *)p)[0] - actor[3], ((int *)p)[1] - actor[4], 0);
      p[0x1B] = 0;
      p[0x1C] = 0x14;
      p[0x17] = home / (int)p[0xC];
    } else {
      if (p[0x1C] != 0) {
        p[0x1C] = p[0x1C] - 1;
      }
      if (p[0x1D] != 0) {
        p[0x1D] = p[0x1D] - 1;
      }
    }
  }
}
