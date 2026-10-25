#include "funcs.h"
#include "globals.h"
#include "spyro_state.h"
#include "vecmath.h"

/* 0x8004ac24 (0x214): resets Spyro's entity record. Leaving a looping-sfx
   state stops Spyro's voices (state 7 just clears the fx alpha). With
   `full` set the whole 0x2a4-byte record is wiped, keeping only the world
   position, the persistent euler and the level-health flag, then Spyro is
   put back into state 0; otherwise only the transient flags/timers clear. */
void ResetSpyroEntity(int full) {
  int pos[3];
  unsigned char euler[3];
  int health;

  switch (g_nSpyroState) {
  case SPYRO_ST_CHARGE:
  case SPYRO_ST_GLIDE:
  case SPYRO_ST_FLY:
  case SPYRO_ST_SUPERCHARGE:
    StopSoundVoicesByOwner(g_anSpyroWorldPos, 2);
    break;
  case SPYRO_ST_HURT_TINT:
    g_abSpyroTintRgba[3] = 0;
    break;
  }

  if (full) {
    CopyVector(pos, g_anSpyroWorldPos);
    euler[0] = g_abSpyroPersistentEuler[0];
    euler[1] = g_abSpyroPersistentEuler[1];
    euler[2] = g_abSpyroPersistentEuler[2];
    health = g_nSparxHealth;
    FillWord(g_anSpyroWorldPos, 0, 0x2A4);
    CopyVector(g_anSpyroWorldPos, pos);
    g_bSpyroOtBinBias = 4;
    g_bSpyroAnimFrame = 1;
    g_nSparxHealth = health;
    g_nSpyroGroundChunkId = -1;
    g_abSpyroPersistentEuler[0] = euler[0];
    g_abSpyroPersistentEuler[1] = euler[1];
    g_abSpyroPersistentEuler[2] = euler[2];
    g_nSpyroBodyPitch = euler[0] << 4;
    g_nSpyroBodyRoll = euler[1] << 4;
    g_nSpyroBodyYaw = euler[2] << 4;
    ChangeSpyroState(SPYRO_ST_IDLE);
    g_nSpyroFirstContactChunkId = 0xFF;
    g_bSpyroSfxVoiceMarker = 0x3F;
    g_nSpyroSuperflameTimer = 0;
  } else {
    g_nSpyroGroundChunkId = -1;
    g_nSpyroFirstContactChunkId = 0xFF;
    *(int *)g_abSpyroTintRgba = 0;
    g_dwSpyroTriggerEventFlags = 0;
    g_nSpyroHazardFloorZ = 0;
    g_nSpyroFlightMode = 0;
    g_nSpyroChargeHoldFlag = 0;
    g_nSpyroGlideBankingFlag = 0;
    g_nSpyroHazardSinkDepth = 0;
    g_nSpyroSuperflameTimer = 0;
    g_nSpyroBodyPitch = g_abSpyroPersistentEuler[0] << 4;
    g_nSpyroBodyRoll = g_abSpyroPersistentEuler[1] << 4;
    g_nSpyroBodyYaw = g_abSpyroPersistentEuler[2] << 4;
  }
}
