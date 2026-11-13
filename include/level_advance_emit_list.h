/* level_advance_emit_list.h — declarations shared by the *_AdvanceEmitList.c clones, declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H
#define OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H

#include "sparx.h"

extern void AddSVector(short *dst, short *a, short *b);

extern void SVectorToVectorShl2(int *dst, short *src);
extern void SVectorToVector(int *dst, short *src);

extern void VectorToSVectorShr2(short *dst, int *src);

extern int abs(int n);

extern void FreeEmitListRecord(unsigned char *rec);

extern unsigned char *g_pEmitListBase;

extern struct Actor *g_pActorListBase; /* level actor pool */

extern short g_anSineLut[];        /* sine table */

extern short g_anCosineLut[];        /* cosine table */

extern int g_anCameraPos[];

extern int g_nSpyroInvulnFrames[]; /* [-0x58] is the Spyro world position (g_anSpyroWorldPos) */

/* g_anSpyroWorldPos reached through the invulnerability counter's held base. */
#define SPYRO_POS_VIA_INVULN (g_nSpyroInvulnFrames - 0x58)

extern int g_nSparxHealth;

/* Field access on the current emit record `rec` (0x20 bytes, layout varies
 * by class; see each file's field map). These stay byte-offset accessors on
 * purpose: every field must be an address giv off `rec` so loop.c folds them
 * onto one base register, which a struct view does not reproduce. */
#define CLASS rec[0x1]                         /* class; 0xFF ends the list */
#define AGE rec[0x2]                           /* frames lived */
#define VISIBLE rec[0x3] /* RasterizeEmitList: 1 drawn, 0 culled last frame */
/* The usual free test: aged past `n` frames, or culled off screen. */
#define AGED_OR_CULLED(n) (AGE >= (n) || VISIBLE == 0)
#define HS(n) (*(short *)(rec + (n)))          /* s16 field at +n */
#define HU(n) (*(unsigned short *)(rec + (n))) /* u16 field at +n */
#define W(n) (*(int *)(rec + (n)))             /* s32 field at +n */
#define POS ((short *)(rec + 4))               /* tail point, svector */
#define POS_U ((unsigned short *)(rec + 4))    /* POS through its unsigned view */
/* A streak's head point (the LINE_G2's second vertex; the quad classes keep
 * SIZE / SPIN / colour there instead). */
#define HEAD ((short *)(rec + 0xA))
#define HEAD_U ((unsigned short *)(rec + 0xA))
#define VEL ((short *)(rec + 0x18))            /* velocity / spin, svector */
#define VEL_U ((unsigned short *)(rec + 0x18))
/* The drifting quads' spin step (added to SPIN each tick) and the 0/1
 * toggle that paces the sparks' z climb (SPARK_Z_CLIMB). */
#define SPIN_RATE rec[0x1E]
#define CLIMB_TOGGLE HS(0x1E)
#define CLIMB_TOGGLE_U HU(0x1E)
/* A billboard's half-size and 8-bit spin angle (RasterizeEmitList scales and
 * turns the quad's corners by them); every arm that touches them is a quad
 * class. */
#define SIZE rec[0xA]
#define SPIN rec[0xB]
/* The colour bytes of the packed RGB / mode word at 0x0C that
 * RasterizeEmitList lights the prim with (every class but the streaks, whose
 * colours sit at 0x10 / 0x14); fading a record is stepping these down. */
#define COL_R rec[0xC]
#define COL_G rec[0xD]
#define COL_B rec[0xE]
/* All three at once (the fx byte masked off): 0 once faded to black. */
#define COL_RGB (W(0xC) & 0xFFFFFF)
/* The colour word's top byte: the GPU command (EMIT_FX_*) the prim gets. */
#define FX rec[0xF]
/* The line classes' two colours (LINE_G2): COL0 goes with the point at 0x04,
 * COL1 with the one at 0x0A; COL1's top byte (0x17) is the OT-bin bias. */
#define COL0_R rec[0x10]
#define COL0_G rec[0x11]
#define COL0_B rec[0x12]
#define COL1_R rec[0x14]
#define COL1_G rec[0x15]
#define COL1_B rec[0x16]
/* A quad's texture sub-record index and OT depth-bin bias. */
#define TEX_INDEX rec[0x10]
#define DEPTH_BIAS rec[0x11]
/* Swirl records (ribbon / smear): the point circles its centre at
 * SWIRL_ANGLE (sine-LUT index), SWIRL_RADIUS growing until it is freed. */
#define SPIRAL_CENTRE_U ((unsigned short *)(rec + 0x12)) /* EMIT_RISING_SPIRAL x, y */
#define SMEAR_CENTRE_U ((unsigned short *)(rec + 0x10))  /* EMIT_RISING_SPIRAL_SMEAR x, y */
/* Per-class fields past the colour word: the jittering motes' (twinkle /
 * ember / yellow dust) drift step, a burst spark's velocity, the Spyro
 * hurt sparkle's 12-bit orbit angle and a rising puff's climb rate. */
#define DRIFT ((short *)(rec + 0x10))
#define DRIFT_U ((unsigned short *)(rec + 0x10))
#define BURST_VEL ((short *)(rec + 0x12))
#define ORBIT_ANGLE HS(0x18)
#define ORBIT_ANGLE_U HU(0x18)
#define RISE_SPEED_U HU(0x1E)
/* EMIT_RESCUE_SWIRL: the ring's centre, the band's lift above it and its
 * radius (both re-shaped from RSWIRL_BAND and g_nDragonRescueTimer). */
#define RSWIRL_CENTRE_U ((unsigned short *)(rec + 0x12))
#define RSWIRL_LIFT HS(0x18)
#define RSWIRL_LIFT_U HU(0x18)
#define RSWIRL_RADIUS HS(0x1A)
/* EMIT_ACTOR_PLUME: the world point it rises from, the widest its spiral
 * grows and the height (x 0x40 above the anchor) that frees it. */
#define PLUME_ANCHOR ((int *)W(0x18))
#define PLUME_MAX_RADIUS HS(0x1C)
#define PLUME_TOP HS(0x1E)
/* EMIT_SMOKE_PUFF: its climb per tick, slowing by 2 to 0. */
#define SMOKE_RISE W(0x18)
/* EMIT_ORBIT_DOT: the point it circles (rising AGE * 2 above it), the
 * orbit's phase offset (added to AGE, << 6) and radius. */
#define ODOT_CENTRE_U ((unsigned short *)(rec + 0x18))
#define ODOT_PHASE rec[0x1E]
#define ODOT_RADIUS rec[0x1F]
/* The actor-tracking sparks / orbs: pool index of the actor they chase. */
#define TRACK_ACTOR rec[0x1E]
/* EMIT_PATH_WISP: the fence captain that sent it (pool index), the guard
 * slot of the captain's it flies to (FenceCaptainState.cur when sent) and
 * the waypoint of that guard's wisp path it is flying to. */
#define WISP_CAPTAIN rec[0x18]
#define WISP_GUARD rec[0x19]
#define WISP_WAYPOINT rec[0x1A]
/* EMIT_HOMING_ORB / EMIT_PATH_WISP: the mode halfword (ORB_TRAIL: falling
 * spark trail; ORB_ORBIT: circling its drift point VEL, the X / Y sweeps
 * signed by ORB_X_UP / ORB_Y_UP; neither: homing on ORB_TARGET, the actor
 * the word at 0x18 names (level 15's sentinel aims it at its linked guard,
 * and on arrival the orb frees it if bound: TetherState TCMD_FREED). */
#define ORB_MODE HS(0x1E)
#define ORB_MODE_U HU(0x1E)
#define ORB_TARGET ((struct Actor *)W(0x18))
#define ORB_ORBIT 1
#define ORB_X_UP 2
#define ORB_Y_UP 4
#define ORB_TRAIL 8
/* EMIT_PATH_DOT: its waypoint list (count byte, 0x10-byte points from +8)
 * and the waypoint it is heading for. */
#define PDOT_PATH W(0x18)
#define PDOT_WAYPOINT W(0x1C)
#define SWIRL_ANGLE rec[0x18]
#define SWIRL_RADIUS rec[0x19]
/* EMIT_RESCUE_SWIRL (the dragon-rescue ring, a stack of bands): its spin
 * angle and the record's band index up the stack. */
#define RSWIRL_ANGLE rec[0x1C]
#define RSWIRL_BAND rec[0x1D]
/* Fade all three colour bytes by `n`. */
#define FADE_COL(n)                                                            \
  COL_R -= (n);                                                                \
  COL_G -= (n);                                                                \
  COL_B -= (n)

/* Spark drift: every other tick the z velocity (VEL[2], updated through its
 * unsigned view) climbs by one; CLIMB_TOGGLE is the 0/1 toggle. */
#define SPARK_Z_CLIMB()                                                        \
  if (CLIMB_TOGGLE != 0) {                                                     \
    VEL_U[2]++;                                                                \
  }                                                                            \
  CLIMB_TOGGLE = 1 - CLIMB_TOGGLE_U

#endif /* OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H */
