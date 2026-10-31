#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"
#include "pad_buttons.h"

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
  if (g_nCdStallMarker != 0) {
    g_nCdStallWatchdogTicks += 1;
  }
  if (g_nDemoMode != 0) {
    return;
  }

  padState = PadGetState(0);
  if (padState == 1) {
    g_nPadActAlignedFlag = 0;
  }
  if (g_nPadActAlignedFlag == 0) {
    PadSetAct(0, g_abPadActCommand, 2);
    if (padState == 2) {
      g_nPadActAlignedFlag = 1;
    } else if (padState == 6 && PadSetActAlign(0, g_abPadActAlignTable) != 0) {
      g_nPadActAlignedFlag = 1;
    }
  }

  if (PadInfoMode(0, 2, 0) != 0) {
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
    padType = 0;
  } else {
    switch (g_abPadRawReport[1]) {
    case 0x41:
      padType = 2;
      break;
    case 0x53:
    case 0x73:
      if (g_anPadTypeBlock[0] != 3) {
        ResetPadStickCalibrationDefaults((int)&g_anPadTypeBlock[-3]);
        g_dwPadAnalogRawSnapshot = *(unsigned int *)&g_abPadRawReport[4];
        FinalizePadCalibration((unsigned int *)&g_anPadTypeBlock[-3]);
      }
      padType = 3;
      break;
    default:
      padType = 1;
      break;
    }
  }

  stick = (int *)&g_adwPadHeldBlock[2];
  stick[0] = 0;
  if (padType >= 2) {
    unsigned int buttons;
    unsigned int pressed;
    unsigned int released;
    unsigned int held;
    unsigned int *pad;

    buttons = ~((g_abPadRawReport[2] << 8) | g_abPadRawReport[3]);
    if (padType == 3) {
      *(unsigned int *)cal = *(unsigned int *)&g_abPadRawReport[4];
      MapPadAxisToCalibratedRange(cal, (int)&stick[-4]);
      if (g_abPadRawReport[1] == 0x53) {
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
    if (*(unsigned char *)&g_nInGameTick != 0) {
      g_nPadType = padType;
      g_adwPadHeldBlock[0] = buttons;
      if (g_nFrameTicks != 0) {
        EBB_BREAK();
        g_adwPadHeldBlock[-2] |= pressed;
        g_adwPadHeldBlock[-1] |= released;
      } else {
        g_dwPadPressed = pressed;
        g_dwPadReleased = released;
        g_nPadAllInputIdleFlag = 1;
        g_nPadDirectionalIdleFlag = 1;
      }
      g_dwPadAnalogCalibrated = *(unsigned int *)cal;
      if (g_nPadAllInputIdleFlag != 0) {
        if ((g_dwPadHeld & PAD_DPAD) != 0 ||
            (padType == 3 && (cal[PAD_AXIS_LX] != 0x7F ||
                              cal[PAD_AXIS_LY] != cal[PAD_AXIS_LX]))) {
          g_nPadDirectionalIdleFlag = 0;
        }
        if (g_nPadDirectionalIdleFlag == 0 ||
            (g_dwPadHeld & (PAD_DPAD | 0x00FF)) != 0) {
          g_nPadAllInputIdleFlag = 0;
        }
      }
    } else if (g_nPadType < 2) {
      g_nPadType = padType;
    }

    if (g_nFrameTicks < 4) {
      int slot = g_nFrameTicks * 0x18;
      int *analog;
      int *ring = (int *)&g_adwPadHeldBlock[2];
      PAD_SUBSTEP(slot)->padType = padType;
      PAD_SUBSTEP(slot)->held = buttons;
      PAD_SUBSTEP(slot)->pressed = pressed;
      PAD_SUBSTEP(slot)->released = released;
      PAD_SUBSTEP(slot)->stickActive = ring[0];
      analog = &ring[18];
      *(unsigned int *)((char *)analog + slot) = g_dwPadAnalogCalibrated;
    }
  } else if (*(unsigned char *)&g_nInGameTick != 0) {
    g_nPadType = padType;
  }

  g_nFrameTicks += 1;
}
