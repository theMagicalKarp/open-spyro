/* level_actors.h — main-EXE actor-system globals and functions the level
 * overlays' actor code declares, declared once.
 *
 * Every declaration here is spelled identically in each overlay file that
 * used to declare it locally; files that declare any of them differently
 * keep their own declarations and do not include this header. */

#ifndef OPEN_SPYRO_LEVEL_ACTORS_H
#define OPEN_SPYRO_LEVEL_ACTORS_H

#include "actor.h"

extern void ComputeCameraToTargetAngle(int *out);
extern int g_anRotBillboardOffsetsRand[][3];
extern Actor *g_pThiefEggDrop;
extern int g_anSpuVoiceRowPitch[][7];                        /* g_aSpuVoiceRows[i].nPitch (rows are 7 words) */
extern int g_anSpuVoiceRowPitchStep[][7];                    /* g_aSpuVoiceRows[i].nPitchStep */
extern int g_nGamestate0cDialogPage;
extern Actor g_abHudIconActorRecords[];                      /* the four HUD class icons */
extern int g_anCollisionResolvedPos;                         /* collision contact point */
extern short g_anCosineLut[];
extern int g_anFlightTargetTally[];                          /* tally collected, per class */
extern int g_anLevelDragonsRescued[];                        /* per-g_nLevelIntroIndex completion counters */
extern int g_anLevelEggsRecovered[];
extern short g_anSineLut[];
extern int g_anSpyroMotionVec;                               /* look-at offset */
extern int g_anSpyroWorldPos;                                /* Spyro, as a plain symbol (the loop's spy carrier) */
extern int g_anWorldPersistentFlags[];                       /* per-mode "time attack" flag */
extern Actor *g_apSaveMenuAnimActorList[];                   /* live actor list, 0-terminated */
extern int g_dwPadPressed;                                   /* pad buttons pressed; 0x40 skips the egg */
extern short g_nCameraEulerPitch;
extern int g_nCameraScriptStickRequestY[];
extern int g_nCollisionResolvedZ;                            /* ..z */
extern int g_nSpyroWorldPosY;
extern Actor *g_pActorListBase;                              /* level actor pool */
extern Actor *g_pActorPoolDynBase;                           /* end of the level actor pool */
extern void (*g_pfnLevelBeginActorCutscene)(Actor *actor);
extern void (*g_pfnLevelEmitParticles)(int a, int effect, int *pos, void *c);
extern void (*g_pfnLevelFallImpactCallback)(void);           /* out-of-time handler */
extern Actor *(*g_pfnLevelSpawnActor)(int type, Actor *parent);
extern Actor *g_pLevelOverlayInitRecord;                     /* Sparx, or null when absent */
extern unsigned char *g_pLevelSampleBankEntries;             /* sound definitions, 20 bytes each */
extern unsigned char *g_pLevelSampleBankHeader;              /* sound id table */
extern Actor *g_pScriptedCameraActor;
extern int *g_pSpyroRequestCameraAnchor;                     /* SPY.pCamAnchor, absolute form */
extern unsigned char **g_pTriggerRegionTable;                /* surface records, indexed by the above */
extern int RandomIntInRange(int lo, int hi);                 /* random in [lo, hi] */
extern int RandomSignedInRange(int a, int b);
extern int StepAndLerpAngle8(int ang, int facing, int a, int b);
extern int TickCountdownByFrameDelta(void *timer, int step);
extern void VectorToSVectorShr2(short *d, int *s);

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern int CastRayWorldAndActors(int *a, int *b);
extern int ComputeStepTowardPoint(int *from, int *to, int *vel, int a);
extern int D_80078BBC[]; /* balloon sequence stage (== SPY.u164) */
extern int GetWorldChunkAnimState(int handle); /* bits 8..15 are the anim marker */
extern void ModifyWorldChunkAnimFlags(int handle, int a, int b);
extern int PickupGroupHasLiveActor(int stage);
extern int PlaneDistanceAbs(int *pos, void *st);
extern void SetWorldChunkAnimStep(int handle, int a, int b);
extern int WrapAddAngle8(int a, int b);
extern int g_anActorShardOffsets[];  /* shard offset vectors, 3 ints each */
extern int g_anMotionTrailSliceCounts[];       /* trail lengths */
extern signed char g_bSpyroRequestYaw; /* g_abSpyroRequestEuler[2]: requested yaw */

#endif /* OPEN_SPYRO_LEVEL_ACTORS_H */
