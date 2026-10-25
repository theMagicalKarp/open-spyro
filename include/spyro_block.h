/* spyro_block.h — Spyro's state block seen as one int array.
 *
 * g_anSpyroWorldPosBlock (0x80078a58) is the incomplete-array view of the
 * Spyro globals that starts at g_anSpyroWorldPos; the matched code that holds
 * that base in one register addresses the other globals as SPY[index]. The
 * indices below are WORD offsets from 0x80078a58, each named after the global
 * that lives there. (Byte-offset views stay next to the code that uses them.) */

#ifndef OPEN_SPYRO_SPYRO_BLOCK_H
#define OPEN_SPYRO_SPYRO_BLOCK_H

#define SPY g_anSpyroWorldPosBlock

#define SPY_TRIGGER_FLAGS  0x0B /* g_dwSpyroTriggerEventFlags */
#define SPY_STATE          0x1E /* g_nSpyroState */
#define SPY_STATE_FLAGS    0x1F /* g_nSpyroStateFlags */
#define SPY_CONTACTED      0x22 /* g_nSpyroContactedActorFlag */
#define SPY_PREV_POS       0x23 /* g_anSpyroPrevWorldPos */
#define SPY_PROBE_SUSPEND  0x26 /* g_nSpyroGroundProbeSuspendFlag */
#define SPY_AIRBORNE       0x27 /* g_nSpyroAirborneFrames */
#define SPY_GROUND_SLOPE   0x2C /* g_nSpyroGroundSlope */
#define SPY_WALL_SLIDE     0x2D /* g_nSpyroWallSlideContributedFlag */
#define SPY_NUDGE          0x2E /* g_anSpyroWallSlideNudgeAccum */
#define SPY_CLIFF          0x31 /* g_nSpyroCliffEdgeHint */
#define SPY_SPEED_TARGET   0x32 /* g_nSpyroSpeedTarget */
#define SPY_PLATFORM       0x37 /* g_anSpyroSlopeGravityVec */
#define SPY_CARRY          0x3A /* g_anSpyroCarryMotionVec */
#define SPY_VELOCITY       0x3D /* g_anSpyroVelocity */
#define SPY_VELOCITY_FP6   0x40 /* g_anSpyroVelocityFp6 */
#define SPY_MOTION         0x43 /* g_anSpyroMotionVec */
#define SPY_SPEED          0x46 /* g_nSpyroMotionSpeed */
#define SPY_BODY_YAW       0x47 /* g_nSpyroBodyYaw */
#define SPY_ROLL           0x48 /* g_nSpyroBodyRoll */
#define SPY_PITCH          0x49 /* g_nSpyroBodyPitch */
#define SPY_FRAME_MOTION   0x4A /* g_nSpyroFrameMotionLength */
#define SPY_IMPULSE        0x4B /* g_anSpyroPendingImpulse */
#define SPY_BODY_YAW_STEP  0x52 /* g_nSpyroBodyYawStep */
#define SPY_LOCKOUT        0x58 /* g_nSpyroInputLockoutCountdown */
#define SPY_LEVEL_READY    0x59 /* g_nLevelReadyFlag */
#define SPY_BREATH_TIMER   0x5A /* g_nSpyroBreathTimer */
#define SPY_FIRST_POINT    0x5F /* g_anSpyroFirstContactPoint */
#define SPY_FIRST_NORMAL   0x62 /* g_anSpyroFirstContactNormal */
#define SPY_SCRIPTED_MOVE  0x65 /* g_nSpyroScriptedMoveMode */
#define SPY_COUNTDOWN      0x7C /* g_nSpyroPadSnapshotCountdown */
#define SPY_REQUEST        0x7D /* g_dwSpyroRequestMask */
#define SPY_REQUEST_TARGET 0x7E /* g_anSpyroRequestTargetPos */
#define SPY_REQUEST_STATE  0x86 /* g_nSpyroRequestState */
#define SPY_SCRIPTED_ACTOR 0x89 /* g_pScriptedCameraActor */
#define SPY_FALL_TRACKING  0x93 /* g_nSpyroFallTrackingFlag */
#define SPY_CHARGE_HOLD    0x94 /* g_nSpyroChargeHoldFlag */
#define SPY_GLIDE_BANK     0x95 /* g_nSpyroGlideBankingFlag */

/* Byte offsets into the same block, for the byte-wide globals. */
#define SPYB_EULER       0x0C /* g_abSpyroPersistentEuler (x, y, z bytes) */
#define SPYB_EULER_Z     0x0E /* g_bSpyroPersistentEulerZ */
#define SPYB_ANIM_CURRENT 0x19 /* g_bSpyroAnimCurrent */
#define SPYB_ANIM_FRAME  0x1F /* g_bSpyroAnimFrame */
#define SPYB_SUB_FRAME_TIMER 0x24 /* g_bSpyroSubFrameTimer */
#define SPYB_OT_BIN_BIAS 0x27 /* g_bSpyroOtBinBias */
#define SPYB_BODY_MTX    0x34 /* g_anSpyroBodyMtx */
#define SPYB_VOICE_MARKER 0x2A0 /* g_bSpyroSfxVoiceMarker */

#endif /* OPEN_SPYRO_SPYRO_BLOCK_H */
