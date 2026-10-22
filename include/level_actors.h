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
extern int D_8006E5C4[][3];
extern Actor *D_80075758;
extern int D_80075F38[][7];                                                    /* voice table */
extern int D_80075F44[][7];                                                    /* word 3 of the voice table rows */
extern int D_800777F4;
extern Actor g_abHudIconActorRecords[];                                        /* the four HUD class icons */
extern int g_anCollisionResolvedPos;                                           /* collision contact point */
extern short g_anCosineLut[];
extern int g_anFlightTargetTally[];                                            /* tally collected, per class */
extern int g_anLevelDragonsRescued[];                                          /* per-g_nLevelIntroIndex completion counters */
extern int g_anLevelEggsRecovered[];
extern short g_anSineLut[];
extern int g_anSpyroMotionVec;                                                 /* look-at offset */
extern int g_anSpyroWorldPos;                                                  /* Spyro, as a plain symbol (the loop's spy carrier) */
extern int g_anWorldPersistentFlags[];                                         /* per-mode "time attack" flag */
extern Actor *g_apSaveMenuAnimActorList[];                                     /* live actor list, 0-terminated */
extern int g_dwPadPressed;                                                     /* pad buttons pressed; 0x40 skips the egg */
extern short g_nCameraEulerPitch;
extern int g_nCameraScriptStickRequestY[];
extern int g_nCollisionResolvedZ;                                              /* ..z */
extern int g_nSpyroWorldPosY;
extern Actor *g_pActorListBase;                                                /* level actor pool */
extern Actor *g_pActorPoolDynBase;                                             /* end of the level actor pool */
extern void (*g_pfnLevelBeginActorCutscene)(Actor *actor);
extern void (*g_pfnLevelEmitParticles)(int a, int effect, int *pos, void *c);
extern void (*g_pfnLevelFallImpactCallback)(void);                             /* out-of-time handler */
extern Actor *(*g_pfnLevelSpawnActor)(int type, Actor *parent);
extern Actor *g_pLevelOverlayInitRecord;                                       /* Sparx, or null when absent */
extern unsigned char *g_pLevelSampleBankEntries;                               /* sound definitions, 20 bytes each */
extern unsigned char *g_pLevelSampleBankHeader;                                /* sound id table */
extern Actor *g_pScriptedCameraActor;
extern int *g_pSpyroRequestCameraAnchor;                                       /* SPY.pCamAnchor, absolute form */
extern unsigned char **g_pTriggerRegionTable;                                  /* surface records, indexed by the above */
extern int RandomIntInRange(int lo, int hi);                                   /* random in [lo, hi] */
extern int RandomSignedInRange(int a, int b);
extern int StepAndLerpAngle8(int ang, int facing, int a, int b);
extern int TickCountdownByFrameDelta(void *timer, int step);
extern void VectorToSVectorShr2(short *d, int *s);

#endif /* OPEN_SPYRO_LEVEL_ACTORS_H */
