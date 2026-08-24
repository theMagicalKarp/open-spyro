#include "globals.h"

/* 0x8002d02c, 324 bytes — leave the save/load menu back into the world
   (gamestate 0): unblock gameplay, rebuild the light constants, zero Spyro's
   swing-euler target and reset him, re-seed the swing-euler defaults and the
   pad-snapshot countdown, point the camera anchor 0x1A4 below the swing-target
   block, retarget the dynamic camera params (yaw re-biased to Spyro's body yaw
   and wrapped to 12 bits), restore the flame-breath timer, reset the camera and
   the HUD (all five counters hidden), and re-issue the current music track. */

extern void InitLightVectorConstants(void);
extern void ZeroVector(int *v);
extern void ResetSpyroEntity(int mode);
extern void ComputeCameraToTargetAngle(void *params);
extern void CopyWords(void *dst, void *src, int byte_count);
extern void ResetCameraStateToTarget(void);
extern void InitHudCounters(int mode);
extern void HandleMusicCommand(int track, int cmd);

extern int *g_apCameraAnchorBlock[];          /* alias of g_pCameraAnchorPos */
extern int g_anSpyroSwingTargetBlock[];       /* alias of
                                                 g_nSpyroSwingEulerPitchTarget */
extern int g_anSpyroMoveTargetYawBlock[];     /* +0x50 == g_nSpyroBodyYaw */
extern int g_nSpyroSwingEulerPrev1;           /* g_anSpyroSwingEulerPrev[1] */
extern unsigned char g_bSpyroSwingByteEuler1; /* g_abSpyroSwingByteEuler[1] */

void EndSaveMenuToWorld(void) {
  int *dyn;

  g_nGamestate = 0;
  g_nGameplayBlocked = 0;
  InitLightVectorConstants();
  ZeroVector(g_anSpyroSwingTargetBlock);
  ResetSpyroEntity(1);

  g_nSpyroSwingEulerPrev1 = 0x100;
  g_bSpyroSwingByteEuler1 = 0x10;
  g_nSpyroPadSnapshotCountdown = 8;
  g_apCameraAnchorBlock[0] = &g_anSpyroSwingTargetBlock[-105];

  dyn = g_anCameraDynamicTargetParams;
  g_nCameraCurrentMode = 0;
  g_nCameraNextMode = 0;
  g_pCameraTargetParams = dyn;
  g_nSpyroFlameBreathTimer = g_nSpyroFlameBreathTimerSave;
  g_nCameraTargetYawBiasFromMode = g_anSpyroMoveTargetYawBlock[0x14];

  ComputeCameraToTargetAngle((char *)g_apCameraAnchorBlock - 0xA8);
  CopyWords(dyn, (char *)g_apCameraAnchorBlock - 0x40, 0x18);
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

/* The yaw read goes through the g_anSpyroMoveTargetYawBlock alias (+0x50 ==
 * g_nSpyroBodyYaw, same reloc, identical bytes) and NOT through the scalar
 * symbol, because it has to be an ARRAY_REF. That is what orders it after the
 * camera-anchor store. gcc 2.7.2's sched.c calls true_dependence, whose
 * heuristic drops the dependence when one MEM is in a struct with a varying
 * address and the other is neither: the anchor store is `(mem/s (reg s1))`
 * (MEM_IN_STRUCT_P, register address) and a scalar-symbol read is neither, so
 * the pair is declared independent and the load floats above the store. An
 * ARRAY_REF sets MEM_IN_STRUCT_P on the load, the exclusion no longer applies,
 * and the load is pinned below the store exactly as the original has it.
 * Volatile on both sides (A208) also pins it, but it then pins the flame-timer
 * load too and costs the ComputeCameraToTargetAngle delay slot (+1 insn). */
