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
#define ALIVE rec[0x3]                         /* 0 = kill at the next tick */
#define HS(n) (*(short *)(rec + (n)))          /* s16 field at +n */
#define HU(n) (*(unsigned short *)(rec + (n))) /* u16 field at +n */
#define W(n) (*(int *)(rec + (n)))             /* s32 field at +n */
#define POS ((short *)(rec + 4))               /* tail point, svector */
#define VEL ((short *)(rec + 0x18))            /* velocity / spin, svector */

/* Spark drift: every other tick the z velocity (VEL[2], updated through its
 * unsigned view) climbs by one; the short at 0x1E is the 0/1 toggle. */
#define SPARK_Z_CLIMB()                                                        \
  if (HS(0x1E) != 0) {                                                         \
    HU(0x1C)++;                                                                \
  }                                                                            \
  HS(0x1E) = 1 - HU(0x1E)

#endif /* OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H */
