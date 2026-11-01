/* main_shared.h — C declarations three or more matched main-EXE files spell
 * identically (prototypes, held-base views, scalar aliases), declared once.
 * funcs.h/globals.h keep the Ghidra-import signatures; these are the forms
 * the matched C actually uses.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_MAIN_SHARED_H
#define OPEN_SPYRO_MAIN_SHARED_H

#include "sparx.h"

extern void RestartStalledCdRead(void);

extern int CallCdSync();

extern int SetSpuTransferStartAddr(unsigned int addr);

extern unsigned int WriteSpuRam(void *buf, unsigned int size);

extern void ComputeCameraOrbitOffset(int *out);

extern int g_anCdReadInFlightBlock[]; /* held-base view of g_nCdReadInFlight */

extern int
    g_anWorldTimAudioChunkOffsetBlock[]; /* alias, held across the reads */

extern int g_anActorMeshOffsetBlock[]; /* [-16] is g_nWorldDataExtChunkOffset */
/* Its 0x40 actor-mesh offsets are followed by their g_apActorMeshTable
 * indices, one short each. */
#define ACTOR_MESH_INDEX_LIST(list) ((short *)((list) + 0x40))

extern void *g_apActorMeshTableBlock[]; /* incomplete-array view of
                                           g_apActorMeshTable */

/* Held-base alias of g_anCameraPos (0x80076df8): the [0] store and the
   SubtractVector argument share one base register while [1] and [2] stay
   absolute — same split as g_anSpyroWorldPosBlock in StoryIntro_Update. */
extern int g_anCameraPosBlock[];

/* Held-base views of the Spyro record, the request mask and the camera's
   next mode. */
extern int g_anSpyroWorldPosBlock[];

/* Held-base view of the spring angle set (yaw, pitch, distance, roll, pitch
   bias, yaw bias). */
extern int g_anCameraSpringYawBlock[];

extern int g_anSpyroDeathCamRecordBlock[];

extern int g_nSpyroWorldPosY; /* g_anSpyroWorldPos[1] */

extern int g_nSpyroWorldPosZ; /* g_anSpyroWorldPos[2] */

extern int g_anLevelEscapeRunCount;

extern int g_nLevelEscapeRunCountLane1;

extern int g_nCameraVisZone;

/* array alias for the sub-frame accumulator at 0x80078a7c (see
   config/symbol_addrs.txt) -- forces the held base register, matching the
   original's single `a1` reuse across the load + both stores. */
extern unsigned char g_abSpyroSubFrameTimerBlock[];

extern unsigned char g_bSpyroAnimLayer1Prev;

extern unsigned char g_bSpyroAnimLayer1FramePrev;

extern unsigned char g_bSpyroAnimLayer1Frame;

extern unsigned char g_abSpyroAnimLayer1SubstepBlock[]; /* array alias for
   the substep accumulator at 0x80078a7d, see config/symbol_addrs.txt --
   forces the held base register, matching the original's single `a1` reuse
   across the load + both stores. */

extern void IntegrateSpyroMotionForSubstep(int substep);

extern void AdvanceSpyroSubstepState(void);

extern int g_nSpyroPitchSpringVel;

extern int g_nSpyroRollSpringVel;

/* held-base view of g_nSpyroMoveTargetYaw (0x80078b24): [0] is the move-target
   yaw, [0x14] (+0x50) is g_nSpyroBodyYaw. */
extern int g_anSpyroMoveTargetYawBlock[];

extern unsigned char g_abSpyroStateTransitionMatrixBlock[45][45];

extern void TickSpuPerFrame(void);

extern void LoadActorMeshTables(void);

extern int g_nSfxOverrideFlags;

extern short g_nSfxOverrideVolL;

extern short g_nSfxOverrideVolR;

extern int g_nCdAudioVolume_blk;

/* Return to the current world's hub (0x8002c664): reset the level-transition /
   CD-stream state, arm a scripted respawn back into the hub (camera mode
   0x80000012, gamestate 1, tally + paused flags), tag the preserve-mob slot as
   empty, and snap the target level id back to the world's hub id
   ((active level / 10) * 10). */
extern void InitLightVectorConstants(void);

/* Held-base alias for &g_nCameraEulerPitch: the pitch reads and both output
   matrix addresses (view @ base-0x3A, world-to-camera @ base-0x4E) share it. */
extern short g_anCameraEulerPitchBlock[];

extern void ResetCameraStateToTarget(void);

extern int g_nSpyroMotionAux;

extern int ComputeSaveGameChecksum(unsigned char *data);

extern int g_anSpyroBodyPitchBlock[]; /* +0 pitch, [-12] g_anSpyroVelocity */

extern void TickSpyroGameplayFrame(void);

extern void UpdateCameraFrame(void);

extern void TickCdMusicStream(void);

extern int g_nSpyroHazardKind;

extern int g_bSpyroJumpHoldForced;

extern void BeginStoryIntro(void);

extern int g_nLevelLoadChunkSize;            /* level-load chunk size */

extern int g_nLevelLoadByteOffset;            /* level-load byte offset */

extern void BuildActorDrawList(void);

extern void EmitActorDrawList(void);

extern void RasterizeSpritePrimQueue(void);

extern void RasterizeSpyroModel(void);

extern void ResetSpyroLinearMotion(void);

extern int g_anSparxHealthBlock[];     /* g_nSparxHealth */

extern void EnqueuePendingSpritePrims(void);

extern void UpdateHudCounters(void);

extern void PutDrawEnv(void *env);

extern void *LinkOTPrimitives(int depth_max);

extern void DrawOTag(void *ot);

extern int g_anVsyncFrameEndBlock[]; /* held-base alias: [-1] = pace anchor */

extern void BuildRenderEntityLists(void);

extern void ComposeFrameScene(void);

extern void RasterizeEmitList(void);

extern void DrawPortalsAndStaticActors(void);

extern void SetupFrameOT(void);

extern void DrawCinematicLetterbox(void);

extern void TickSpyroAnimLayer1(void);

extern void TickSpyroAnimLayer2(void);

extern void BeginCutscene(void);

extern short
    g_nCdDaVolumeCurrent;  /* live CD-DA volume (fade ramp current value) */

extern short g_anCameraEulerYawBlock[];

extern int g_nActorFrameDelta;

extern short g_nSpuCommonCdVolumeR;           /* SpuCommonAttr CD volume R */

/* Snap the spring camera to its per-mode target (0x80034358, 0x128).
   Loads the target angles for the current mode, copies all six target
   angles into the spring set, zeroes the spring delta state and the
   collision/stuck counters, recomputes the eye position (orbit offset +
   anchor), seats the smoothed position on it, refreshes the euler angles,
   and clears any camera shake. */
extern void LoadCameraTargetAnglesFromMode(void);

extern void ProjectWorldPointGTEUnscaled(int *out, int *pos);

extern void ScaleVector3Sat(int *dst, int *src, int scale);

extern unsigned int g_adwSpuPendingKeyOffBlock[];

extern unsigned char g_abSpyroPersistentEulerBlock[]; /* 0x80078a64 */

extern void TickSpyroAnimStateMachine(void);

extern void TickSpyroFlameAttack(void);

extern void TickSpyroAnimLayer2GateTimer(void);

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern void AdvanceCameraSpringStep(int column);
extern void ApplyGravityAlongSpyroSlopeNormal(void);
extern void ApplyGravityAlongSpyroSlopeTangent(void);
extern int ApproachAngle8(int target, int cur, int step, int lim);
extern void BuildCameraViewMatrix(void);
extern short g_anTweenBlendWeights4[];
extern short g_anTweenBlendWeights6[];
extern short g_anTweenBlendWeights8[];
extern short g_anTweenBlendWeights2[];
extern void DrawFullscreenTint(int slot, int r, int g, int b);
extern void EmitStaticActorMeshListFogged(void);
extern void EnqueueHudSprites(void);
extern long GetSpuTransferStatus(int mode);
extern void InitActorMeshScratchRegions(int mode);
extern void InitNewGameState(void);
extern void RelocateMobActorTable(void *table);
extern void RotateLightVectorXZ(int steps);
extern void SetSpuCommonAttr(unsigned char *attr);
extern void SnapCameraToTarget(void);
extern int g_abDrawBufPool;          /* draw-buffer sentinel */
extern unsigned int g_adwSpyroRequestMaskBlock[];
extern int g_anCameraViewMtx[]; /* camera view matrix (worldToCam at -0x14) */
extern int g_anDragonRescueAuraStarBlock[]; /* 0x80076248 aura-star state block */
extern int g_anSaveMenuBlock[];
extern int g_anSpawnAnchorBlock[];
extern int g_anSpyroStateBlock[];
extern int *g_apCameraAnchorBlock[]; /* alias of g_pCameraAnchorPos */
extern void *g_apDrawBufBlock[];  /* alias of g_pDrawBufA */
extern void *g_apRenderScratchPrimTopBlock[]; /* held-base alias of g_pRenderScratchPrimTop;
                                              * [-2] is g_pRenderScratchRegionBase */
extern void *g_apActiveActorListBlock[];
extern char g_szContinue[];       /* "CONTINUE"    */
extern unsigned char g_szReturningHome[]; /* "RETURNING HOME..." */

/* Declared identically by two or more main files; hoisted here. */
extern char *g_apWorldUnpackHeadBlock[];
extern char *volatile g_pActiveCutsceneSoundDef;
extern int CdInit(void);
extern int g_anTitleModeBlock[];
extern int g_anDragonRescueBlock[];
extern int g_anSpyroBodyMtxBlock[];
extern int g_anSpyroInputBlankBlock[];
extern int g_anSpyroSlopeGravityVec[];
extern int g_anSpyroSubstateBlock[];
extern int g_anSpyroWorldPosZBlock[];
extern int g_anStoryIntroTimerBlock[];
extern int g_anWorldRenderMeshBlock[];
extern int g_nCameraNextMode_blk[];
extern int g_nFarPlaneDrawDistance;
extern int g_nLevelSpawnPosZ;
extern int g_nSaveMenuCameraFromZ;
extern int g_nSaveMenuCameraToAngle;
extern int g_nSaveMenuCameraToDist;
extern int g_nSaveMenuCameraToZ;
extern int g_nSaveMenuHostFromZ;
extern int g_nSaveMenuHostToAngle;
extern int g_nSaveMenuHostToDist;
extern int g_nSaveMenuHostToZ;
extern int g_nSaveMenuSparxFromAngle;
extern int g_nSaveMenuSparxToAngle;
extern int g_nSaveMenuSparxToDist;
extern int g_nSaveMenuSpyroToYaw;
extern int g_nSaveSlotLoaded;
extern int g_nSpyroAnimLayer2Request;
extern int g_nSpyroAnimPlayRate_blk[];
extern int g_nSpyroCarryMotionZ;
extern int g_nSpyroScriptedLiftBaseZ;
extern int g_nSpyroScriptedLiftRise;
extern int g_nSpyroScriptedLiftTurn;
extern int g_nSpyroScriptedLiftYaw;
extern int g_nSpyroVelocityZ;
extern int g_nTitleSubState;
extern unsigned char g_abSpyroFlameAnchorModeBlock[];
extern unsigned char g_abSpyroFlameAnimTable[];
extern unsigned char g_bSpyroAnimLayer1Current;
extern unsigned char g_bSpyroAnimLayer2Current;
extern unsigned char g_bSpyroAnimLayer2Frame;
extern unsigned char g_bSpyroAnimLayer2FramePrev;
extern unsigned char g_bSpyroAnimLayer2Prev;
extern unsigned int g_adwPadHeldBlock[];
extern unsigned int g_adwPadPressedBlock[];
extern void *g_apOtDepthBinBlock[];
extern void (*g_pfnActorCutsceneSubstateDraw)(void);
extern void BuildSecondaryActorDrawList(void);
extern void ClearPadHistoryRing(void);
extern void ComputeVoicePanAndFalloff(short *vol, int dist, unsigned char bearing, unsigned int range, short *cached);
extern void DrawEntityDirectionalSpikes(void);
extern void DrawSpyroDropShadow(void);
extern void EmitSecondaryActorPrimitives(void);
extern void MapPadAxisToCalibratedRange(unsigned char *cal, int block);
extern void NudgeSpyroFromWallProbes(void);
extern void SetSpuKeyMask(int on, unsigned int voices);
extern void TriggerRespawnOrGameOver(void);
extern void srand(unsigned int seed);

/* Declared identically (by type) in two or more including files; hoisted here. */
extern void AddVector(int *dst, int *a, int *b);
extern int ArcTan2(int y, int x, int high_precision);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern void SubtractVector(int *dst, int *a, int *b);
extern void ZeroVector(int *vec);
extern int g_anFlightTargetTally;
extern int g_bDebugNoDepletion;
extern unsigned char g_bSpyroSwingByteEuler1;
extern int g_nCreditsFirstVisibleLine;
extern int g_nCreditsLastVisibleLine;
extern int g_nCreditsLineCount;
extern int g_nCreditsPageIndex;
extern int g_nCreditsMode;
extern int g_nScreenFadeLevel;
extern int g_nLevelIntroParallaxCountdown;
extern int g_nVisZoneCount;

/* C library. */
extern int abs(int x);

#endif /* OPEN_SPYRO_MAIN_SHARED_H */
