#include "pad.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad_buttons.h"

/* Reset a PadState's per-frame input (seen as an int array): pressed /
   released / held cleared, the analog word centred, both idle flags raised;
   then each of the four ring entries gets the pad type, cleared buttons and a
   centred analog word (its stickActive is left alone). */
void ResetPadFrameSubstepTable(int *base) {
  int i;

  base[PAD_STATE_WORD(analog)] = PAD_ANALOG_CENTERED;
  base[PAD_STATE_WORD(held)] = 0;
  base[PAD_STATE_WORD(pressed)] = 0;
  base[PAD_STATE_WORD(released)] = 0;
  base[PAD_STATE_WORD(allInputIdle)] = 1;
  base[PAD_STATE_WORD(directionalIdle)] = 1;
  for (i = 0; i < 4; i++) {
    int t = base[PAD_STATE_WORD(padType)];
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(held)] = 0;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(pressed)] = 0;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(released)] = 0;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(analog)] = PAD_ANALOG_CENTERED;
    base[i * PAD_SUBSTEP_WORDS + PAD_RING_WORD(padType)] = t;
  }
}

/* Move the 0xA4-byte pad state block (g_dwPadPressed..) from `from` to `to`,
   then clear `from` (ResetPadFrameSubstepTable). Toggles g_bPadInputStashed.
   Spyro's frame ticks call it in pairs: live -> g_dwPadStash before the frame
   (input blanked while g_nSpyroInputBlankCountdown runs), then back after it.
   (0x80053708, 84 bytes.) */
void MovePadInputState(void *from, void *to) {
  g_bPadInputStashed = 1 - g_bPadInputStashed;
  CopyWords(to, from, 0xA4);
  ResetPadFrameSubstepTable(from);
}

/* Reset the four analog-stick calibration channels to neutral: center 0x7F,
   range 0..0xFF (PadState.stickNeutral / stickLow / stickHigh). */
void ResetPadStickCalibrationDefaults(unsigned char *cal) {
  int i;

  for (i = 0; i < 4; i++) {
    unsigned char *p = cal + i;
    p[PAD_STATE_OFFSET(stickLow)] = 0;
    p[PAD_STATE_OFFSET(stickNeutral)] = 0x7F;
    p[PAD_STATE_OFFSET(stickHigh)] = 0xFF;
  }
}

/* 0x80053790, 432 bytes — validate and bake the pad-stick calibration block.
   Each of the four axes needs its recorded stickNeutral at least 0x30 away
   from both reported extremes (stickLow / stickHigh), otherwise the whole
   block falls back to the uncalibrated defaults and the function returns 0.
   Otherwise each axis gets its two breakpoints (deadHigh = neutral + 0x30,
   deadLow = neutral - 0x30) and the two Q7 gains (scaleHigh / scaleLow), and
   the per-frame stick state is cleared: the head words, the centred analog
   bytes, and the four ring substep records. */

/* Three of the register assignments here are decided by allocation PRIORITY
   (global.c allocno_compare: floor_log2(refs)*refs/live_length, ties by lower
   allocno), not by the code shape, so two dials are load-bearing:

   - the nested `do { } while (0)` wrappers: flow.c weights each mention of a
     pseudo by its basic block's loop depth, so wrapping one statement in N
     dummy loops adds N refs per mention without emitting an insn. Loop 2 needs
     `axis` lifted over `q` (both 9 refs; lengths 25 vs 22 -> 1.08 vs 1.227),
     which takes +2 refs on `axis`; the phase-3 fill loop needs `fill` (7 refs /
     11 insns) lifted over the shared `j` (18/21), which takes +6.
   - `j` is ONE variable across both fill loops on purpose: that makes it live
     across the phase-4 `rec` giv init, whose a0 preference then keeps `j` out
     of a0 (find_reg pass 0 skips regs_someone_prefers), leaving a0 for
     `neutral` and v1 for `k`.

   `rec` is a plain pointer biv, initialized BEFORE `k`, because both preheader
   inits have to come out in that order and loop.c always emits giv inits last
   (its per-biv giv list is built by prepending, so emission is reverse
   discovery order and the loop-test giv, recorded at the bottom compare, would
   go first). The three record-head stores are volatile only to stop loop.c
   reducing rec+0x48/0x4C/0x50 into a second induction register (+2 insns). */
int FinalizePadCalibration(unsigned char *cal) {
  unsigned char *p;
  unsigned char *q;
  unsigned char *fill;
  unsigned char *rec;
  unsigned char *fill2;
  int i;
  int axis;
  int ok;
  int j;
  int k;
  int mid;
  int hi;
  int num1;
  int num2;
  int neutral;
  int neutral2;

  ok = 1;
  for (i = 0; i < 4;) {
    p = cal + i;
    mid = p[PAD_STATE_OFFSET(stickNeutral)];
    if (mid + 0x30 >= p[PAD_STATE_OFFSET(stickHigh)] ||
        (i += 1, p[PAD_STATE_OFFSET(stickLow)] >= mid - 0x30)) {
      ResetPadStickCalibrationDefaults(cal);
      ok = 0;
      break;
    }
  }

  axis = 0;
  num1 = 0x4000;
  num2 = 0x3F80;
  do {
    do {
      do {
        p = cal + axis;
      } while (0);
    } while (0);
    q = cal + axis * 2;
    p[PAD_STATE_OFFSET(deadHigh)] = p[PAD_STATE_OFFSET(stickNeutral)] + 0x30;
    hi = p[PAD_STATE_OFFSET(stickHigh)];
    hi =
        num1 / (hi - *(volatile unsigned char *)&p[PAD_STATE_OFFSET(deadHigh)]);
    p[PAD_STATE_OFFSET(deadLow)] = p[PAD_STATE_OFFSET(stickNeutral)] - 0x30;
    PAD_STATE_RAW(q, short, scaleHigh) = hi;
    PAD_STATE_RAW(q, short, scaleLow) =
        num2 / (*(volatile unsigned char *)&p[PAD_STATE_OFFSET(deadLow)] -
                p[PAD_STATE_OFFSET(stickLow)]);
    axis += 1;
  } while (axis < 4);

  neutral = 0x7F;
  j = 3;
  fill = cal + 3;
  ((PadState *)cal)->pressed = 0;
  ((PadState *)cal)->released = 0;
  ((PadState *)cal)->held = 0;
  ((PadState *)cal)->allInputIdle = 1;
  ((PadState *)cal)->directionalIdle = 1;
  do {
    do {
      do {
        fill[PAD_STATE_OFFSET(analog)] = neutral;
        fill -= 1;
      } while (0);
    } while (0);
  } while (--j >= 0);

  neutral2 = 0x7F;
  rec = cal;
  k = 0;
  do {
    j = 3;
    fill2 = rec + 3;
    PAD_STATE_RAW(rec, volatile int, ring[0].pressed) = 0;
    PAD_STATE_RAW(rec, volatile int, ring[0].released) = 0;
    PAD_STATE_RAW(rec, volatile int, ring[0].held) = 0;
    do {
      fill2[PAD_STATE_OFFSET(ring[0].analog)] = neutral2;
      fill2 -= 1;
    } while (--j >= 0);
    k += sizeof(PadSubstep);
    rec += sizeof(PadSubstep);
  } while (k < 0x60);

  return ok;
}

/* 0x80053940: maps 4 raw 8-bit analog stick axes into the calibrated
   0..0xff range: outside the dead zone [deadLow, deadHigh] the deflection is
   scaled by the axis' Q7 gain away from center 0x7f (see
   FinalizePadCalibration, which fills the PadState tables). */
void MapPadAxisToCalibratedRange(unsigned char *axes, int cal) {
  int i;
  PadState *calib;
  unsigned int raw, hi, lo;
  int v;

  calib = (PadState *)cal;
  for (i = 0; i < 4; i++) {
    raw = axes[0];
    hi = calib->deadHigh[i];
    if (raw > hi) {
      int shifted = (((int)(raw - hi)) * calib->scaleHigh[i]) >> 7;
      v = shifted + 0x7f;
      if (v >= 0x100) {
        axes[0] = 0xff;
      } else {
        axes[0] = (unsigned char)v;
      }
    } else {
      lo = calib->deadLow[i];
      if (raw < lo) {
        v = 0x7f - (((int)(lo - raw) * calib->scaleLow[i]) >> 7);
        if (v < 0) {
          axes[0] = 0;
        } else {
          axes[0] = (unsigned char)v;
        }
      } else {
        axes[0] = 0x7f;
      }
    }
    axes += 1;
  }
}

/* Pad sampler for the attract demo (0x800539fc, 0x26C). While a demo is
   being recorded (g_nDemoMode == DEMO_RECORD) it samples the real
   pad — inverted button halfword plus the calibrated right-stick bytes —
   and appends the packed frame at g_pDemoPadCursor; otherwise it
   plays the next recorded frame back into the pad state (stick centered
   at 0x7F). Either way it then rebuilds pressed/held/released, the
   stick-active flag, fans the held state out to the 4 pad substep-ring
   slots, refreshes the input-idle flags and forces a 2-tick frame.
   Returns 2 (the tick count). */

int SampleDemoPadFrame(void) {
  int frame;
  unsigned int held;
  unsigned int rel;
  int i;
  int off;
  unsigned int *p;
  unsigned int *heldp;

  if (g_nDemoMode == DEMO_RECORD) {
    unsigned char *cal = g_abPadAnalogCalibratedBlock;
    unsigned char *cur;
    frame = ~((g_abPadRawReport[2] << 8) | g_abPadRawReport[3]);
    *(unsigned int *)cal = *(unsigned int *)&g_abPadRawReport[4];
    MapPadAxisToCalibratedRange(cal, (int)(cal - 0x14));
    frame &= 0xFFFF;
    frame |= cal[PAD_AXIS_LX] << 16;
    frame |= cal[PAD_AXIS_LY] << 24;
    cur = g_pDemoPadCursor;
    g_pDemoPadCursor = cur + 4;
    *(unsigned int *)cur = frame;
  } else {
    unsigned char *cal = (unsigned char *)&g_dwPadAnalogCalibrated;
    unsigned char *cur;
    cur = g_pDemoPadCursor;
    g_pDemoPadCursor = cur + 4;
    frame = *(int *)cur;
    cal[0] = 0x7F;
    cal[1] = 0x7F;
    cal[PAD_AXIS_LX] = frame >> 16;
    cal[PAD_AXIS_LY] = frame >> 24;
  }
  frame &= 0xFFFF;
  held = g_dwPadHeld;
  g_nPadType = PAD_TYPE_ANALOG;
  g_dwPadPressed = ~held & frame;
  rel = held & ~frame;
  g_dwPadHeld = frame;
  g_dwPadReleased = rel;
  if ((g_dwPadAnalogCalibrated & PAD_LEFT_STICK_MASK) !=
      PAD_LEFT_STICK_CENTERED) {
    g_nPadStickActiveFlag = 1;
  } else {
    g_nPadStickActiveFlag = 0;
  }
  i = 0;
  heldp = g_adwPadHeldBlock;
  p = &heldp[0x14];
  off = 0;
  do {
    PAD_SUBSTEP(off)->held = heldp[0];
    PAD_SUBSTEP(off)->pressed = heldp[-2];
    PAD_SUBSTEP(off)->released = heldp[-1];
    PAD_SUBSTEP(off)->padType = heldp[1];
    i += 1;
    *p = heldp[3];
    p += 6;
    PAD_SUBSTEP(off)->stickActive = heldp[2];
    off += 0x18;
  } while (i < 4);
  g_nPadAllInputIdleFlag = 1;
  g_nPadDirectionalIdleFlag = 1;
  if ((g_dwPadHeld & PAD_DPAD) != 0 ||
      (g_dwPadAnalogCalibrated & PAD_LEFT_STICK_MASK) !=
          PAD_LEFT_STICK_CENTERED) {
    g_nPadDirectionalIdleFlag = 0;
  }
  if (g_nPadDirectionalIdleFlag == 0 ||
      (g_dwPadHeld & (PAD_DPAD | 0x00FF)) != 0) {
    g_nPadAllInputIdleFlag = 0;
  }
  g_nFrameTicks = 2;
  return 2;
}

/* Views off the held pad-type and held-buttons bases. */
#define PAD_CALIBRATION                                                        \
  ((unsigned char *)&g_anPadTypeBlock[-3]) /* the g_dwPadPressed PadState */
#define PAD_PRESSED_ACC (g_adwPadHeldBlock[-2])  /* g_dwPadPressed */
#define PAD_RELEASED_ACC (g_adwPadHeldBlock[-1]) /* g_dwPadReleased */
#define PAD_STICK_ACTIVE                                                       \
  ((int *)&g_adwPadHeldBlock[2]) /* g_nPadStickActiveFlag */

/* Per-VBlank pad poll callback (0x80053c68, 0x67C), registered by
   InitPadSystem via InstallVSyncCallback. Ticks the vblank/CD watchdog
   counters, bails out while the attract demo owns the pad, then drives the
   rumble command block, classifies the controller from the raw report id
   byte, remaps Saturn buttons, synthesizes virtual D-pad bits from the
   calibrated left stick, edge-detects pressed/released against g_dwPadHeld,
   maintains the sticky idle flags and fans the sample out into the current
   g_nFrameTicks slot of the 4-record substep ring. */

void PollPadAndDistributeInput(void) {
  unsigned char cal[4];
  int padState;
  int padType;
  int *stick;

  g_nVblankTickCount += 1;
  if (g_nCdStallTimeoutTicks != 0) {
    g_nCdStallWatchdogTicks += 1;
  }
  if (g_nDemoMode != DEMO_OFF) {
    return;
  }

  padState = PadGetState(0);
  if (padState == PadStateFindPad) {
    g_nPadActAlignedFlag = 0;
  }
  if (g_nPadActAlignedFlag == 0) {
    PadSetAct(0, g_abPadActCommand, 2);
    if (padState == PadStateFindCTP1) {
      g_nPadActAlignedFlag = 1;
    } else if (padState == PadStateStable &&
               PadSetActAlign(0, g_abPadActAlignTable) != 0) {
      g_nPadActAlignedFlag = 1;
    }
  }

  if (PadInfoMode(0, InfoModeCurExID, 0) != 0) {
    if (g_nPadSetMainModePending != 0) {
      if (PadSetMainMode(0, 1, 0) != 0) {
        g_nPadSetMainModePending = 0;
      }
    }
    g_nPadIsDualshockFlag = 1;
  } else {
    g_nPadIsDualshockFlag = 0;
  }

  if (g_nOptionVibrationEnabled == 0 || g_nPadIsDualshockFlag == 0) {
    g_nHitRumbleTimer = 0;
    g_nVibrationLevel = 0;
    g_nPulseRumbleTimer = 0;
    g_nPulseRumbleAmount = 0;
  }
  if (g_nHitRumbleTimer != 0) {
    g_abPadActCommand[0] = 1;
    g_abPadActCommand[1] = 0x78;
  } else if (g_nVibrationLevel != 0) {
    g_abPadActCommand[0] = 1;
    g_abPadActCommand[1] = 0;
  } else if (g_nPulseRumbleTimer != 0) {
    g_abPadActCommand[0] = 0;
    g_abPadActCommand[1] = g_nPulseRumbleAmount;
  } else {
    g_abPadActCommand[0] = 0;
    g_abPadActCommand[1] = 0;
  }

  g_nHitRumbleTimer -= 1;
  if (g_nHitRumbleTimer < 0) {
    g_nHitRumbleTimer = 0;
  }
  g_nVibrationLevel -= 1;
  if (g_nVibrationLevel < 0) {
    g_nVibrationLevel = 0;
  }
  g_nPulseRumbleTimer -= 1;
  if (g_nPulseRumbleTimer < 0) {
    g_nPulseRumbleTimer = 0;
  }

  if (g_abPadRawReport[0] != 0) {
    g_nPadSetMainModePending = 1;
    padType = PAD_TYPE_NONE;
  } else {
    switch (g_abPadRawReport[1]) {
    case PAD_ID_DIGITAL:
      padType = PAD_TYPE_DIGITAL;
      break;
    case PAD_ID_ANALOG_STICK:
    case PAD_ID_DUALSHOCK:
      if (g_anPadTypeBlock[0] != PAD_TYPE_ANALOG) {
        ResetPadStickCalibrationDefaults(PAD_CALIBRATION);
        g_dwPadAnalogRawSnapshot = *(unsigned int *)&g_abPadRawReport[4];
        FinalizePadCalibration(PAD_CALIBRATION);
      }
      padType = PAD_TYPE_ANALOG;
      break;
    default:
      padType = PAD_TYPE_UNSUPPORTED;
      break;
    }
  }

  stick = PAD_STICK_ACTIVE;
  stick[0] = 0;
  if (padType >= PAD_TYPE_DIGITAL) {
    unsigned int buttons;
    unsigned int pressed;
    unsigned int released;
    unsigned int held;
    unsigned int *pad;

    buttons = ~((g_abPadRawReport[2] << 8) | g_abPadRawReport[3]);
    if (padType == PAD_TYPE_ANALOG) {
      *(unsigned int *)cal = *(unsigned int *)&g_abPadRawReport[4];
      MapPadAxisToCalibratedRange(cal, (int)&stick[-4]);
      if (g_abPadRawReport[1] == PAD_ID_ANALOG_STICK) {
        /* the Saturn-pad face/shoulder remap accumulator deliberately reuses
           `pressed`: the original serves both from one register (their ranges
           are disjoint, A196), and a separate local rotates the whole function.
         */
        pressed =
            buttons & ~(PAD_SQUARE | PAD_TRIANGLE | PAD_R1 | PAD_L1 | PAD_R2);
        if (buttons & PAD_SQUARE) {
          pressed |= PAD_R2;
        }
        if (buttons & PAD_TRIANGLE) {
          pressed |= PAD_R1;
        }
        if (buttons & PAD_R1) {
          pressed |= PAD_TRIANGLE;
        }
        if (buttons & PAD_R2) {
          pressed |= PAD_L1;
        }
        if (buttons & PAD_L1) {
          pressed |= PAD_SQUARE;
        }
        buttons = pressed;
      }
      if ((buttons & PAD_DPAD) == 0) {
        if (cal[PAD_AXIS_LX] >= 0xC1) {
          buttons |= PAD_RIGHT;
        } else if (cal[PAD_AXIS_LX] < 0x40) {
          buttons |= PAD_LEFT;
        }
        if (cal[PAD_AXIS_LY] >= 0xC1) {
          buttons |= PAD_DOWN;
        } else if (cal[PAD_AXIS_LY] < 0x40) {
          buttons |= PAD_UP;
        }
        if (cal[PAD_AXIS_LX] != 0x7F || cal[PAD_AXIS_LY] != cal[PAD_AXIS_LX]) {
          g_nPadStickActiveFlag = 1;
        }
      }
    } else {
      *(unsigned int *)cal = PAD_ANALOG_CENTERED;
    }

    held = g_adwPadHeldBlock[0];
    pressed = ~held & buttons;
    released = held & ~buttons;
    if (*(unsigned char *)&g_nPadLatchOpen != 0) {
      g_nPadType = padType;
      g_adwPadHeldBlock[0] = buttons;
      if (g_nFrameTicks != 0) {
        EBB_BREAK();
        PAD_PRESSED_ACC |= pressed;
        PAD_RELEASED_ACC |= released;
      } else {
        g_dwPadPressed = pressed;
        g_dwPadReleased = released;
        g_nPadAllInputIdleFlag = 1;
        g_nPadDirectionalIdleFlag = 1;
      }
      g_dwPadAnalogCalibrated = *(unsigned int *)cal;
      if (g_nPadAllInputIdleFlag != 0) {
        if ((g_dwPadHeld & PAD_DPAD) != 0 ||
            (padType == PAD_TYPE_ANALOG &&
             (cal[PAD_AXIS_LX] != 0x7F ||
              cal[PAD_AXIS_LY] != cal[PAD_AXIS_LX]))) {
          g_nPadDirectionalIdleFlag = 0;
        }
        if (g_nPadDirectionalIdleFlag == 0 ||
            (g_dwPadHeld & (PAD_DPAD | 0x00FF)) != 0) {
          g_nPadAllInputIdleFlag = 0;
        }
      }
    } else if (g_nPadType < PAD_TYPE_DIGITAL) {
      g_nPadType = padType;
    }

    if (g_nFrameTicks < 4) {
      int slot = g_nFrameTicks * 0x18;
      int *analog;
      int *ring = PAD_STICK_ACTIVE;
      PAD_SUBSTEP(slot)->padType = padType;
      PAD_SUBSTEP(slot)->held = buttons;
      PAD_SUBSTEP(slot)->pressed = pressed;
      PAD_SUBSTEP(slot)->released = released;
      PAD_SUBSTEP(slot)->stickActive = ring[0];
      analog = &ring[18];
      *(unsigned int *)((char *)analog + slot) = g_dwPadAnalogCalibrated;
    }
  } else if (*(unsigned char *)&g_nPadLatchOpen != 0) {
    g_nPadType = padType;
  }

  g_nFrameTicks += 1;
}

#undef PAD_CALIBRATION
#undef PAD_PRESSED_ACC
#undef PAD_RELEASED_ACC
#undef PAD_STICK_ACTIVE
