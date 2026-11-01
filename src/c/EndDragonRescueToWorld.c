#include "actor_states.h"
#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Leave the dragon-rescue / dragon-rescue overlay back into the world
   (0x8002CB6C, gamestate 0): unblock gameplay, release the actor that owned
   the pickup (clear its +0x3C hold byte and re-enable it at +0x48), reset
   Spyro, re-seed the pad-snapshot countdown and point the camera anchor at his
   world position, retarget the dynamic camera params (yaw re-biased to Spyro's
   body yaw, wrapped to 12 bits), restore the superflame timer, reset the
   camera and HUD (all five counters hidden), and re-issue the current music
   track (command 8).

   Shares its tail with EndSaveMenuToWorld. */

void EndDragonRescueToWorld(void) {
  int **cam;
  int *dyn;
  int *snap;
  int id;
  Actor *rec;
  int timer;

  id = ((DragonStatueState *)g_pDragonRescueSourceActor->state)->partner;
  g_nGamestate = GS_GAMEPLAY;
  g_nCinematicActive = 0;
  if (id != -1) {
    rec = &g_pActorListBase[id];
    rec->anim = 0;
    rec->mode = 1;
  }
  ResetSpyroEntity(1);
  cam = g_apCameraAnchorBlock;
  snap = g_anSpyroPadSnapshotBlock;
  snap[0] = 0xC;
  cam[0] = &snap[-0x7C];
  timer = g_nSpyroSuperflameTimerSave;
  dyn = g_anCameraDynamicTargetParams;
  g_nCameraCurrentMode = CAM_MODE_STAND;
  g_nCameraNextMode = CAM_MODE_STAND;
  g_pCameraTargetParams = dyn;
  g_nSpyroSuperflameTimer = timer;
  g_nCameraTargetYawBiasFromMode = g_nSpyroBodyYaw;
  ComputeCameraToTargetAngle(((char *)cam) - 0xA8);
  CopyWords(dyn, ((char *)cam) - 0x40, 0x18);
  dyn[0] = (dyn[0] + g_nCameraTargetYawBiasFromMode) & 0xFFF;
  ResetCameraStateToTarget();
  InitHudCounters(0);
  g_abHudCounterDisplayState[0] = 0;
  g_abHudCounterDisplayState[1] = 0;
  g_abHudCounterDisplayState[2] = 0;
  g_abHudCounterDisplayState[3] = 0;
  g_abHudCounterDisplayState[4] = 0;
  HandleMusicCommand(g_nCurrentMusicTrack, 8);
}
