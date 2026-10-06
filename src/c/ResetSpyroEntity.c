#include "globals.h"

extern void StopSoundVoicesByOwner(void *owner, int mask);
extern void CopyVector(int *dst, int *src);
extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void ChangeSpyroState(int state);

/* 0x8004ac24 (0x214): resets Spyro's entity record. Leaving a looping-sfx
   state stops Spyro's voices (state 7 just clears the fx alpha). With
   `full` set the whole 0x2a4-byte record is wiped, keeping only the world
   position, the persistent euler and the level-ready flag, then Spyro is
   put back into state 0; otherwise only the transient flags/timers clear. */
void ResetSpyroEntity(int full) {
  int pos[3];
  unsigned char euler[3];
  int ready;

  switch (g_nSpyroState) {
  case 11:
  case 15:
  case 32:
  case 44:
    StopSoundVoicesByOwner(g_anSpyroWorldPos, 2);
    break;
  case 7:
    g_abSpyroFxParticleRgba[3] = 0;
    break;
  }

  if (full) {
    CopyVector(pos, g_anSpyroWorldPos);
    euler[0] = g_abSpyroPersistentEuler[0];
    euler[1] = g_abSpyroPersistentEuler[1];
    euler[2] = g_abSpyroPersistentEuler[2];
    ready = g_nLevelReadyFlag;
    FillWord(g_anSpyroWorldPos, 0, 0x2A4);
    CopyVector(g_anSpyroWorldPos, pos);
    g_bSpyroOtBinBias = 4;
    g_bSpyroAnimFrame = 1;
    g_nLevelReadyFlag = ready;
    g_nSpyroGroundChunkId = -1;
    g_abSpyroPersistentEuler[0] = euler[0];
    g_abSpyroPersistentEuler[1] = euler[1];
    g_abSpyroPersistentEuler[2] = euler[2];
    g_nSpyroBodyPitch = euler[0] << 4;
    g_nSpyroBodyRoll = euler[1] << 4;
    g_nSpyroBodyYaw = euler[2] << 4;
    ChangeSpyroState(0);
    g_nSpyroFirstContactChunkId = 0xFF;
    g_bSpyroSfxVoiceMarker = 0x3F;
    g_nSpyroFlameBreathTimer = 0;
  } else {
    g_nSpyroGroundChunkId = -1;
    g_nSpyroFirstContactChunkId = 0xFF;
    *(int *)g_abSpyroFxParticleRgba = 0;
    g_dwSpyroTriggerEventFlags = 0;
    g_nSpyroSwimWaterFloorZ = 0;
    g_nSpyroFallTrackingFlag = 0;
    g_nSpyroChargeHoldFlag = 0;
    g_nSpyroGlideBankingFlag = 0;
    g_nSpyroBreathTimer = 0;
    g_nSpyroFlameBreathTimer = 0;
    g_nSpyroBodyPitch = g_abSpyroPersistentEuler[0] << 4;
    g_nSpyroBodyRoll = g_abSpyroPersistentEuler[1] << 4;
    g_nSpyroBodyYaw = g_abSpyroPersistentEuler[2] << 4;
  }
}
