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

extern int g_nSpyroInputLockoutCountdown[]; /* [-0x58] is the Spyro world position (g_anSpyroWorldPos) */

/* g_anSpyroWorldPos reached through the lockout counter's held base. */
#define SPYRO_POS_VIA_LOCKOUT (g_nSpyroInputLockoutCountdown - 0x58)

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
#define VEL ((short *)(rec + 0x18))            /* velocity / spin, svector */
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
#define SWIRL_ANGLE rec[0x18]
#define SWIRL_RADIUS rec[0x19]
/* Fire-jet bands: spin angle and segment index along the jet. */
#define JET_ANGLE rec[0x1C]
#define JET_SEGMENT rec[0x1D]
/* Fade all three colour bytes by `n`. */
#define FADE_COL(n)                                                            \
  COL_R -= (n);                                                                \
  COL_G -= (n);                                                                \
  COL_B -= (n)

/* Spark drift: every other tick the z velocity (VEL[2], updated through its
 * unsigned view) climbs by one; the short at 0x1E is the 0/1 toggle. */
#define SPARK_Z_CLIMB()                                                        \
  if (HS(0x1E) != 0) {                                                         \
    HU(0x1C)++;                                                                \
  }                                                                            \
  HS(0x1E) = 1 - HU(0x1E)

#endif /* OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H */
