/* level_balloonist_cutscene.h — declarations shared by the *_BalloonistCutscene_Update.c / _Draw.c clones
 * (also included by the flight-results clones for the same names), declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_BALLOONIST_CUTSCENE_H
#define OPEN_SPYRO_LEVEL_BALLOONIST_CUTSCENE_H

extern void CopyWords(void *dst, void *src, int len);

extern void ApplyEulerRotation(int *a, int *b, int c);

extern void ComputeCameraToTargetAngle(int *a0);

extern int FindGroundHeightBelow(int *a0, int a1);

extern void PlaySoundEffect(int a0, int a1, int a2, int a3);

extern void StopAllSoundExceptMask(int a0);

extern void ClearWord(int a0);

extern int g_anCameraDynamicTargetParams[];

extern unsigned char *g_apSaveMenuAnimActorList[];

extern void (*g_pfnLevelAdvanceEmitList)(int);

extern unsigned char *g_pLevelOverlayInitRecord;

extern void (*g_pfnLevelOnSaveMenuExit)();

extern unsigned char *g_pLevelSampleBankHeader;

extern int g_anCameraPos[];

extern short g_nCameraEulerPitch;

extern int g_pCameraAnchorPos[];

extern int g_pCameraTargetParams;

extern int g_dwPadPressed[];

extern int g_nGamestate0cSubstate[];            /* [0] substate [1] phase [2] selected [3] submode [4] greeting */

extern int g_nGamestate0cTimer[];

extern int g_nGamestate0cMenuCursor[];

extern int g_nGamestate0cDialogPage[];

extern int g_nGamestate0cCamYawGoal[];

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

extern void DrawActors();

extern void RasterizeEmitList();

extern int g_pSpriteRecordWriteCursor;              /* sprite-record write cursor */

extern int g_apSpritePrimQueue;              /* sprite prim queue head */

extern int g_pOtDepthBinHead0[];            /* OT depth-bin list */

extern unsigned short g_anCosineLut[]; /* cosine LUT */

extern char *g_apBalloonistNames[];          /* per-world balloonist names */

extern char *g_apBalloonistDialogueLines[];          /* dialogue lines */

#endif /* OPEN_SPYRO_LEVEL_BALLOONIST_CUTSCENE_H */
