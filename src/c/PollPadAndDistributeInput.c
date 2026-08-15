#include "globals.h"

/* Per-VBlank pad poll callback (0x80053c68, 0x67C), registered by
   InitPadSystem via InstallVSyncCallback. Ticks the vblank/CD watchdog
   counters, bails out while the death replay owns the pad, then drives the
   rumble command block, classifies the controller from the raw report id
   byte, remaps Saturn buttons, synthesizes virtual D-pad bits from the
   calibrated left stick, edge-detects pressed/released against g_dwPadHeld,
   maintains the sticky idle flags and fans the sample out into the current
   g_nFrameTicks slot of the 4-record substep ring. */
extern int PadGetState(int socket);
extern void PadSetAct(int socket, unsigned char *table, int len);
extern int PadSetActAlign(int socket, unsigned char *table);
extern int PadInfoMode(int socket, int term, int offs);
extern int PadSetMainMode(int socket, int offs, int lock);
extern void ResetPadStickCalibrationDefaults(int block);
extern int FinalizePadCalibration(unsigned int *block);
extern void MapPadAxisToCalibratedRange(unsigned char *cal, int block);

extern int g_anPadTypeBlock[];           /* g_nPadType as an array view */
extern unsigned int g_adwPadHeldBlock[]; /* g_dwPadHeld as an array view */

void PollPadAndDistributeInput(void) {
  unsigned char cal[4];
  int padState;
  int padType;
  int *stick;

  g_nVblankTickCount += 1;
  if (g_nCdStallMarker != 0) {
    g_nCdStallWatchdogTicks += 1;
  }
  if (g_nDeathState != 0) {
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
        /* the Saturn remap accumulator deliberately reuses `pressed`: the
           original serves both from one register (their ranges are disjoint,
           A196), and a separate local rotates the whole function. */
        pressed = buttons & ~0x9E;
        if (buttons & 0x80) {
          pressed |= 2;
        }
        if (buttons & 0x10) {
          pressed |= 8;
        }
        if (buttons & 8) {
          pressed |= 0x10;
        }
        if (buttons & 2) {
          pressed |= 4;
        }
        if (buttons & 4) {
          pressed |= 0x80;
        }
        buttons = pressed;
      }
      if ((buttons & 0xF000) == 0) {
        if (cal[2] >= 0xC1) {
          buttons |= 0x2000;
        } else if (cal[2] < 0x40) {
          buttons |= 0x8000;
        }
        if (cal[3] >= 0xC1) {
          buttons |= 0x4000;
        } else if (cal[3] < 0x40) {
          buttons |= 0x1000;
        }
        if (cal[2] != 0x7F || cal[3] != cal[2]) {
          g_nPadStickActiveFlag = 1;
        }
      }
    } else {
      *(unsigned int *)cal = 0x7F7F7F7F;
    }

    held = g_adwPadHeldBlock[0];
    pressed = ~held & buttons;
    released = held & ~buttons;
    if (*(unsigned char *)&g_nInGameTick != 0) {
      g_nPadType = padType;
      g_adwPadHeldBlock[0] = buttons;
      if (g_nFrameTicks != 0) {
        do {
        } while (0);
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
        if ((g_dwPadHeld & 0xF000) != 0 ||
            (padType == 3 && (cal[2] != 0x7F || cal[3] != cal[2]))) {
          g_nPadDirectionalIdleFlag = 0;
        }
        if (g_nPadDirectionalIdleFlag == 0 || (g_dwPadHeld & 0xF0FF) != 0) {
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
      *(int *)((char *)g_anPadSubstepRing + slot) = padType;
      *(unsigned int *)((char *)g_anPadSubstepRing + slot + 4) = buttons;
      *(unsigned int *)((char *)g_anPadSubstepRing + slot + 8) = pressed;
      *(unsigned int *)((char *)g_anPadSubstepRing + slot + 0xC) = released;
      *(int *)((char *)g_anPadSubstepRing + slot + 0x10) = ring[0];
      analog = &ring[18];
      *(unsigned int *)((char *)analog + slot) = g_dwPadAnalogCalibrated;
    }
  } else if (*(unsigned char *)&g_nInGameTick != 0) {
    g_nPadType = padType;
  }

  g_nFrameTicks += 1;
}
