#include "angle.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* The D-pad direction yaw for byte offset `off` = (held >> 12) * 2 of
   g_anDpadDirectionYaw (the original scales the D-pad bits with one shift
   and mask, srl 11 / andi 0x1e). */
#define DPAD_DIRECTION_YAW(off)                                                \
  (*(short *)((char *)g_anDpadDirectionYaw + (off)))

/* 0x8003d3b8 (0x174) — convert this substep's pad input (g_pPadSubstepState)
   into a movement target. Left stick (analog axes PAD_AXIS_LX / LY, 0x7F
   centred): ArcTan2 target yaw made camera-relative, speed scaled by |dx|+|dy|.
   D-pad (PAD_DPAD): the yaw table plus g_nCameraEulerYaw at full speed. Neutral
   input holds the persistent yaw and zeroes speed.

   Match notes: the two analog yaw stores go through one held base (`target`,
   via the incomplete-array alias). The first store is volatile and so is the
   pad pointer reload, so the reload stays below it (true_dependence's
   both-volatile clause) but may rise above the plain second store. Each
   branch reloads the pad pointer into its own local (a function-wide `pad`
   would be one global-alloc pseudo in a1). Both stick bytes are read before
   the abs blocks, and |dx| is computed in place in `k` (the original reuses
   the 0x7f register, s0). */
void ComputeSpyroMoveTargetFromPad(int speed) {
  unsigned char *pad;
  unsigned char *stick;
  unsigned char *dpad;
  volatile int *target;
  int yaw;
  int k;
  int y;
  int x;
  int dy;

  pad = (unsigned char *)g_pPadSubstepState;
  if (((PadSubstep *)pad)->stickActive != 0 &&
      (((PadSubstep *)pad)->analog.word & PAD_LEFT_STICK_MASK) !=
          PAD_LEFT_STICK_CENTERED) {
    k = 0x7f;
    yaw = ArcTan2(k - ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LY],
                  k - ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LX],
                  ATAN_ANGLE12);
    target = (volatile int *)g_anSpyroMoveTargetYawBlock;
    *target = yaw;
    *(int *)target = (yaw + g_nCameraEulerYaw) & 0xfff;
    stick = *(unsigned char *volatile *)&g_pPadSubstepState;
    y = ((PadSubstep *)stick)->analog.axis[PAD_AXIS_LY];
    x = ((PadSubstep *)stick)->analog.axis[PAD_AXIS_LX];
    dy = k - y;
    if (dy < 0) {
      dy = -dy;
    }
    k = k - x;
    if (k < 0) {
      k = -k;
    }
    g_nSpyroSpeedTarget = speed * (dy + k) >> 7;
    if (speed < g_nSpyroSpeedTarget) {
      g_nSpyroSpeedTarget = speed;
    }
  } else {
    dpad = (unsigned char *)g_pPadSubstepState;
    if ((((PadSubstep *)dpad)->held & PAD_DPAD) != 0) {
      g_nSpyroSpeedTarget = speed;
      g_nSpyroMoveTargetYaw =
          DPAD_DIRECTION_YAW((PAD_SUBSTEP_RAW(dpad, int, held) >> 0xb) & 0x1e);
      g_nSpyroMoveTargetYaw =
          (g_nSpyroMoveTargetYaw + g_nCameraEulerYaw) & 0xfff;
    } else {
      g_nSpyroSpeedTarget = 0;
      g_nSpyroBodyYawStep = 0;
      g_nSpyroMoveTargetYaw = g_abSpyroPersistentEuler[2] << 4;
    }
  }
}

#undef DPAD_DIRECTION_YAW

/* Yaw written as a record member, so its in-struct load stays below the
   in-struct roll store (sched.c true_dependence, A230). */
#define YAW (((struct { int v; } *)&g_nSpyroBodyYaw)->v)

/* 0x8003d52c (0x1a4) — integrate Spyro's body euler angles by `rate` of
   angular velocity: roll advances by rate*sin(pitch), yaw by
   rate*cos(pitch)/cos(roll) and pitch by rate*sin(roll)*cos(pitch)/cos(roll)
   (cos(roll) clamped away from 0), each wrapped to 12 bits and mirrored into
   g_abSpyroPersistentEuler as a byte. The deltas are also kept in a local
   triple that nothing reads.

   The two empty do/while(0) blocks end cse's extended basic block (A200) so
   the roll accesses stay off the held pitch base instead of folding to
   absolute addresses; the first one sits between the pitch read for
   LookupSine and the call itself, which leaves the call's result copy free to
   follow the first roll load. */
void IntegrateSpyroBodyEuler(int rate) {
  int delta[3];
  int cosPitch;
  int sinPitch;
  int cosRoll;
  int rollTerm;
  int yawStep;
  int pitch;

  cosPitch = LookupCosine(g_anSpyroBodyPitchBlock[0]);
  pitch = g_anSpyroBodyPitchBlock[0];
  EBB_BREAK();
  sinPitch = LookupSine(pitch);
  cosRoll =
      LookupCosine(-g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] & 0xfff);
  if (cosRoll == 0) {
    cosRoll = 1;
  }
  rollTerm =
      rate *
      LookupSine(-g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] & 0xfff) /
      cosRoll;
  yawStep = rate * cosPitch / cosRoll;
  delta[1] = (rate * sinPitch) >> 12;
  g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] =
      (g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] + delta[1]) & 0xfff;
  g_abSpyroPersistentEuler[1] =
      g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] >> 4;
  delta[2] = yawStep;
  YAW = (YAW + delta[2]) & 0xfff;
  g_abSpyroPersistentEuler[2] = ANGLE12_TO_8(YAW);
  delta[0] = (rollTerm * cosPitch) >> 12;
  g_anSpyroBodyPitchBlock[0] = (g_anSpyroBodyPitchBlock[0] + delta[0]) & 0xfff;
  g_abSpyroPersistentEuler[0] = g_anSpyroBodyPitchBlock[0] >> 4;
}

#undef YAW

/* g_anSpyroTurnRateRows (0x8006c5f0) is per speed bucket; row stride 0x1a
   bytes, the lean-indexed rate short sits at +0xc (short[6]). */

/* 0x8003d6d0, 368 B: Steers Spyro's body yaw toward the move-target yaw using a
   lean accumulator g_nSpyroBodyYawStep (clamped -6..+6). Picks the turn rate
   from the 2D table (speed bucket selects the row, the lean the column). Resets
   the lean when the desired delta falls inside the table-indexed band, then
   integrates the chosen angular delta and reports the remaining |delta| via
   AbsAngleDelta12.

   The two yaw reads are shared locals with a second set in the recompute arm
   (A238): a single-set temp is boosted by sched1's birthing hoist and comes out
   of the block LAST, so both loads have to be multi-set for the target read to
   be emitted first. That puts both in global.c's allocno_compare, where the
   target read always loses (its live range is strictly the longer one), so the
   nested do/while(0) around the arm's target read weights that ref by 3 --
   6 refs / 6 insns == the body read's 4 / 4 -- and the tie falls to the lower
   allocno number, which is the target read. */
int TurnSpyroTowardMoveTargetWithLean(int bucket) {
  int delta;
  int rate;
  int targetYaw;
  int bodyYaw;
  short *base;
  short *row;

  targetYaw = g_nSpyroMoveTargetYaw;
  bodyYaw = g_nSpyroBodyYaw;
  delta = targetYaw - bodyYaw;
  delta = delta & ANGLE12_MASK;
  do {
    if (delta < 0x800) {
      g_nSpyroBodyYawStep = g_nSpyroBodyYawStep + 1;
      if (g_nSpyroBodyYawStep < 1) {
        g_nSpyroBodyYawStep = 1;
      }
      if (g_nSpyroBodyYawStep >= 7) {
        g_nSpyroBodyYawStep = 6;
      }
    } else {
      g_nSpyroBodyYawStep = g_nSpyroBodyYawStep - 1;
      if (g_nSpyroBodyYawStep < -6) {
        g_nSpyroBodyYawStep = -6;
      }
      if (g_nSpyroBodyYawStep >= 0) {
        g_nSpyroBodyYawStep = -1;
      }
    }
  } while (0);

  base = g_anSpyroTurnRateRows;
  row = (short *)((char *)base + bucket * 0x1a);
  rate = row[g_nSpyroBodyYawStep + 6];
  if (rate >= delta || rate + 0x1000 <= delta) {
    do {
      do {
        targetYaw = g_nSpyroMoveTargetYaw;
      } while (0);
    } while (0);
    bodyYaw = g_nSpyroBodyYaw;
    rate = (targetYaw - bodyYaw) & ANGLE12_MASK;
    if (rate > 0x800) {
      rate = rate - 0x1000;
    }
    g_nSpyroBodyYawStep = 0;
  }
  IntegrateSpyroBodyEuler(rate);
  return AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw);
}

/* 0x8003d840 (236 bytes) — steer Spyro's move-target yaw from the analog
   stick.  While the stick is engaged (stickActive set and the left-stick axes
   not both centred at 0x7f) and its magnitude is under 0x60, step the target
   yaw toward the body yaw by (magnitude * wrapped delta) >> 9; a delta past
   0x100 also raises the snap flag.  Always followed by a lean update, whose
   remaining |yaw delta| it returns. */
int ApplyPadInputToSpyroBodyYaw(void) {
  int vec[3];
  unsigned char *pad;
  int mag;
  int delta;
  int yaw;
  int tgt;

  pad = (unsigned char *)g_pPadSubstepState;
  if ((((PadSubstep *)pad)->stickActive != 0) &&
      ((((PadSubstep *)pad)->analog.word & PAD_LEFT_STICK_MASK) !=
       PAD_LEFT_STICK_CENTERED)) {
    vec[0] = ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LX] - PAD_STICK_CENTER;
    vec[1] = ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LY] - PAD_STICK_CENTER;
    vec[2] = 0;
    mag = VectorLength(vec, VEC_LEN_XY);
    if (mag < SPYRO_PIVOT_STICK) {
      tgt = g_anSpyroMoveTargetYawBlock[0];
      EBB_BREAK();
      yaw = g_anSpyroMoveTargetYawBlock[SPY_BODY_YAW - SPY_MOVE_TARGET_YAW];
      delta = (tgt - yaw) & ANGLE12_MASK;
      if (delta > ANGLE12_HALF) {
        delta -= ANGLE12_TURN;
      }
      if (abs(delta) > SPYRO_PIVOT_TURN) {
        g_nSpyroSubstate = SPYRO_SUB_WALK_PIVOT;
      }
      g_anSpyroMoveTargetYawBlock[0] =
          (yaw + ((mag * delta) >> 9)) & ANGLE12_MASK;
    }
  }
  return TurnSpyroTowardMoveTargetWithLean(0);
}

/* held-base view of g_nSpyroSpeedTarget (0x80078b20): [0] is the target,
   [0x14] (+0x50) is g_nSpyroMotionSpeed. */

/* 0x8003d92c (76 bytes) — move the motion speed one substep toward the target:
   accelerate by `accel` while under it, decelerate by `decel` while over it,
   and clamp to the target on the step that crosses it.

   The clamp store is written per arm so every access stays `off(a3)` off the
   held base (A201; jump.c cross-jumps the two tails back together), and the
   A195 depth-2 wrapper lifts `stepped` over `speed`/`target`.  The decelerate
   arm uses its own `down` local: global.c expand_preferences merges hard-reg
   preferences across every insn where a register dies, so a shared `stepped`
   would carry accel's $a0 preference over to `speed` through `speed - decel`
   and cost a `move v1,a0`. */
void AdvanceSpyroSpeedTowardTarget(int accel, int decel) {
  int stepped;
  int target;
  int speed;
  int down;

  target = g_anSpyroSpeedBlock[0];
  EBB_BREAK();
  speed = g_anSpyroSpeedBlock[SPY_SPEED - SPY_SPEED_TARGET];
  if (speed < target) {
    do {
      do {
        stepped = speed + accel;
      } while (0);
    } while (0);
    g_anSpyroSpeedBlock[SPY_SPEED - SPY_SPEED_TARGET] = stepped;
    if (target < stepped) {
      g_anSpyroSpeedBlock[SPY_SPEED - SPY_SPEED_TARGET] = target;
    }
  } else {
    down = speed - decel;
    g_anSpyroSpeedBlock[SPY_SPEED - SPY_SPEED_TARGET] = down;
    if (down < target) {
      g_anSpyroSpeedBlock[SPY_SPEED - SPY_SPEED_TARGET] = target;
    }
  }
}

/* 0x8003d978 (0x90) — build Spyro's world-space velocity: snapshot the body
   Euler angles (>> 4) into a byte triple, expand them into a rotation matrix
   on the stack, then rotate (speed, 0, 0) through it into g_anSpyroVelocity.
   The matrix is a 32-byte local declared ahead of the angle bytes (frame
   0x40: matrix at sp+0x10, angles at sp+0x30). Pitch and the velocity's x
   component / both RotateVectorByMatrix pointers go through the pitch-block
   alias, so &pitch is held in s0 across the first call and the velocity is
   reached at -0x30; the y/z zero stores stay direct. */
void BuildSpyroVelocityFromBodyEuler(void) {
  int mtx[8];
  signed char euler[3];

  euler[0] = (signed char)(g_anSpyroBodyPitchBlock[0] >> 4);
  euler[1] = (signed char)(g_nSpyroBodyRoll >> 4);
  euler[2] = (signed char)(ANGLE12_TO_8(g_nSpyroBodyYaw));
  ApplyEulerRotation(euler, mtx, 0);

  g_anSpyroVelocity[1] = 0;
  g_anSpyroVelocity[2] = 0;
  g_anSpyroBodyPitchBlock[SPY_VELOCITY - SPY_PITCH] = g_nSpyroMotionSpeed;
  RotateVectorByMatrix(mtx, &g_anSpyroBodyPitchBlock[SPY_VELOCITY - SPY_PITCH],
                       &g_anSpyroBodyPitchBlock[SPY_VELOCITY - SPY_PITCH]);
}

/* 0x8003da08 (0xDC) — spring Spyro's body pitch and roll back toward level
   while free-falling: wrap each angle's error into [-0x800,0x800), run it
   through a critically-damped spring (velocity in g_nSpyroPitchSpringVel/CB4),
   and apply a quarter of the spring velocity to the angle each frame. */
/* Statement order carries the spring section: the chain-1 error shift is
   split (`<< 2` right after chain 0's spring update, `>> 4` after the
   g_nSpyroRollSpringVel load) so sched1 does not hoist it to the head of the
   block, the g_nSpyroRollSpringVel load is written ahead of the
   g_nSpyroPitchSpringVel store, and each angle's final sum goes to its own
   single-set local (`out0`/`out1`) so the addu keeps the original's `addu
   a1,t0,a1` operand order instead of accumulating in place. */

void DampenSpyroFreeFallAttitude(void) {
  int err[2];
  int pitch;
  int roll;
  int wrapped;
  int pitchAccel;
  int rollAccel;
  int pitchVel;
  int rollStepVel;
  int pad[2];
  int rollVel;
  int out0;
  int out1;
  err[0] = 0;
  err[1] = 0;
  pitch = g_anSpyroBodyPitchBlock[0];
  wrapped = (-pitch) & ANGLE12_MASK;
  err[0] = wrapped;
  if (wrapped >= 0x800) {
    err[0] = wrapped - 0x1000;
  }
  roll = g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH];
  wrapped = (-roll) & ANGLE12_MASK;
  err[1] = wrapped;
  if (wrapped >= 0x800) {
    err[1] = wrapped - 0x1000;
  }
  pitchAccel = (err[0] << 2) >> 4;
  pitchVel = g_nSpyroPitchSpringVel;
  pitchAccel = pitchAccel - ((pitchVel << 4) >> 6);
  pitchVel = pitchVel + pitchAccel;
  rollAccel = err[1] << 2;
  rollVel = g_nSpyroRollSpringVel;
  rollAccel = rollAccel >> 4;
  g_nSpyroPitchSpringVel = pitchVel;
  pitchVel = pitchVel >> 2;
  err[0] = pitchVel;
  out0 = pitch + pitchVel;
  rollStepVel = rollVel;
  rollAccel = rollAccel - ((rollStepVel << 4) >> 6);
  rollStepVel = rollStepVel + rollAccel;
  g_nSpyroRollSpringVel = rollStepVel;
  rollStepVel = rollStepVel >> 2;
  err[1] = rollStepVel;
  out1 = roll + rollStepVel;
  g_anSpyroBodyPitchBlock[0] = out0;
  g_anSpyroBodyPitchBlock[SPY_ROLL - SPY_PITCH] = out1;
}

/* Held base at g_nSpyroGroundSlope: the slope test, the first persistent-
   euler byte (-0xA4) and the three call-argument addresses (euler -0xA4, body
   matrix -0x7C, world position -0xB0) all come off one callee-saved reg. */
#define SLOPE_BASE ((char *)g_anSpyroGroundSlopeBlock)

/* Second view of g_anSpyroGroundNormal for the reads after the second
   LookupCosine/LookupSine pair: with one symbol cse relates every access to
   one `la` pseudo and holds it across the calls (A231). */

/* 0x8003dae4 (0x360) — per-frame glide/flight attitude controller near the
   ground (g_nSpyroGroundSlope < SLOPE_STEEP; otherwise
   DampenSpyroFreeFallAttitude). Projects the ground normal into Spyro's yaw
   frame, derives the target pitch and roll from it, spring-damps the body
   pitch/roll toward them (velocities in g_nSpyroPitchSpringVel/CB4), and when
   both persistent-euler snapshots are tilted past 0xc0+0x20 layers a ground-hug
   offset (rotated by the body euler matrix) onto g_anSpyroWorldPos.

   Match notes: the angle pair lives in an addressable `ang[2]` (stack 0x20)
   and the frame has one unreferenced 8-byte local after it; the euler and
   matrix pointers are ONE variable reassigned (a separate matrix pointer is
   local-allocated to s0 inside the last block and pushes the cosine temp to
   s1); `0x2000 - cos(pitch) - cos(roll)` is written in the order that fold
   reverses into the original's subtraction order. */

void UpdateSpyroFlightAttitudeNearGround(void) {
  int vec[3];
  int ang[2];
  int pad[2];
  int c;
  int s;
  char *p;

  if (g_anSpyroGroundSlopeBlock[0] >= SLOPE_STEEP) {
    DampenSpyroFreeFallAttitude();
  } else {
    c = LookupCosine(g_nSpyroBodyYaw);
    s = LookupSine(g_nSpyroBodyYaw);
    vec[0] =
        (g_anSpyroGroundNormal[0] * c + g_anSpyroGroundNormal[1] * s) >> 12;
    c = LookupCosine(g_nSpyroBodyYaw);
    s = LookupSine(g_nSpyroBodyYaw);
    vec[1] =
        (g_anSpyroGroundNormalPost[1] * c - g_anSpyroGroundNormalPost[0] * s) >>
        12;
    vec[2] = g_anSpyroGroundNormalPost[2];
    ang[0] = -ArcTan2(IntegerSqrt(vec[0] * vec[0] + vec[2] * vec[2]), vec[1],
                      ATAN_ANGLE12);
    ang[1] = -ArcTan2(vec[2], vec[0], ATAN_ANGLE12);
    ang[0] = (ang[0] - g_nSpyroBodyPitch) & 0xfff;
    if (ang[0] > ANGLE12_HALF) {
      ang[0] -= ANGLE12_TURN;
    }
    ang[1] = (ang[1] - g_nSpyroBodyRoll) & 0xfff;
    if (ang[1] > ANGLE12_HALF) {
      ang[1] -= ANGLE12_TURN;
    }
    g_nSpyroPitchSpringVel +=
        ((ang[0] << 2) >> 4) - ((g_nSpyroPitchSpringVel << 4) >> 6);
    g_nSpyroRollSpringVel +=
        ((ang[1] << 2) >> 4) - ((g_nSpyroRollSpringVel << 4) >> 6);
    ang[0] = g_nSpyroPitchSpringVel >> 2;
    ang[1] = g_nSpyroRollSpringVel >> 2;
    g_nSpyroBodyPitch = (g_nSpyroBodyPitch + ang[0]) & ANGLE12_MASK;
    if (g_nSpyroBodyPitch > ANGLE12_HALF) {
      g_nSpyroBodyPitch -= ANGLE12_TURN;
    }
    g_nSpyroBodyRoll = (g_nSpyroBodyRoll + ang[1]) & ANGLE12_MASK;
    if (g_nSpyroBodyRoll > ANGLE12_HALF) {
      g_nSpyroBodyRoll -= ANGLE12_TURN;
    }
    p = SLOPE_BASE - 0xa4;
    if ((unsigned char)(SLOPE_BASE[-0xa4] - 0x20) > 0xc0 &&
        (unsigned char)(g_abSpyroPersistentEuler[1] - 0x20) > 0xc0) {
      vec[1] = (-LookupSine(ang[0]) * 0x174) >> 12;
      vec[0] = (-LookupSine(ang[1]) * 0x174) >> 12;
      c = LookupCosine(ang[1]);
      vec[2] = ((0x2000 - LookupCosine(ang[0]) - c) * 0x174) >> 12;
      {
        char *euler = p;
        p = SLOPE_BASE - 0x7c;
        ApplyEulerRotation((unsigned char *)euler, (int *)p, 0);
      }
      RotateVectorByMatrix((int *)p, vec, vec);
      AddVector((int *)(SLOPE_BASE - 0xb0), (int *)(SLOPE_BASE - 0xb0), vec);
    }
  }
}

#undef SLOPE_BASE

/* Align Spyro's body pitch/roll toward his velocity direction (0x8003de44,
   284 bytes). Computes the target pitch from ArcTan2(speed, vz) + 0x8E (roll
   target is 0), wraps each delta to a signed 12-bit angle, then advances the
   two spring velocities (g_nSpyroPitchSpringVel/CB4) toward the deltas and
   steps the body euler angles by spring/4. */

/* Scheduling levers: the pitch delta is written as four separate statements
   (stack read, global read, subtract, mask). The array read must come FIRST in
   source order — an embedded `(pitch = g_nSpyroBodyPitch)` operand has side
   effects, so expand evaluates it before `d[0]` and sched1 then hoists the
   global's lui/lw above the stack reload — and the subtract/mask must be their
   own statements or the result lands in a fresh register instead of reusing
   the loaded one. The roll delta below wants the opposite (embedded) form. */

void AlignSpyroBodyToVelocity(void) {
  int d[3];
  int len;
  int roll;
  int pitch;
  int v;
  int step2;
  int u;
  int step;
  d[0] = 0;
  len = VectorLength(g_anSpyroVelocity, VEC_LEN_XY);
  d[2] = len;
  d[1] = ArcTan2(len, g_anSpyroVelocity[2], ATAN_ANGLE12) + 0x8E;
  v = d[0];
  pitch = g_nSpyroBodyPitch;
  v -= pitch;
  v &= ANGLE12_MASK;
  d[0] = v;
  if (v >= 0x800) {
    d[0] = v - 0x1000;
  }
  v = (d[1] - (roll = g_nSpyroBodyRoll)) & ANGLE12_MASK;
  d[1] = v;
  if (v >= 0x800) {
    d[1] = v - 0x1000;
  }
  step = g_nSpyroPitchSpringVel +
         (((d[0] << 2) >> 4) - ((g_nSpyroPitchSpringVel << 4) >> 6));
  u = (d[1] << 2) >> 4;
  g_nSpyroPitchSpringVel = step;
  step2 = g_nSpyroRollSpringVel + (u - ((g_nSpyroRollSpringVel << 4) >> 6));
  step >>= 2;
  d[0] = step;
  g_nSpyroBodyPitch = pitch + step;
  g_nSpyroRollSpringVel = step2;
  step2 >>= 2;
  d[1] = step2;
  g_nSpyroBodyRoll = roll + step2;
}

/* Reset Spyro's per-frame motion: copy between the world position and the
   previous-position slot (g_anSpyroPrevWorldPos, +0x8c), clear the motion
   vector (g_anSpyroMotionVec, +0x10c) and the frame motion length. The two
   position vectors are reached through a single base pointer, so they share one
   address register; the motion length keeps its own global. (0x8003df60,
   68 bytes.)
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
void ResetSpyroFrameMotion(void) {
  int *p = g_anSpyroWorldPos;
  CopyVector(p, p + 0x23);
  ZeroVector(p + 0x43);
  g_nSpyroFrameMotionLength = 0;
}

/* Hard-zero of Spyro's linear motion. Clears the knockback/motion accumulator
   g_anSpyroMotionVec, the frame-delta length g_nSpyroFrameMotionLength, the
   live velocity g_anSpyroVelocity, and the speed scalar g_nSpyroMotionSpeed.
   Called by jump/glide/dive transitions just before they overwrite velocity
   with their own launch. (g_anSpyroVelocity sits 0x18 below g_anSpyroMotionVec,
   so gcc reuses the motion-vec base register with a -0x18 offset for the second
   ZeroVector call.) */

void ResetSpyroLinearMotion(void) {
  int *motion = g_anSpyroMotionVec;

  ZeroVector(motion);
  g_nSpyroFrameMotionLength = 0;
  ZeroVector(motion - 6); /* g_anSpyroVelocity sits 0x18 (6 ints) below */
  g_nSpyroMotionSpeed = 0;
}

/* Turn Spyro's byte yaw toward his motion vector (0x8003dfec; SPYRO_ST_SLIDE's
   only use): nothing inside
   +-`band`, a snap when within +-`step`, else one `step`; g_nSpyroBodyYaw is
   rebuilt from the byte. */
void TurnSpyroYawTowardMotion(int step, int band) {
  int ret;
  int yaw;
  int cur;
  int diff;
  ret = ArcTan2(g_anSpyroMotionVec[0], g_anSpyroMotionVec[1], ATAN_ANGLE8);
  cur = g_abSpyroPersistentEuler[2];
  diff = ret - cur;
  diff = diff & 0xFF;
  yaw = ret;
  ret = band < diff;
  if (ret && (diff < (0x100 - band))) {
    if ((diff <= step) || (diff >= (0x100 - step))) {
      g_abSpyroPersistentEuler[2] = yaw;
    } else {
      g_abSpyroPersistentEuler[2] =
          (((unsigned int)diff) < 0x80) ? (cur + step) : (cur - step);
    }
    g_nSpyroBodyYaw = g_abSpyroPersistentEuler[2] << 4;
  }
}

/* 0x80049880: critically-damped spring that carries Spyro's three swing euler
   angles toward their targets and publishes them as byte angles. Per axis: the
   12-bit signed error drives a velocity accumulator (error*128 minus the
   current accumulator*16, scaled by 1/64), the accumulator advances the
   angle by 1/64, and the angle is republished as a byte euler (>> 4). */
/* MATCHED 2026-09-10. The block structure, read off the branch targets: each
   stanza is `delta; guard; spring`, but the NEXT stanza's delta is computed
   BEFORE the current stanza's spring (the original software-pipelines it in
   source: the delta subu/andi sit in the join block of the previous guard), and
   the byte-0 and byte-1 publications sit between the THIRD guard and the third
   spring. Writing three uniform self-contained stanzas costs ~60 insns.

   Five load-bearing levers, all measured — do not "clean up" any of them:
     - A186 REFINEMENT: the first two guards show `sll f,d,7` in the delay slot
       AND again after the arm. Writing that join insn ONCE is only reachable
       when its dest is not the guarded variable's register: written once it is
       block-local, local-alloc ties its dest to the dying `d`, and reorg cannot
       copy it into the slot at all. `f = d << 7;` BEFORE the guard AND inside
       the arm makes `f` cross-block and both slots come out with the original's
       duplicate. The third guard must NOT use that form.
     - the accumulator is a PER-STANZA local (A138/A146); one shared variable is
       a cross-block allocno, conflicts with v0 and lands in a0.
     - `d` is ONE shared variable across all three stanzas.
     - the >>6 is its own statement (A167): `acc = acc + (f >> 6)` makes a fresh
       temp instead of shifting in place.
     - stanza 2 computes into its OWN block-scoped `f2`, not the shared `f` —
       the original's stanza-2 chain sits in v1 with the `acc2 << 4` temp in
       `f`'s a0, i.e. `f` is not live there at all.

   THE LAST FOUR INSNS WERE THREE A195/A200 `do { } while (0)` DIALS; the
   mechanism matters if you ever meet this shape again (every number below is
   from `cc1 -dl`/`-dg`, the §D 2c probe):
     1. EMPTY barrier after the byte-1 store. Without it sched1 hoists the
        accum-2 load and the `d << 7` above that store (21 insns of diff, and
        every positional residue in the function). With it the whole instruction
        sequence is position-for-position identical. 21 -> 13.
     2. The prev1 and prev2 held loads swap a3 <-> t0 because the two pseudos
        have the same 3 refs and live lengths that differ by ONE insn (29 vs
        28), so global.c's `floor_log2(R)*R/L` allocates prev2 first (3/28 =
        .1071 > 3/29 = .1034). Statement order CANNOT fix this — the lengths are
        post-sched1 (F14b), and a six-position sweep of the stanza-1 delta
        leaves them unchanged. The dial is A195: flow.c weights every ref by
        `loop_depth`, so a do/while around a statement adds one ref per
        reference in it, and 3 -> 4 refs crosses a `floor_log2` step (priority
        .1034 -> .276). Wrapping the prev1 UPDATE alone overshoots past prev0
        too (a2/a3 rotate, 18); wrapping the prev0 DELTA as well restores the
        chain prev0 > prev1 > prev2. 13 -> 8 -> 3.
     3. `f` loses its stanza-2 references to `f2`, which drops it below `d` and
        rotates a0/a1 through the whole function (22). An A195 wrap on stanza
        0's `f = f >> 6; acc0 = acc0 + f;` pair puts it back. -> 3.
     4. The last 3 insns were the A200 barrier's OWN cost: the original issues
        the stanza-2 guard's `slti` BEFORE the prev1 store, and the dial's
        closing NOTE_INSN_LOOP_END sits between them, where nothing can cross it
        (A200c). The fix is to put the guard `if` INSIDE the same wrapper as the
        prev1 update, so the compare is above the closing note. -> MATCH.
     GENERAL RULE worth keeping: when an A195 ref dial's barrier cost is "one
     insn of the FOLLOWING block wants to be above my store", widen the wrapper
     to swallow that insn rather than moving the dial. */
void SmoothSpyroByteEulerSpring(void) {
  int d;
  int f;
  int acc0;
  int acc1;
  int acc2;

  do {
    d = (g_nSpyroSwingEulerTargetX - g_nSpyroSwingEulerPrev0) & ANGLE12_MASK;
  } while (0);
  f = d << 7;
  if (d > 0x800) {
    d = d - 0x1000;
    f = d << 7;
  }

  d = (g_nSpyroSwingEulerTargetY - g_nSpyroSwingEulerPrev1) & ANGLE12_MASK;
  acc0 = g_nSpyroSwingEulerSpringAccum0;
  f = f - (acc0 << 4);
  do {
    f = f >> 6;
    acc0 = acc0 + f;
  } while (0);
  g_nSpyroSwingEulerSpringAccum0 = acc0;
  g_nSpyroSwingEulerPrev0 = g_nSpyroSwingEulerPrev0 + (acc0 >> 6);

  f = d << 7;
  if (d > 0x800) {
    d = d - 0x1000;
    f = d << 7;
  }

  d = (g_nSpyroSwingEulerTargetZ - g_nSpyroSwingEulerPrev2) & ANGLE12_MASK;
  acc1 = g_nSpyroSwingEulerSpringAccum1;
  f = f - (acc1 << 4);
  f = f >> 6;
  acc1 = acc1 + f;
  g_nSpyroSwingEulerSpringAccum1 = acc1;

  do {
    g_nSpyroSwingEulerPrev1 = g_nSpyroSwingEulerPrev1 + (acc1 >> 6);
    if (d > 0x800) {
      d = d - 0x1000;
    }
  } while (0);
  g_abSpyroSwingByteEuler[0] = g_nSpyroSwingEulerPrev0 >> 4;
  g_abSpyroSwingByteEuler[1] = g_nSpyroSwingEulerPrev1 >> 4;
  EBB_BREAK();
  acc2 = g_nSpyroSwingEulerSpringAccum2;
  {
    int f2;
    f2 = (d << 7) - (acc2 << 4);
    f2 = f2 >> 6;
    acc2 = acc2 + f2;
  }
  g_nSpyroSwingEulerSpringAccum2 = acc2;
  g_nSpyroSwingEulerPrev2 = g_nSpyroSwingEulerPrev2 + (acc2 >> 6);
  g_abSpyroSwingByteEuler[2] = g_nSpyroSwingEulerPrev2 >> 4;
}
