/* level_funcs.h — main-EXE functions the level overlays call, declared once.
 *
 * Every prototype here is spelled identically in each overlay file that
 * used to declare it locally. A file that declares one of these functions
 * differently (a K&R form, an Actor * parameter) keeps its own declarations
 * and does not include this header: a prototype changes argument promotion. */

#ifndef OPEN_SPYRO_LEVEL_FUNCS_H
#define OPEN_SPYRO_LEVEL_FUNCS_H

#include "types.h"

extern int abs(int n);
extern int AbsAngleDelta12(int a, int b);
extern void AddSVector(short *dst, short *src, short *delta);
extern void AddVector(int *out, int *a, int *b);
extern void *AllocEmitListRecord(int cls);
#ifndef ATAN_ANGLE8
#define ATAN_ANGLE8 0  /* ArcTan2: 8-bit angle, 0..0xFF per turn */
#define ATAN_ANGLE12 1 /* ArcTan2: 12-bit angle, 0..0xFFF per turn (interpolated) */
#endif
extern int ArcTan2(int dx, int dy, int scale);
extern void BeginPauseToMenuTransition(void);
extern void BuildActiveActorList(void);
extern void BuildTextSpriteChain(); /* K&R: callers pass int-typed text buffers */
extern void *BuildTextSprites(char *text, int *pos, int *attr, int spacing, int flags);
extern void ChangeSpyroState(int state);
extern int CheckLineOfSightSegment(int *from, int *to);
extern int CheckSpyroLineOfSightTo(int *pos);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2); /* ground/wall probe */
struct Actor;
extern void CommitActorKillGems(int value, struct Actor *actor);
extern void ComputeCameraOrbitOffset(int *out);
extern void ConvertRGB15ToLuma8(void *buf, int count);
extern void CopyVector(int *dst, int *src);
extern void DivideVector3(int *dst, int *s, int k);
extern void DrawOTag(void *ot);
extern void DrawShadedMenuBox(int left, int right, int top, int bottom);
extern int DrawSync(int mode);
extern void EmitRadialShadedLine(int x0, int y0, int x1, int y1);
extern void EndSaveMenuToWorld(void);
extern void EnqueueBlinkingMarkerSprite(int *pos, int frame, int tile);
extern void EnqueuePendingSpritePrims(void);
extern void FillWord(void *dst, unsigned int value, int bytes);
extern void FreeEmitListRecord(unsigned char *rec);
extern int ReflectVectorOffNormal(int *v, int *n, int *out);
extern int GetRandomU32();
extern void InitHudCounters(int layout_butterflies);
extern void InitLightVectorConstants(void);
struct TriggerBox;
extern int IsPointInRotatedBox(int *pos, struct TriggerBox *box);
extern void LoadAndStartLevelFromCd(void);
extern void LoadImage(); /* K&R: callers pass their own rect views */
extern int LookupCosine(int ang);
extern int LookupSine(int ang);
extern void *memcpy(void *dst, const void *src, unsigned int n);
extern void ProjectWorldPointGTEUnscaled(int *out, int *s);
extern DISPENV *PutDispEnv(DISPENV *env);
extern void PutDrawEnv(void *env);
extern int RaiseSpyroTriggerInRadius(int *pos, int radius, int flags);
extern void RefreshHudLevelGemCounter(void);
extern void ResetCameraStateToTarget(void);
#ifndef RESET_SPYRO_SOFT
#define RESET_SPYRO_SOFT 0 /* clears input/charge flags, rebuilds body angles */
#define RESET_SPYRO_FULL 1 /* wipes the Spyro block (keeps pos, euler, Sparx), state IDLE */
#endif
extern void ResetSpyroEntity(int full);
extern void ResetSpyroLinearMotion(void);
extern void RotateVectorByMatrix(int *mtx, int *src, int *dst);
extern void RShiftVector3(int *vec, int shift);
extern void SampleSpyroShadowRingHeights(int probe);
extern void ScaleVector3Sat(int *out, int *src, int scale);
extern int SignedAngleDelta12(int a, int b);
extern void sprintf(); /* K&R: callers format into int-typed buffers */
extern int StepAngle8Toward(int to, int from, int maxStep);
#ifndef VOICE_RELEASE
#define VOICE_RELEASE 4 /* StopSoundVoiceByIndex: key off and free the voice row (2 does the same; other modes only key off) */
#endif
extern void StopSoundVoiceByIndex(int ch, int mode);
extern void StoreImage(); /* K&R: the flight results draw passes a stray third argument (the original sets $a2) */
extern int strlen();                                         /* text glyph count */
extern void StubRewardHook(int script, int a);
extern void SubtractVector(int *out, int *a, int *b);
extern void TallyFlightTarget(int tallyClass);
extern void TickActiveSoundVoices(void);
struct Actor;
extern void TickActorAnims(struct Actor **list, int mode);
extern void TickLevelTransitionStream(int full);
extern void TickSpyroAttackEffects(int steps);
extern void UnpackWorldCollisionTri(int chunk, int *out);
extern void UpdateCameraEulerAngles(void);
#ifndef VEC_LEN_XY
#define VEC_LEN_XY 0  /* VectorLength: X/Y only (Z ignored) */
#define VEC_LEN_XYZ 1 /* VectorLength: all three components */
#endif
extern int VectorLength(int *vec, int withZ);
extern int VSync(int mode);
extern int WrapAngleDelta8(int a, int b);                    /* signed angle delta */

/* Level/asset streaming helpers the credits overlay drives directly. */
extern void CdReadSyncSectors(int lbaBase, void *dst, int size, int offset, int marker);
extern void StartCdReadAsync(int lbaBase, void *dst, int size, int offset, int marker);
extern char *UnpackWorldDataChunks(int *p, int flag);
extern void CopyWords(void *dst, void *src, int len);
extern int TickCountdownByFrameDelta(void *pValue, int width);

#ifndef TICK_DOWN
/* Count the 1-, 2- or 4-byte lvalue `v` down by the frame delta (see
 * TickCountdownByFrameDelta): 0 still counting, 2 reached zero now, 1 was
 * already zero. */
#define TICK_DOWN(v) TickCountdownByFrameDelta(&(v), sizeof(v))
#endif
extern int RandomSignedInRange(int lo, int hi);

/* World/OT render entry points the credits backdrop calls. */
extern void FillBlock16(void *dst, int value, int count);
extern void RenderWorldChunks(int lod);
extern void EmitStaticActorMeshList(int lod, void *viewMtx, void *worldMtx);

#endif /* OPEN_SPYRO_LEVEL_FUNCS_H */
