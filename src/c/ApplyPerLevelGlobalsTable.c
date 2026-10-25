#include "globals.h"

#include "level_entry_points.h"
#include "main_shared.h"

void ApplyPerLevelGlobalsTable(void) {
  switch (g_nActiveLevelId) {
  case 0:
    g_pDrawBufA = LevelId0_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId0_SpawnActor;
    g_pfnLevelUpdateActors = LevelId0_UpdateActors;
    g_pfnLevelEmitParticles = LevelId0_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId0_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = LevelId0_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = LevelId0_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = LevelId0_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = LevelId0_BalloonistCutscene_Draw;
    break;
  case 1:
    g_pDrawBufA = LevelId1_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId1_SpawnActor;
    g_pfnLevelUpdateActors = LevelId1_UpdateActors;
    g_pfnLevelEmitParticles = LevelId1_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId1_AdvanceEmitList;
    break;
  case 2:
    g_pDrawBufA = LevelId2_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId2_SpawnActor;
    g_pfnLevelUpdateActors = LevelId2_UpdateActors;
    g_pfnLevelEmitParticles = LevelId2_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId2_AdvanceEmitList;
    break;
  case 3:
    g_pDrawBufA = LevelId3_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId3_SpawnActor;
    g_pfnLevelUpdateActors = LevelId3_UpdateActors;
    g_pfnLevelEmitParticles = LevelId3_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId3_AdvanceEmitList;
    break;
  case 4:
    g_pDrawBufA = LevelId4_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId4_SpawnActor;
    g_pfnLevelUpdateActors = LevelId4_UpdateActors;
    g_pfnLevelEmitParticles = LevelId4_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId4_AdvanceEmitList;
    break;
  case 7:
    g_pDrawBufA = LevelId7_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId7_SpawnActor;
    g_pfnLevelUpdateActors = LevelId7_UpdateActors;
    g_pfnLevelEmitParticles = LevelId7_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId7_AdvanceEmitList;
    break;
  case 9:
    g_pDrawBufA = LevelId9_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId9_SpawnActor;
    g_pfnLevelUpdateActors = LevelId9_UpdateActors;
    g_pfnLevelEmitParticles = LevelId9_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId9_AdvanceEmitList;
    break;
  case 10:
    g_pDrawBufA = Level0_OverlayEnd;
    g_pfnLevelSpawnActor = Level0_SpawnActor;
    g_pfnLevelUpdateActors = Level0_UpdateActors;
    g_pfnLevelEmitParticles = Level0_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level0_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level0_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level0_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level0_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level0_BalloonistCutscene_Draw;
    break;
  case 11:
    g_pDrawBufA = Level1_OverlayEnd;
    g_pfnLevelSpawnActor = Level1_SpawnActor;
    g_pfnLevelUpdateActors = Level1_UpdateActors;
    g_pfnLevelEmitParticles = Level1_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level1_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level1_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level1_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level1_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level1_BalloonistCutscene_Draw;
    break;
  case 12:
    g_pDrawBufA = Level2_OverlayEnd;
    g_pfnLevelSpawnActor = Level2_SpawnActor;
    g_pfnLevelUpdateActors = Level2_UpdateActors;
    g_pfnLevelEmitParticles = Level2_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level2_AdvanceEmitList;
    break;
  case 13:
    g_pDrawBufA = Level3_OverlayEnd;
    g_pfnLevelSpawnActor = Level3_SpawnActor;
    g_pfnLevelUpdateActors = Level3_UpdateActors;
    g_pfnLevelEmitParticles = Level3_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level3_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level3_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level3_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level3_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level3_BalloonistCutscene_Draw;
    break;
  case 14:
    g_pDrawBufA = Level4_OverlayEnd;
    g_pfnLevelSpawnActor = Level4_SpawnActor;
    g_pfnLevelUpdateActors = Level4_UpdateActors;
    g_pfnLevelEmitParticles = Level4_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level4_AdvanceEmitList;
    break;
  case 15:
    g_pDrawBufA = Level5_OverlayEnd;
    g_pfnLevelSpawnActor = Level5_SpawnActor;
    g_pfnLevelUpdateActors = Level5_UpdateActors;
    g_pfnLevelEmitParticles = Level5_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level5_AdvanceEmitList;
    g_pfnLevelFallImpactCallback = Level5_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level5_HandleFlightResults;
    g_pfnGamestate7Handler = Level5_FlightResultsMenu_Update;
    g_pfnGamestate7DrawHook = Level5_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level5_DrawFlightRaceTime;
    break;
  case 20:
    g_pDrawBufA = Level6_OverlayEnd;
    g_pfnLevelSpawnActor = Level6_SpawnActor;
    g_pfnLevelUpdateActors = Level6_UpdateActors;
    g_pfnLevelEmitParticles = Level6_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level6_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level6_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level6_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level6_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level6_BalloonistCutscene_Draw;
    break;
  case 21:
    g_pDrawBufA = Level7_OverlayEnd;
    g_pfnLevelSpawnActor = Level7_SpawnActor;
    g_pfnLevelUpdateActors = Level7_UpdateActors;
    g_pfnLevelEmitParticles = Level7_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level7_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level7_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level7_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level7_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level7_BalloonistCutscene_Draw;
    break;
  case 22:
    g_pDrawBufA = Level8_OverlayEnd;
    g_pfnLevelSpawnActor = Level8_SpawnActor;
    g_pfnLevelUpdateActors = Level8_UpdateActors;
    g_pfnLevelEmitParticles = Level8_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level8_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level8_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level8_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level8_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level8_BalloonistCutscene_Draw;
    break;
  case 23:
    g_pDrawBufA = Level9_OverlayEnd;
    g_pfnLevelSpawnActor = Level9_SpawnActor;
    g_pfnLevelUpdateActors = Level9_UpdateActors;
    g_pfnLevelEmitParticles = Level9_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level9_AdvanceEmitList;
    break;
  case 24:
    g_pDrawBufA = Level10_OverlayEnd;
    g_pfnLevelSpawnActor = Level10_SpawnActor;
    g_pfnLevelUpdateActors = Level10_UpdateActors;
    g_pfnLevelEmitParticles = Level10_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level10_AdvanceEmitList;
    break;
  case 25:
    g_pDrawBufA = Level11_OverlayEnd;
    g_pfnLevelSpawnActor = Level11_SpawnActor;
    g_pfnLevelUpdateActors = Level11_UpdateActors;
    g_pfnLevelEmitParticles = Level11_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level11_AdvanceEmitList;
    g_pfnLevelFallImpactCallback = Level11_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level11_HandleFlightResults;
    g_pfnGamestate7Handler = Level11_FlightResultsMenu_Update;
    g_pfnGamestate7DrawHook = Level11_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level11_DrawFlightRaceTime;
    break;
  case 30:
    g_pDrawBufA = Level12_OverlayEnd;
    g_pfnLevelSpawnActor = Level12_SpawnActor;
    g_pfnLevelUpdateActors = Level12_UpdateActors;
    g_pfnLevelEmitParticles = Level12_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level12_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level12_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level12_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level12_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level12_BalloonistCutscene_Draw;
    break;
  case 31:
    g_pDrawBufA = Level13_OverlayEnd;
    g_pfnLevelSpawnActor = Level13_SpawnActor;
    g_pfnLevelUpdateActors = Level13_UpdateActors;
    g_pfnLevelEmitParticles = Level13_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level13_AdvanceEmitList;
    break;
  case 32:
    g_pDrawBufA = Level14_OverlayEnd;
    g_pfnLevelSpawnActor = Level14_SpawnActor;
    g_pfnLevelUpdateActors = Level14_UpdateActors;
    g_pfnLevelEmitParticles = Level14_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level14_AdvanceEmitList;
    break;
  case 33:
    g_pDrawBufA = Level15_OverlayEnd;
    g_pfnLevelSpawnActor = Level15_SpawnActor;
    g_pfnLevelUpdateActors = Level15_UpdateActors;
    g_pfnLevelEmitParticles = Level15_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level15_AdvanceEmitList;
    break;
  case 34:
    g_pDrawBufA = Level16_OverlayEnd;
    g_pfnLevelSpawnActor = Level16_SpawnActor;
    g_pfnLevelUpdateActors = Level16_UpdateActors;
    g_pfnLevelEmitParticles = Level16_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level16_AdvanceEmitList;
    break;
  case 35:
    g_pDrawBufA = Level17_OverlayEnd;
    g_pfnLevelSpawnActor = Level17_SpawnActor;
    g_pfnLevelUpdateActors = Level17_UpdateActors;
    g_pfnLevelEmitParticles = Level17_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level17_AdvanceEmitList;
    g_pfnLevelFallImpactCallback = Level17_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level17_HandleFlightResults;
    g_pfnGamestate7Handler = Level17_FlightResultsMenu_Update;
    g_pfnGamestate7DrawHook = Level17_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level17_DrawFlightRaceTime;
    break;
  case 40:
    g_pDrawBufA = Level18_OverlayEnd;
    g_pfnLevelSpawnActor = Level18_SpawnActor;
    g_pfnLevelUpdateActors = Level18_UpdateActors;
    g_pfnLevelEmitParticles = Level18_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level18_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level18_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level18_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level18_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level18_BalloonistCutscene_Draw;
    break;
  case 41:
    g_pDrawBufA = Level19_OverlayEnd;
    g_pfnLevelSpawnActor = Level19_SpawnActor;
    g_pfnLevelUpdateActors = Level19_UpdateActors;
    g_pfnLevelEmitParticles = Level19_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level19_AdvanceEmitList;
    break;
  case 42:
    g_pDrawBufA = Level20_OverlayEnd;
    g_pfnLevelSpawnActor = Level20_SpawnActor;
    g_pfnLevelUpdateActors = Level20_UpdateActors;
    g_pfnLevelEmitParticles = Level20_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level20_AdvanceEmitList;
    break;
  case 43:
    g_pDrawBufA = Level21_OverlayEnd;
    g_pfnLevelSpawnActor = Level21_SpawnActor;
    g_pfnLevelUpdateActors = Level21_UpdateActors;
    g_pfnLevelEmitParticles = Level21_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level21_AdvanceEmitList;
    break;
  case 44:
    g_pDrawBufA = Level22_OverlayEnd;
    g_pfnLevelSpawnActor = Level22_SpawnActor;
    g_pfnLevelUpdateActors = Level22_UpdateActors;
    g_pfnLevelEmitParticles = Level22_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level22_AdvanceEmitList;
    break;
  case 45:
    g_pDrawBufA = Level23_OverlayEnd;
    g_pfnLevelSpawnActor = Level23_SpawnActor;
    g_pfnLevelUpdateActors = Level23_UpdateActors;
    g_pfnLevelEmitParticles = Level23_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level23_AdvanceEmitList;
    g_pfnLevelFallImpactCallback = Level23_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level23_HandleFlightResults;
    g_pfnGamestate7Handler = Level23_FlightResultsMenu_Update;
    g_pfnGamestate7DrawHook = Level23_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level23_DrawFlightRaceTime;
    break;
  case 50:
    g_pDrawBufA = Level24_OverlayEnd;
    g_pfnLevelSpawnActor = Level24_SpawnActor;
    g_pfnLevelUpdateActors = Level24_UpdateActors;
    g_pfnLevelEmitParticles = Level24_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level24_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level24_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level24_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level24_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level24_BalloonistCutscene_Draw;
    break;
  case 51:
    g_pDrawBufA = Level25_OverlayEnd;
    g_pfnLevelSpawnActor = Level25_SpawnActor;
    g_pfnLevelUpdateActors = Level25_UpdateActors;
    g_pfnLevelEmitParticles = Level25_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level25_AdvanceEmitList;
    break;
  case 52:
    g_pDrawBufA = Level26_OverlayEnd;
    g_pfnLevelSpawnActor = Level26_SpawnActor;
    g_pfnLevelUpdateActors = Level26_UpdateActors;
    g_pfnLevelEmitParticles = Level26_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level26_AdvanceEmitList;
    break;
  case 53:
    g_pDrawBufA = Level27_OverlayEnd;
    g_pfnLevelSpawnActor = Level27_SpawnActor;
    g_pfnLevelUpdateActors = Level27_UpdateActors;
    g_pfnLevelEmitParticles = Level27_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level27_AdvanceEmitList;
    break;
  case 54:
    g_pDrawBufA = Level28_OverlayEnd;
    g_pfnLevelSpawnActor = Level28_SpawnActor;
    g_pfnLevelUpdateActors = Level28_UpdateActors;
    g_pfnLevelEmitParticles = Level28_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level28_AdvanceEmitList;
    break;
  case 55:
    g_pDrawBufA = Level29_OverlayEnd;
    g_pfnLevelSpawnActor = Level29_SpawnActor;
    g_pfnLevelUpdateActors = Level29_UpdateActors;
    g_pfnLevelEmitParticles = Level29_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level29_AdvanceEmitList;
    g_pfnLevelFallImpactCallback = Level29_FlightEndHook;
    g_pfnLevelHandleFlightResults = Level29_HandleFlightResults;
    g_pfnGamestate7Handler = Level29_FlightResultsMenu_Update;
    g_pfnGamestate7DrawHook = Level29_FlightResults_Draw;
    g_pfnFlightTimeFormatter = Level29_DrawFlightRaceTime;
    break;
  case 60:
    g_pDrawBufA = Level30_OverlayEnd;
    g_pfnLevelSpawnActor = Level30_SpawnActor;
    g_pfnLevelUpdateActors = Level30_UpdateActors;
    g_pfnLevelEmitParticles = Level30_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level30_AdvanceEmitList;
    g_pfnLevelBeginActorCutscene = Level30_BeginActorCutscene;
    g_pfnLevelOnSaveMenuExit = Level30_OnSaveMenuExit;
    g_pfnActorCutsceneSubstateUpdate = Level30_BalloonistCutscene_Update;
    g_pfnActorCutsceneSubstateDraw = Level30_BalloonistCutscene_Draw;
    break;
  case 61:
    g_pDrawBufA = Level31_OverlayEnd;
    g_pfnLevelSpawnActor = Level31_SpawnActor;
    g_pfnLevelUpdateActors = Level31_UpdateActors;
    g_pfnLevelEmitParticles = Level31_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level31_AdvanceEmitList;
    break;
  case 62:
    g_pDrawBufA = Level32_OverlayEnd;
    g_pfnLevelSpawnActor = Level32_SpawnActor;
    g_pfnLevelUpdateActors = Level32_UpdateActors;
    g_pfnLevelEmitParticles = Level32_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level32_AdvanceEmitList;
    break;
  case 63:
    g_pDrawBufA = Level33_OverlayEnd;
    g_pfnLevelSpawnActor = Level33_SpawnActor;
    g_pfnLevelUpdateActors = Level33_UpdateActors;
    g_pfnLevelEmitParticles = Level33_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level33_AdvanceEmitList;
    break;
  case 64:
    g_pDrawBufA = Level34_OverlayEnd;
    g_pfnLevelSpawnActor = Level34_SpawnActor;
    g_pfnLevelUpdateActors = Level34_UpdateActors;
    g_pfnLevelEmitParticles = Level34_EmitParticles;
    g_pfnLevelAdvanceEmitList = Level34_AdvanceEmitList;
    break;
  case 99:
    g_pDrawBufA = LevelId99_OverlayEnd;
    g_pfnLevelSpawnActor = LevelId99_SpawnActor;
    g_pfnLevelUpdateActors = LevelId99_UpdateActors;
    g_pfnLevelEmitParticles = LevelId99_EmitParticles;
    g_pfnLevelAdvanceEmitList = LevelId99_AdvanceEmitList;
    break;
  }
  g_pDrawBufB = g_pDrawBufA;
}
