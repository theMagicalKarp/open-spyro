/* level_globals.h — main-EXE globals the level overlays read, declared once.
 *
 * Every declaration here is spelled identically in each overlay file that
 * used to declare it locally. Overlays build with -G0, so a declaration
 * only matters through its type; files that view one of these symbols
 * through a different type keep their own declaration and do not include
 * this header. */

#ifndef OPEN_SPYRO_LEVEL_GLOBALS_H
#define OPEN_SPYRO_LEVEL_GLOBALS_H

#include "actor_type.h"

extern int g_anSparkleGlowDesc;
extern int g_anFlightHudClassWords[];
extern void *g_dwBurstColorType15;
extern void *g_dwBurstColorType14;
extern int g_anTintByDropKind[]; /* packed Actor tint word per dropKind (see actor.h) */
/* Flight results board rows, indexed world * 4 (titles) or world * 5 (bonus
 * values / claimed flags); _rN is row N of the same table, declared as its
 * own symbol because the original addresses each row directly. */
extern char *g_apFlightRowTitles[];                               /* row title string tables */
extern char *g_apFlightRowTitles_r1[];
extern char *g_apFlightRowTitles_r2[];
extern char *g_apFlightRowTitles_r3[];
extern int g_anFlightCourseBonusTable_r1[];
extern int g_anFlightCourseBonusTable_r2[];
extern int g_anFlightCourseBonusTable_r3[];
extern int g_anFlightCourseBonusTable_r4[];
extern unsigned char g_abGemTallyIconGlyphs[];
extern int g_anActorCutsceneLandingX[];
extern int g_anActorCutsceneLandingY[];
extern char *g_apWorldNameByLevelTens[];                               /* world names */
extern char *g_apLevelNameByWorldRow[];                               /* level names */
extern unsigned char g_abBalloonistDialoguePageEnds[];                       /* dialogue page end line */
extern int g_pActiveActorList1;
extern Actor g_abActorCutsceneActorSnapshot[]; /* focus-actor copy taken at lift-off */
extern int g_bDebugNoDepletion;
extern unsigned char g_bWorldBalloonistState1;                         /* per-world "seen it" flags */
extern int *g_apActorMeshTable_1A0[]; /* g_apActorMeshTable[0x1A0] (balloonist cutscene relocates it) */
extern int g_nLevelEggsRecovered1;
extern int g_nLevelEggsRecovered7;
extern int g_nLevelEggsRecovered8;
extern int g_nLevelEggsRecovered12;
extern int g_nLevelEggsRecovered15;
extern int g_anActorCutsceneTimerBlock_s;
extern int g_nActorCutsceneMenuCursor_s;
extern int g_nActorCutsceneDialogPage_s;                                 /* same address, scalar DECL -> absolute lui/%lo form (B16 per-site) */
extern int g_nActorCutsceneDialogVariant;
extern int g_anActorCutsceneSpyroFrom[];
extern int g_nActorCutsceneSpyroFromY;
extern int g_nActorCutsceneSpyroFromZ;
extern int g_nActorCutsceneSpyroToX;
extern int g_nActorCutsceneSpyroToY;
extern int g_nActorCutsceneSpyroToZ;
extern int g_nActorCutsceneSpyroYawStart;
extern int g_nActorCutsceneSpyroYawGoal;
extern int g_nActorCutsceneCamEulerRollStart;
extern int g_nActorCutsceneCamEulerPitchStart;
extern int g_nActorCutsceneCamEulerYawStart;
extern int g_nActorCutsceneRecordZStart;
extern int g_nActorCutsceneRecordZGoal;
extern unsigned char g_abHudIconActorRecords_rotZ[];
extern int g_anFlightTallyActiveSlots_e1; /* queue slots 1..3, shifted down as each class is shown */
extern int g_anFlightTallyActiveSlots_e2;
extern int g_anFlightTallyActiveSlots_e3;
extern unsigned char g_abFlightBonusPaid_r1[];
extern unsigned char g_abFlightBonusPaid_r2[];
extern unsigned char g_abFlightBonusPaid_r3[];
extern unsigned char g_abFlightBonusPaid_r4[];
extern int g_nSpyroMotionY;
extern int g_nSpyroRequestTargetZ;
extern unsigned char g_abBalloonistDialoguePageStarts[]; /* dialogue page first line */
extern unsigned char g_abEmitTrailColors[];              /* trail colour table, stride 4 */
extern unsigned char g_abEmitTrailColorsB[];
extern unsigned char g_abEmitTrailColorsG[];
extern unsigned char g_abGemTallyIconUvTable[];
extern unsigned char g_abHudCounterDisplayState;
extern unsigned char g_abLevelExited[];              /* per-level "left through the exit" flags */
extern unsigned char g_abWorldBalloonistState[];             /* per-world balloonist progress 0/1/2 */
extern unsigned char g_abSpyroStateCameraMode[];
extern unsigned char g_abFlightBonusPaid[];       /* per flight level bonus-paid flags */
extern void *g_adwActorBurstColors[];
extern int g_anEmitSlotOffsets[];                        /* per-slot offsets, stride 3 ints */
extern int g_anEmitStepOffsets[];                        /* per-step offsets, stride 3 ints */
extern int g_anFlightCourseBonusTable[];                 /* per-course bonus table rows */
extern int g_anFlightTallyActiveSlots;                   /* pending tally-class queue, 4 deep */
extern int g_anFlightTallyShown[];                       /* tally shown, per class */
extern short g_anLaunchDirTable[];                       /* launch-direction table, x */
extern short g_anLaunchDirTableZ[];                      /* launch-direction table, z */
extern int g_anRotBillboardOffset0;
extern int g_anRotBillboardOffset1;
extern int g_anRotBillboardOffset2;
extern int g_anRotBillboardOffset3;
extern int g_anRotBillboardOffset4;
extern int g_anRotBillboardOffset5;
extern int g_anSparxEmitOffset[];
extern int g_anSpyroFirstContactNormal[3];
extern int g_anSpyroFirstContactPoint[3];
extern int g_anSpyroVelocity[3];
extern unsigned char g_bSpyroAnimCurrent;
extern unsigned char g_bSpyroAnimFrame;
extern unsigned char g_bSpyroAnimPrev[];
extern unsigned char g_bSpyroFramePrev;
extern unsigned char g_bSpyroPersistentEulerY;           /* SPY.bodyRotY, absolute form */
extern unsigned char g_bSpyroSubFrameTimer;
extern unsigned char g_bSpyroSwingByteEuler1;
extern int g_dwGamestateFrames;                          /* frame counter; the arm acts on every 4th frame */
extern int g_dwSpyroRequestMask;                         /* scripted camera control word */
extern int g_dwSpyroTriggerEventFlags;                   /* SPY.triggerEvents, absolute form */
extern int g_nActiveLevelId;                             /* active level id */
extern int g_nActiveWorldIndex;                          /* course index */
extern int g_nActorAnimDone;                             /* current actor's animation finished */
extern int g_nActorAnimNewFrame;                         /* current actor's new-frame flag */
extern int g_nActorPoolCapacity;
extern int g_nActorPoolLiveCount;
extern int g_nCameraCurrentMode;                         /* camera mode */
extern short g_nCameraEulerRoll;
extern short g_nCameraEulerYaw;                          /* camera yaw, as the balloonist reads it */
extern int g_nCameraNextMode;
extern int g_nCameraPosY;                                /* turret aim target Y */
extern int g_nCameraPosZ;
extern int g_nSpyroSwingEulerTargetZ;
extern int g_nCameraTargetYawBiasFromMode;
extern int g_nCdStreamState;
extern int g_nCollisionProbeVecY;
extern int g_nCollisionProbeVecZ;
extern int g_nCollisionResolvedY;                        /* ..y */
extern int g_nCurrentLevelId;                            /* current level id */
extern int g_nDemoActive;
extern int g_nExtraLives;                                /* life count, capped at 99 */
extern int g_nFlightQuitFlag;
extern int g_nFlightResultsRevealStep;                   /* reveal sound step */
extern int g_nFlightRunClock;                            /* run clock */
extern int g_nFlightTallyShownClass;                     /* tally class being shown */
extern int g_nFlightTallySlot;                           /* tally slot index */
extern int g_nFlightTargetTally1;
extern int g_nFlightTargetTally2;
extern int g_nFlightTargetTally3;
extern int g_nFlightTimeBonus;                           /* time bonus accumulator */
extern int g_nFlightTimeRemaining;                       /* time remaining */
extern int g_nFrameStep;                                 /* frame delta in ticks */
extern int g_nGamePaused;
extern int g_nCinematicActive;
extern int g_nGamestate;                                 /* g_nGamestate */
extern int g_nActorCutsceneCamHeightGoal;
extern int g_nActorCutsceneCamHeightStart;
extern int g_nActorCutsceneCamRadiusGoal;
extern int g_nActorCutsceneCamRadiusStart;
extern int g_nActorCutsceneCamYawGoal_s;                   /* same address, scalar DECL -> absolute lui/%lo form (B16 per-site) */
extern int g_nActorCutsceneCamYawStart;
extern int g_nActorCutsceneCardVariant;
extern int g_nDragonRescueSubstate;
extern int g_nDragonRescueTimer;
extern int g_nGemTallyIconCount;
extern int g_nGroundSurfaceType;                         /* index of the surface under the probe */
extern int g_nLastCollisionHitChunkId;                   /* collision triangle index */
extern int g_nLevelKeyState;                             /* key collected */
extern int g_nLevelIndex;
extern int g_nLevelIntroParallaxCountdown;
extern int g_nLevelTransitionPhase;
extern int g_nLevelTransitionTallyActive;
extern int g_nPauseMenuCursor;                           /* menu cursor */
extern int g_nPauseMenuIdleFrames;                       /* idle/blink counter */
extern int g_nPauseMenuTransitionFrames;                 /* results frame counter */
extern int g_nEnteredPortalIdx;
extern int g_nPrimBufferOverflowFlag;
extern int g_nReturnPortalLevelId;
extern int g_nPortalViewPitchTilt;
extern int g_nPortalViewYawSpin;
extern int g_nPortalTransitionActive;
extern int g_nSfxOverrideFlags;
extern short g_nSfxOverrideVolL;                         /* sound volume override, left/right */
extern short g_nSfxOverrideVolR;
extern int g_nSparxFoodCount;                            /* life orb count */
extern int g_nSpyroAirborneFrames;
extern int g_nSpyroAnimPlayRate;
extern int g_nSpyroBodyYaw;                              /* camera yaw accumulator */
extern int g_nSpyroDrawSuppressed;                       /* Spyro is being carried */
extern int g_nSpyroFlightMode;
extern int g_nSpyroSuperflameTimer;
extern int g_nSpyroSuperflameTimerSave;
extern int g_nSpyroGroundHeightZ;                        /* g_nSpyroGroundHeight */
extern int g_nSpyroFlameVariant;
extern int g_nSpyroMotionSpeed;
extern int g_nSpyroMotionZ;
extern int g_nSpyroPadSnapshotCountdown;
extern int g_nSpyroRequestState;                         /* scripted camera mode */
extern int g_nSpyroRequestVelocityZ;
extern int g_nSpyroState;                                /* Spyro's state */
extern int g_nSpyroSubstate;
extern int g_nSpyroStateTimer;
extern int g_nSpyroSwingEulerTargetX;
extern int g_nSpyroSwingEulerPrev1;
extern int g_nSpyroWorldPosZ;                            /* SPY.posZ, absolute form */
extern int g_nSubMenuCursor;                             /* reveal timer */
extern int g_nSubMenuScrollOffset;
extern int g_nVsyncFramePaceAnchor;
extern int g_nWorldDragonsRescued;
extern int g_nWorldEggsRecovered;
extern int g_nWorldTreasureCollected;                    /* gem total shown on the balloon */
extern struct Actor *g_pActorCutsceneActor;  /* focus actor (BeginActorCutscene) */
extern unsigned char *g_pLevelSoundIds;                  /* level sound ids */
extern void *g_pSpyroRequestCameraTargetParams;          /* SPY.pCamTarget, absolute form */

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern int g_nCreditsGlyphCount;
extern int g_nCreditsPageIndex;
extern char *g_pCreditsGlyphPool;

/* Declared identically by two or more overlay files; hoisted here. */
extern void MoveImage();
extern int (*g_pfnFlightTimeFormatter)();
extern void (*g_pfnLevelHandleFlightResults)();

/* Declared identically (by type) in two or more including files; hoisted here. */
extern void BuildActiveActorList(void);
extern int CheckLineOfSightSegment(int *from, int *to);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern void CommitActorKillGems(int value, void *actor);
extern int FindGroundHeightBelow(int *pos, int depth);
extern int IsPointInRotatedBox(int *pos, int *rec);
extern int RaiseSpyroTriggerInRadius(int *pos, int radius, int flags);
extern int ReflectVectorOffNormal(int *v, int *n, int *out);
extern void RefreshHudLevelGemCounter(void);
extern void ResetSpyroEntity(int hard);
extern void ResetSpyroLinearMotion(void);
extern int StepAngle8Toward(int to, int from, int maxStep);
extern void StubRewardHook(int script, int a);
extern void UnpackWorldCollisionTri(int chunk, int *out);
extern int WrapAngleDelta8(int a, int b);
extern unsigned char g_abLevelVisitedFlag[];
extern int g_anLevelTriggerStateSlots[];
extern int g_anRunningKillBitmap[];
extern int g_anFlightClearTimes[];
extern int g_nCutsceneId[]; /* [0] the cutscene id; [1]/[2] are g_nLevelVortexSeen1/2, indexed by LootVortexState.seen */
extern int g_nCreditsMode;
extern int g_nScreenFadeLevel;


/* World / level CD-stream chunk bookkeeping and credits-sequence state
 * (main .bss; the credits overlay reads them). */
extern int g_nReturningHomePhase;
extern int g_nPortalCount;
extern int g_nWorldTimAudioChunkSize;
extern int g_nWorldBundleChunkOffset;
extern int g_nWorldBundleChunkSize;
extern int g_nLevelArchiveChunkOffset;
extern int g_nLevelArchiveBaseLba;

#endif /* OPEN_SPYRO_LEVEL_GLOBALS_H */
