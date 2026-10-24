/* pad.h — the per-substep controller ring (g_anPadSubstepRing, 4 x 0x18)
 * and the per-pad state/calibration block (PadState).
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

/* Byte offset of a PadSubstep field, and the field read as `type` through a
 * byte pointer to an entry: same address as the member, without the
 * in-struct MEM flag (or with a different width/signedness) where the matched
 * code needs that (ComputeSpyroMoveTargetFromPad, PollPadAndDistributeInput). */
#define PAD_SUBSTEP_OFFSET(field) ((int)&((PadSubstep *)0)->field)
#define PAD_SUBSTEP_RAW(p, type, field) \
  (*(type *)((char *)(p) + PAD_SUBSTEP_OFFSET(field)))

/* One controller's live state + stick calibration block: pad 1 at
 * g_dwPadPressed (0x80077378), pad 2 mirrored at g_dwPad2Buttons
 * (0x800776d8). The head words are the per-frame pad globals
 * (g_dwPadPressed .. g_dwPadAnalogRawSnapshot); the calibration tables are
 * filled by ResetPadStickCalibrationDefaults / FinalizePadCalibration and
 * read by MapPadAxisToCalibratedRange; the tail is the substep ring
 * (g_anPadSubstepRing for pad 1). Per-axis arrays are indexed by stick axis
 * 0..3 (the four analog bytes of the report). */
typedef struct PadState {
  unsigned int pressed;          /* 0x00 g_dwPadPressed */
  unsigned int released;         /* 0x04 g_dwPadReleased */
  unsigned int held;             /* 0x08 g_dwPadHeld */
  int padType;                   /* 0x0C g_nPadType */
  int stickActive;               /* 0x10 g_nPadStickActiveFlag */
  unsigned char analog[4];       /* 0x14 g_dwPadAnalogCalibrated, 0x7F = centered */
  int allInputIdle;              /* 0x18 g_nPadAllInputIdleFlag */
  int directionalIdle;           /* 0x1C g_nPadDirectionalIdleFlag */
  unsigned char stickNeutral[4]; /* 0x20 recorded center (g_dwPadAnalogRawSnapshot) */
  unsigned char stickLow[4];     /* 0x24 reported minimum */
  unsigned char stickHigh[4];    /* 0x28 reported maximum */
  unsigned char deadHigh[4];     /* 0x2C neutral + 0x30: above it the axis deflects up */
  unsigned char deadLow[4];      /* 0x30 neutral - 0x30: below it the axis deflects down */
  unsigned short scaleHigh[4];   /* 0x34 0x4000 / (high - deadHigh), Q7 gain */
  unsigned short scaleLow[4];    /* 0x3C 0x3F80 / (deadLow - low), Q7 gain */
  PadSubstep ring[4];            /* 0x44 per-substep snapshots */
} PadState;                      /* 0xA4 */

/* Byte offset of a PadState field. */
#define PAD_STATE_OFFSET(field) ((int)&((PadState *)0)->field)

/* A PadState field read through a byte pointer `p` that is offset from the
 * block base (per-axis or per-record cursors). Same address and width as the
 * struct member; the matched calibration code keeps byte-pointer bivs. */
#define PAD_STATE_RAW(p, type, field) (*(type *)((char *)(p) + PAD_STATE_OFFSET(field)))

#endif /* OPEN_SPYRO_PAD_H */
