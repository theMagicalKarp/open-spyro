/* level_funcs.h — main-EXE functions the level overlays call, declared once.
 *
 * Every prototype here is spelled identically in each overlay file that
 * used to declare it locally. A file that declares one of these functions
 * differently (a K&R form, an Actor * parameter) keeps its own declarations
 * and does not include this header: a prototype changes argument promotion. */

#ifndef OPEN_SPYRO_LEVEL_FUNCS_H
#define OPEN_SPYRO_LEVEL_FUNCS_H

extern int abs(int n);
extern int AbsAngleDelta12(int a, int b);
extern void AddSVector(short *dst, short *src, short *delta);
extern void AddVector(int *out, int *a, int *b);
extern void *AllocEmitListRecord(int cls);
extern int ArcTan2(int dy, int dx, int flag);
extern void BeginPauseToMenuTransition(void);
extern void BuildActiveActorList(void);
extern void BuildTextSpriteChain();                          /* draw text run */
extern void BuildTextSprites();                              /* BuildTextSpriteChain */
extern void ChangeSpyroState(int state);
extern int CheckLineOfSightSegment(int *from, int *to);
extern int CheckSpyroLineOfSightTo(int *pos);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2); /* ground/wall probe */
extern void CommitActorKillGems(int value, void *actor);
extern void ComputeCameraOrbitOffset(int *out);
extern void ConvertRGB15ToLuma8();                           /* texture-page sync */
extern void CopyVector();
extern void DivideVector3(int *dst, int *s, int k);
extern void DrawOTag();
extern void DrawShadedMenuBox();
extern void DrawSync();
extern void EmitRadialShadedLine();
extern void EndSaveMenuToWorld(void);
extern void EnqueueBlinkingMarkerSprite();
extern void EnqueuePendingSpritePrims();
extern void FillWord();                                      /* word fill */
extern void FreeEmitListRecord(unsigned char *rec);
extern int ReflectVectorOffNormal(int *v, int *n, int *out);
extern int GetRandomU32();
extern void InitHudCounters(int a0);
extern void InitLightVectorConstants(void);
extern int IsPointInRotatedBox(int *pos, int *rec);
extern void LoadAndStartLevelFromCd(void);
extern void LoadImage();
extern int LookupCosine(int ang);
extern int LookupSine(int ang);
extern void *memcpy();
extern void ProjectWorldPointGTEUnscaled(int *out, int *s);
extern void PutDispEnv();
extern void PutDrawEnv();
extern int RaiseSpyroTriggerInRadius(int *pos, int radius, int flags);
extern void RefreshHudLevelGemCounter(void);
extern void ResetCameraStateToTarget(void);
extern void ResetSpyroEntity(int a0);
extern void ResetSpyroLinearMotion(void);
extern void RotateVectorByMatrix();
extern void RShiftVector3(int *vec, int shift);
extern void SampleSpyroShadowRingHeights(int a0);
extern void ScaleVector3Sat(int *out, int *src, int scale);
extern int SignedAngleDelta12(int a, int b);
extern void sprintf();                                       /* format string */
extern int StepAngle8Toward(int to, int from, int maxStep);
extern void StopSoundVoiceByIndex(int ch, int mode);
extern void StoreImage();                                    /* MoveImage */
extern int strlen();                                         /* text glyph count */
extern void StubRewardHook(int script, int a);
extern void SubtractVector(int *out, int *a, int *b);
extern void TallyFlightTarget(int tallyClass);
extern void TickActiveSoundVoices(void);
struct Actor;
extern void TickActorAnims(struct Actor **list, int mode);
extern void TickLevelTransitionStream();
extern void TickSpyroAttackEffects(int a0);
extern void UnpackWorldCollisionTri(int chunk, int *out);
extern void UpdateCameraEulerAngles(void);
#ifndef VEC_LEN_XY
#define VEC_LEN_XY 0  /* VectorLength: X/Y only (Z ignored) */
#define VEC_LEN_XYZ 1 /* VectorLength: all three components */
#endif
extern int VectorLength(int *vec, int withZ);
extern int VSync();
extern int WrapAngleDelta8(int a, int b);                    /* signed angle delta */

/* Level/asset streaming helpers the credits overlay drives directly. */
extern void CdReadSyncSectors(int lbaBase, void *dst, int size, int offset, int marker);
extern void StartCdReadAsync(int lbaBase, void *dst, int size, int offset, int marker);
extern char *UnpackWorldDataChunks(int *p, int flag);
extern void CopyWords(void *dst, void *src, int len);
extern int TickCountdownByFrameDelta(void *timer, int step);
extern int RandomSignedInRange(int lo, int hi);

/* World/OT render entry points the credits backdrop calls. */
extern void FillBlock16(void *dst, int value, int count);
extern void RenderWorldChunks(int lod);
extern void EmitStaticActorMeshList(int lod, void *viewMtx, void *worldMtx);

#endif /* OPEN_SPYRO_LEVEL_FUNCS_H */
