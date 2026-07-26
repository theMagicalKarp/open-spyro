#include "globals.h"

/* Leave the gem-pickup / dragon-rescue overlay back into the world
   (0x8002CB6C, gamestate 0): unblock gameplay, release the actor that owned
   the pickup (clear its +0x3C hold byte and re-enable it at +0x48), reset
   Spyro, re-seed the pad-snapshot countdown and point the camera anchor at his
   world position, retarget the dynamic camera params (yaw re-biased to Spyro's
   body yaw, wrapped to 12 bits), restore the flame-breath timer, reset the
   camera and HUD (all five counters hidden), and re-issue the current music
   track (command 8).

   Shares its tail with EndSaveMenuToWorld. */
extern void ResetSpyroEntity(int param);
extern void ComputeCameraToTargetAngle(void *params);
extern void CopyWords(void *dst, void *src, int count);
extern void ResetCameraStateToTarget(void);
extern void InitHudCounters(int mode);
extern void HandleMusicCommand(int track, int cmd);

extern int *g_apCameraAnchorBlock[]; /* alias of g_pCameraAnchorPos */
extern int
    g_anSpyroPadSnapshotBlock[]; /* alias of g_nSpyroPadSnapshotCountdown */

void EndGemPickupToWorld(void) {
  int **cam;
  int *dyn;
  int *snap;
  int id;
  unsigned char *rec;
  int timer;

  id = ((int *)(*(void **)g_pGemPickupSourceActor))[8];
  g_nGamestate = 0;
  g_nGameplayBlocked = 0;
  if (id != -1) {
    rec = (unsigned char *)g_pActorListBase + id * 0x58;
    rec[0x3C] = 0;
    rec[0x48] = 1;
  }
  ResetSpyroEntity(1);
  cam = g_apCameraAnchorBlock;
  snap = g_anSpyroPadSnapshotBlock;
  snap[0] = 0xC;
  cam[0] = &snap[-0x7C];
  timer = g_nSpyroFlameBreathTimerSave;
  dyn = g_anCameraDynamicTargetParams;
  g_nCameraCurrentMode = 0;
  g_nCameraNextMode = 0;
  g_pCameraTargetParams = dyn;
  g_nSpyroFlameBreathTimer = timer;
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
