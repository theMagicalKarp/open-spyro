#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "model.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* `mesh` is Spyro's model plus anim * 4, so ANIM_SLOT reads that anim's
   header pointer (Model.m_Animations[anim]); a frame record is the header plus
   frame * 4, whose AnimationHeader.m_FrameWords word is the frame's sound key
   and its top byte the sfx id (0xFF none).  The header pointer is re-read per
   use (volatile) — the original never keeps it live across the sfx-id test. */
/* ...and from the held previous-key base (g_adwSpyroAnimSfxPrevKeyBlock,
   0x80078cbc) back to g_anSpyroWorldPos (the sound owner) and on to
   g_bSpyroSfxVoiceMarker. */
#define SFX_KEY_TO_SPYRO_POS 0x264
#define SFX_KEY_TO_VOICE_MARKER 0x3C

#define ANIM_SLOT(mesh) (*(volatile int *)((mesh) + MODEL_OFFSET(m_Animations)))
#define FRAMEREC(mesh, frame) ((frame) + ANIM_SLOT(mesh))
#define FRAME_KEY(rec)                                                         \
  (*(unsigned int *)((rec) + ANIM_HEADER_OFFSET(m_FrameWords)))
#define FRAME_SFX(rec)                                                         \
  (*(unsigned char *)((rec) + ANIM_HEADER_OFFSET(m_FrameWords) + 3))

/* 0x8004a200 (1516 bytes) — the in-level gameplay tick: snapshot the pad,
   integrate a frame of motion, run the state behaviour + the per-substep
   anim machine, compose the body and composed-body matrices, then police the
   death plane (respawn / hard-fall state change / scripted death camera).  Ends
   by firing the two anim-frame sounds (primary anim + layer-1 overlay):
   each layer's frame sound plays when its frame word differs from last
   frame's (g_dwSpyroAnimSfxPrevKey / g_dwSpyroLayer1SfxPrevKey), so a sound
   fires once on entering its keyframe. */
void TickSpyroGameplayFrame(void) {
  unsigned int key;
  unsigned int key2;
  int *countdown;
  unsigned char *euler;
  int *bodyMtx;
  int *composedMtx;
  int *pos;
  int *fallFrames;
  int *camRecord;
  unsigned char *animBlock;
  unsigned int *prevKey;
  int mesh;
  int rec;
  int ptr;
  int step;
  int frame;
  int scratch[8]; /* frame slack ahead of sfx[] (A127) */
  int sfx[2];     /* per-slot sfx id, kept in the frame across the sfx calls */
  int pad[2];     /* frame slack */

  if (((g_dwSpyroRequestMask & SPYRO_RQ_FREEZE_INPUT) != 0) ||
      (g_nSpyroScriptedMoveMode != 0)) {
    if (g_nSpyroInputBlankCountdown < 2) {
      g_nSpyroInputBlankCountdown = 2;
    }
  }
  countdown = g_anSpyroInputBlankBlock;
  g_bPadInputStashed = 0;
  if (countdown[0] != 0) {
    MovePadInputState(&g_dwPadPressed, &g_dwPadStash);
    countdown[0] -= 1;
  }
  AdvanceSpyroFrameMotion();
  if (g_nSpyroState == SPYRO_ST_HAZARD) {
    if (g_nSpyroHazardSinkDepth <= 0) {
      g_nSpyroHazardSinkDepth = 1;
    }
  } else {
    g_nSpyroHazardSinkDepth = 0;
  }
  if (((g_dwSpyroRequestMask & SPYRO_RQ_FORCE_STATE) == 0) &&
      (g_nSpyroScriptedMoveMode == 0)) {
    UpdateSpyroStateBehavior();
  }
  step = 0;
  while (step < g_nFrameStep) {
    step += 1;
    TickSpyroAnimStateMachine();
    TickSpyroFlameAttack();
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2GateTimer();
    TickSpyroAnimLayer2();
  }
  euler = g_abSpyroPersistentEulerBlock;
  bodyMtx = (int *)(euler + EULER_TO_BODY_MTX);
  euler[0] = ANGLE12_TO_8(g_nSpyroBodyPitch);
  g_abSpyroPersistentEuler[1] = g_nSpyroBodyRoll >> 4;
  g_abSpyroPersistentEuler[2] = ANGLE12_TO_8(g_nSpyroBodyYaw);
  ApplyEulerRotation(euler, bodyMtx, 0);
  composedMtx = (int *)(euler + EULER_TO_COMPOSED_MTX);
  ApplyEulerRotation(euler + 4, composedMtx, 0);
  MulMatrix0(bodyMtx, composedMtx, composedMtx);
  SampleSpyroShadowRingHeights(1);
  TickSpyroAttackEffects(g_nFrameStep);
  g_dwSpyroTriggerEventFlags = 0;
  if (g_nSparxHealth < SPARX_GONE) {
    switch (g_nSpyroState) {
    case SPYRO_ST_HAZARD:
      if (g_nSpyroHazardSinkDepth == SPYRO_HAZARD_SINK_FATAL) {
        if (g_nFlightLevelActive != 0) {
          g_pfnLevelEndFlightRun();
          g_pfnLevelUpdateActors();
        } else {
          TriggerRespawnOrGameOver();
        }
      }
      break;
    case SPYRO_ST_DEATH:
      if (g_nSpyroStateTimer >= SPYRO_DEATH_TICKS) {
        TriggerRespawnOrGameOver();
      }
      break;
    case SPYRO_ST_DEATH_PINNED:
      if (g_nSpyroStateTimer >= SPYRO_DEATH_PINNED_TICKS) {
        TriggerRespawnOrGameOver();
      }
      break;
    }
  } else if ((g_nSpyroWorldPosZ < WORLD_MIN_COORD) ||
             ((g_nSpyroState == SPYRO_ST_FALL) &&
              (g_nSpyroSubstate >= SPYRO_DEATH_FALL_FRAMES))) {
    TriggerRespawnOrGameOver();
  } else {
    pos = g_anSpyroWorldPosZBlock;
    if (pos[0] < g_anLevelDeathPlaneZTable[g_nLevelIndex]) {
      if ((g_nSpyroState != SPYRO_ST_FALL) &&
          (g_nSpyroState != SPYRO_ST_GLIDE_DROP)) {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if ((g_anSpyroWorldPos[0] < SPYRO_WORLD_EDGE_XY) ||
               (g_nSpyroWorldPosY < SPYRO_WORLD_EDGE_XY)) {
      if ((g_nSpyroState != SPYRO_ST_FALL) &&
          (g_nSpyroState != SPYRO_ST_GLIDE_DROP)) {
        ZeroVector(pos + (SPY_MOTION - SPY_POS_Z));
        ZeroVector(pos + (SPY_VELOCITY - SPY_POS_Z));
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else {
      goto done;
    }
    fallFrames = g_anSpyroSubstateBlock;
    if (fallFrames[0] == 0) {
      camRecord = g_anSpyroDeathCamRecordBlock;
      CopyVector(camRecord, g_anCameraPos);
      ActivateScriptedCameraView(SCRIPTED_CAM_VIEW_AT_POS(camRecord));
    }
    fallFrames[0] += g_nFrameStep;
  }
done:
  animBlock = g_abSpyroAnimPrevBlock;
  mesh = animBlock[0] * 4 + (int)g_apActorMeshTable;
  frame = g_bSpyroFramePrev * 4;
  sfx[0] = FRAME_SFX(FRAMEREC(mesh, frame));
  if (sfx[0] != 0xFF) {
    ptr = ANIM_SLOT(mesh);
    rec = frame + ptr;
    if (g_dwSpyroAnimSfxPrevKey != FRAME_KEY(rec)) {
      PlaySoundEffect(sfx[0], (Actor *)SPY_FROM_ANIM_PREV(animBlock, 0),
                      SFX_MODE_FLAT,
                      SPY_FROM_ANIM_PREV(animBlock, SPYB_VOICE_MARKER));
    }
  }
  key = FRAME_KEY(FRAMEREC(g_bSpyroAnimPrev * 4 + (int)g_apActorMeshTable,
                           g_bSpyroFramePrev * 4));
  prevKey = g_adwSpyroAnimSfxPrevKeyBlock;
  prevKey[0] = key;
  sfx[1] =
      FRAME_SFX(FRAMEREC(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable,
                         g_bSpyroAnimLayer1FramePrev * 4));
  if ((sfx[1] != 0xFF) && (sfx[0] != sfx[1])) {
    ptr = ANIM_SLOT(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable);
    rec = g_bSpyroAnimLayer1FramePrev * 4 + ptr;
    if (g_dwSpyroLayer1SfxPrevKey != FRAME_KEY(rec)) {
      PlaySoundEffect(
          sfx[1], (Actor *)((unsigned char *)prevKey - SFX_KEY_TO_SPYRO_POS),
          SFX_MODE_FLAT, (unsigned char *)prevKey + SFX_KEY_TO_VOICE_MARKER);
    }
  }
  key2 =
      FRAME_KEY(FRAMEREC(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable,
                         g_bSpyroAnimLayer1FramePrev * 4));
  g_dwSpyroRequestMask &= ~SPYRO_RQ_PENDING;
  g_dwSpyroLayer1SfxPrevKey = key2;
  if (g_bPadInputStashed != 0) {
    MovePadInputState(&g_dwPadStash, &g_dwPadPressed);
  }
}
