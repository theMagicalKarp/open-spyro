/* pad.h — the per-substep controller ring (g_anPadSubstepRing, 4 x 0x18).
 *
 * PollPadAndDistributeInput fills one entry per frame tick (up to 4 ticks
 * per game frame) and the death replay refills all four from a recorded
 * frame; each entry mirrors the live pad globals of that tick. */

#ifndef OPEN_SPYRO_PAD_H
#define OPEN_SPYRO_PAD_H

typedef struct PadSubstep {
  int padType;           /* 0x00 g_nPadType */
  unsigned int held;     /* 0x04 g_dwPadHeld */
  unsigned int pressed;  /* 0x08 g_dwPadPressed */
  unsigned int released; /* 0x0C g_dwPadReleased */
  int stickActive;       /* 0x10 g_nPadStickActiveFlag */
  unsigned int analog;   /* 0x14 g_dwPadAnalogCalibrated */
} PadSubstep;            /* 0x18 */

/* Ring entry at byte offset `off` (tick * 0x18). */
#define PAD_SUBSTEP(off) ((PadSubstep *)((char *)g_anPadSubstepRing + (off)))

#endif /* OPEN_SPYRO_PAD_H */
