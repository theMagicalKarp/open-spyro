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
extern Actor *g_pLockedChestReward;
extern int g_anSpuVoiceRowPitch[][7];                        /* g_aSpuVoiceRows[i].nPitch (rows are 7 words) */
extern int g_anSpuVoiceRowPitchStep[][7];                    /* g_aSpuVoiceRows[i].nPitchStep */
extern int g_nActorCutsceneDialogPage;
extern Actor g_abHudIconActorRecords[];                      /* the four HUD class icons */
extern int g_anCollisionResolvedPos;                         /* collision contact point */
extern short g_anCosineLut[];
extern int g_anFlightTargetTally[];                          /* tally collected, per class */
extern int g_anLevelDragonsRescued[];                        /* per-g_nLevelIndex completion counters */
extern int g_anLevelEggsRecovered[];
extern short g_anSineLut[];
extern int g_anSpyroMotionVec;                               /* look-at offset */
extern int g_anSpyroWorldPos;                                /* Spyro, as a plain symbol (the loop's spy carrier) */
extern int g_anWorldPersistentFlags[];                       /* per-mode "time attack" flag */
extern Actor *g_apActiveActorList[];                   /* live actor list, 0-terminated */
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
extern Actor *g_pSparxActor;                     /* Sparx, or null when absent */
extern unsigned char *g_pLevelSampleBankEntries;             /* sound definitions, 20 bytes each */
extern unsigned char *g_pLevelSampleBankHeader;              /* sound id table */
/* Sample-bank entry pitch (u16 at +0xA of the 0x14-byte entry; full layout
 * in PlaySoundEffect.c SAMPLEENTRY). Kept as byte arithmetic on purpose: it
 * is the access form every cloned call site was matched with. */
#define SAMPLE_PITCH(id) \
  (*(unsigned short *)(g_pLevelSampleBankEntries + (id) * 20 + 0xA))
extern Actor *g_pScriptedCameraActor;
extern int *g_pSpyroRequestCameraAnchor;                     /* SPY.pCamAnchor, absolute form */
extern unsigned char **g_pTriggerRegionTable;                /* surface records, indexed by the above */
extern int RandomIntInRange(int lo, int hi);                 /* random in [lo, hi] */
extern int RandomSignedInRange(int a, int b);
extern int StepAndLerpAngle8(int ang, int facing, int a, int b);
extern int TickCountdownByFrameDelta(void *timer, int step);
extern void VectorToSVectorShr2(short *d, int *s);

/* World chunk-animation flags (byte [1] of a chunk-anim entry; each step
 * entry's low two bits are copied into it on arrival, TickWorldChunkAnimations). */
#define CHUNK_ANIM_BRANCH 0x01 /* follow the step's next_b successor, not next_a */
#define CHUNK_ANIM_HALTED 0x02 /* stopped: the tick skips the entry */
/* ModifyWorldChunkAnimFlags andMask keeping every flag but `bits`. */
#define CHUNK_ANIM_KEEP(bits) (0xFF & ~(bits))
/* andMask that clears HALTED and BRANCH: run on along next_a. */
#define CHUNK_ANIM_RESUME CHUNK_ANIM_KEEP(CHUNK_ANIM_HALTED | CHUNK_ANIM_BRANCH)
/* Current step index from a GetWorldChunkAnimState value. */
#define CHUNK_ANIM_STEP(state) (((state) >> 8) & 0xFF)

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern int CastRayWorldAndActors(int *a, int *b);
extern int ComputeStepTowardPoint(int *from, int *to, int *vel, int a);
extern int g_anSparxHealthBlock[]; /* == g_nSparxHealth (SPY.sparxHealth): 3 gold .. 1 green, 0 Sparx gone, -1 the fatal hit was taken */
extern int GetWorldChunkAnimState(int handle); /* flags | step << 8 | delay << 16 */
extern void ModifyWorldChunkAnimFlags(int handle, int a, int b);
extern int PickupGroupHasLiveActor(int stage);
extern int PlaneDistanceAbs(int *pos, void *st);
extern void SetWorldChunkAnimStep(int handle, int a, int b);
extern int WrapAddAngle8(int a, int b);
extern int g_anActorShardOffsets[];  /* shard offset vectors, 3 ints each */
extern int g_anMotionTrailSliceCounts[];       /* trail lengths */
extern signed char g_bSpyroRequestYaw; /* g_abSpyroRequestEuler[2]: requested yaw */

/* Engine helpers every actor family calls; parameter names follow the main definitions. */
extern int DispatchActorContactAtSphere(int *pos, int radius, int a, int mask, Actor *actor, int flag);
extern void GetActorAttachPoint(Actor *actor, int slot, int *pos);
extern int SteerActorAroundPoint(Actor *actor, int *tgt, int radius, int face, int deadzone, int speed, int p6, int p7, int blockA, int blockB, int p10, int p11, int flags);
extern int SteerActorTowardPoint(Actor *actor, int *target, int speed, int yawLim, int pitchLim, int radius);
extern void SVectorToVectorShl2(int *d, int *s);
extern int TrackEntityYawTowardSpyro(Actor *actor, int step, int threshold, int flag);
extern int g_anCameraPos[];

#endif /* OPEN_SPYRO_LEVEL_ACTORS_H */
