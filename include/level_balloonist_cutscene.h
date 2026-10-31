/* level_balloonist_cutscene.h — declarations shared by the *_BalloonistCutscene_Update.c / _Draw.c clones
 * (also included by the flight-results clones for the same names), declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_BALLOONIST_CUTSCENE_H
#define OPEN_SPYRO_LEVEL_BALLOONIST_CUTSCENE_H

#include "actor_type.h"
#include "actor_states.h"

extern void CopyWords(void *dst, void *src, int len);

extern void ApplyEulerRotation(int *euler, int *mtx, int base);

extern void ComputeCameraToTargetAngle(int *a0);

extern int FindGroundHeightBelow(int *a0, int a1);

extern void PlaySoundEffect(int a0, int a1, int a2, int a3);

extern void StopAllSoundExceptMask(int a0);

extern void ClearWord(int a0);

extern int g_anCameraDynamicTargetParams[];

extern Actor *g_apActiveActorList[];

extern void (*g_pfnLevelAdvanceEmitList)(int);

extern Actor *g_pSparxActor;

extern void (*g_pfnLevelOnSaveMenuExit)();

extern unsigned char *g_pLevelSampleBankHeader;

extern int g_anCameraPos[];

extern short g_nCameraEulerPitch;

extern int g_pCameraAnchorPos[];

extern int g_pCameraTargetParams;

extern int g_dwPadPressed[];

extern int g_nActorCutsceneSubstate[];            /* [0] substate [1] phase [2] selected [3] submode [4] greeting */

extern int g_nActorCutsceneTimer[];

extern int g_nActorCutsceneMenuCursor[];

extern int g_nActorCutsceneDialogPage[];

extern int g_nActorCutsceneCamYawGoal[];

extern int g_pRenderScratchRegionBase;

extern int g_pWorldRenderMeshChunkArray;

extern int g_anSpyroWorldPos[];

extern int g_nSpyroWorldPosY;

extern unsigned char g_abSpyroPersistentEuler[];

extern unsigned char g_bSpyroPersistentEulerZ;

extern unsigned char g_bSpyroOtBinBias;

extern void RasterizeSpritePrimQueue();

extern void BuildRenderEntityLists();

extern void ComposeFrameScene();

extern void SetupFrameOT();

extern void DrawPortalsAndStaticActors();

extern void RasterizeEmitList();

/* The sprite-record write cursor, declared int here: the balloonist Draw
 * clones walk it with int cursors whose loop tests compare signed (as
 * Actor * they would compare unsigned). */
extern int g_pSpriteRecordWriteCursor;

extern int g_apSpritePrimQueue;              /* sprite prim queue head */

extern int g_pOtDepthBinHead0[];            /* OT depth-bin list */

extern unsigned short g_anCosineLut[]; /* cosine LUT */

extern char *g_apBalloonistNames[];          /* per-world balloonist names */

extern char *g_apBalloonistDialogueLines[];          /* dialogue lines */

/* Declared identically (by type) in two or more including files; hoisted here. */
extern unsigned int g_dwHudBobSinePhase[];

/* The balloonist actor record (Update reads its fields; Draw links it into
 * the OT as the pending node). */
extern Actor *g_pActorCutsceneRecord;


/* g_nActorCutsceneSpyroToX (From + 12), stored through a char * off the From
 * base: not in-struct, so sched keeps the following load below it (lever L2
 * in the Update clones), and the address shares From's lui. The plain
 * scalar store costs an extra lui. */
#define SPYRO_TO_X_VIA_FROM (*(int *)((char *)g_anActorCutsceneSpyroFrom + 12))

#endif /* OPEN_SPYRO_LEVEL_BALLOONIST_CUTSCENE_H */
