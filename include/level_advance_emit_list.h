/* level_advance_emit_list.h — declarations shared by the *_AdvanceEmitList.c clones, declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H
#define OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H

extern void AddSVector(short *dst, short *src, short *delta);

extern void SVectorToVectorShl2(int *dst, short *src);

extern void VectorToSVectorShr2(short *dst, int *src);

extern int abs(int n);

extern void FreeEmitListRecord(unsigned char *rec);

extern unsigned char *g_pEmitListBase;

extern char *g_pActorListBase;

extern short g_anSineLut[];        /* sine table */

extern short g_anCosineLut[];        /* cosine table */

extern int g_anCameraPos[];

extern int g_nSpyroInputLockoutCountdown[]; /* [-0x58] is the Spyro world position (g_anSpyroWorldPos) */

extern int g_nLevelReadyFlag;

#endif /* OPEN_SPYRO_LEVEL_ADVANCE_EMIT_LIST_H */
