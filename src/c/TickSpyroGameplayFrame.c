#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "model.h"
#include "spyro_state.h"
#include "vecmath.h"

/* `mesh` is Spyro's model plus anim * 4, so ANIM_SLOT reads that anim's
   header pointer (Model.m_Animations[anim]); a frame record is the header plus
   frame * 4, whose AnimationHeader.m_FrameWords word is the footstep sfx key
   and its top byte the sfx id.  The header pointer is re-read per use
   (volatile) — the original never keeps it live across the sfx-id test. */
/* Byte distances from the held euler base (g_abSpyroPersistentEuler,
   0x80078a64) to the two matrices it builds: g_anSpyroBodyMtx (0x80078a8c)
   and g_anSpyroComposedBodyMtx (0x80078c20). */
#define EULER_TO_BODY_MTX 0x28
#define EULER_TO_COMPOSED_MTX 0x1BC

/* ...and from the held footstep-key base (g_adwSpyroFootstepSfxBlock,
   0x80078cbc) back to g_anSpyroWorldPos (the sound owner) and on to
   g_bSpyroSfxVoiceMarker. */
#define FOOTSTEP_TO_SPYRO_POS 0x264
#define FOOTSTEP_TO_VOICE_MARKER 0x3C

#define ANIM_SLOT(mesh) (*(volatile int *)((mesh) + MODEL_OFFSET(m_Animations)))
#define FRAMEREC(mesh, frame) ((frame) + ANIM_SLOT(mesh))
#define FRAME_KEY(rec)                                                         \
  (*(unsigned int *)((rec) + ANIM_HEADER_OFFSET(m_FrameWords)))
#define FRAME_SFX(rec)                                                         \
  (*(unsigned char *)((rec) + ANIM_HEADER_OFFSET(m_FrameWords) + 3))

/* 0x8004a200 (1516 bytes) — the in-level gameplay tick: snapshot the pad,
   integrate a frame of motion, run the state behaviour + the per-substep
   anim machine, compose the body/horn matrices, then police the death plane
   (respawn / hard-fall state change / scripted death camera).  Ends by firing
   the two footstep sound-effect slots (primary anim + layer-1 overlay) when
   their anim record's sfx byte changes. */
void TickSpyroGameplayFrame(void) {
  unsigned int key;
  unsigned int key2;
  int *countdown;
  unsigned char *euler;
  int *bodyMtx;
  int *composedMtx;
  int *pos;
  int *stateFlags;
  int *camRecord;
  unsigned char *animBlock;
  unsigned int *footstep;
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
    if (g_nSpyroPadSnapshotCountdown < 2) {
      g_nSpyroPadSnapshotCountdown = 2;
    }
  }
  countdown = g_anSpyroPadSnapshotBlock;
  g_bSpyroPadSnapshotReverseFlag = 0;
  if (countdown[0] != 0) {
    SnapshotPadInputState(&g_dwPadPressed, &g_dwPad2Buttons);
    countdown[0] -= 1;
  }
  AdvanceSpyroFrameMotion();
  if (g_nSpyroState == SPYRO_ST_HAZARD) {
    if (g_nSpyroBreathTimer <= 0) {
      g_nSpyroBreathTimer = 1;
    }
  } else {
    g_nSpyroBreathTimer = 0;
  }
  if (((g_dwSpyroRequestMask & SPYRO_RQ_FORCE_STATE) == 0) &&
      (g_nSpyroScriptedMoveMode == 0)) {
    UpdateSpyroStateBehavior();
  }
  step = 0;
  while (step < g_nFrameStep) {
    step += 1;
    TickSpyroAnimStateMachine();
    TickSpyroHornStrikeAttack();
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2GateTimer();
    TickSpyroAnimLayer2();
  }
  euler = g_abSpyroPersistentEulerBlock;
  bodyMtx = (int *)(euler + EULER_TO_BODY_MTX);
  euler[0] = g_nSpyroBodyPitch >> 4;
  g_abSpyroPersistentEuler[1] = g_nSpyroBodyRoll >> 4;
  g_abSpyroPersistentEuler[2] = g_nSpyroBodyYaw >> 4;
  ApplyEulerRotation(euler, bodyMtx, 0);
  composedMtx = (int *)(euler + EULER_TO_COMPOSED_MTX);
  ApplyEulerRotation(euler + 4, composedMtx, 0);
  MulMatrix0(bodyMtx, composedMtx, composedMtx);
  SampleSpyroShadowRingHeights(1);
  TickSpyroAttackEffects(g_nFrameStep);
  g_dwSpyroTriggerEventFlags = 0;
  if (g_nSparxHealth < 0) {
    switch (g_nSpyroState) {
    case 0x1D:
      if (g_nSpyroBreathTimer == 0x240) {
        if (g_nFlightLevelActive != 0) {
          (*(void (*)(void))g_pfnLevelFallImpactCallback)();
          (*(void (*)(void))g_pfnLevelUpdateActors)();
        } else {
          TriggerRespawnOrGameOver();
        }
      }
      break;
    case 0x1E:
      if (g_nSpyroStateTimer >= 0x65) {
        TriggerRespawnOrGameOver();
      }
      break;
    case 0x1F:
      if (g_nSpyroStateTimer >= 0x7D) {
        TriggerRespawnOrGameOver();
      }
      break;
    }
  } else if ((g_nSpyroWorldPosZ < 0x400) || ((g_nSpyroState == SPYRO_ST_FALL) &&
                                             (g_nSpyroStateFlags >= 0x79))) {
    TriggerRespawnOrGameOver();
  } else {
    pos = g_anSpyroWorldPosZBlock;
    if (pos[0] < g_anLevelDeathPlaneZTable[g_nLevelIndex]) {
      if ((g_nSpyroState != SPYRO_ST_FALL) &&
          (g_nSpyroState != SPYRO_ST_GLIDE_DROP)) {
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else if ((g_anSpyroWorldPos[0] < 0x800) || (g_nSpyroWorldPosY < 0x800)) {
      if ((g_nSpyroState != SPYRO_ST_FALL) &&
          (g_nSpyroState != SPYRO_ST_GLIDE_DROP)) {
        ZeroVector(pos + (0x104 / 4));
        ZeroVector(pos + (0xEC / 4));
        ChangeSpyroState(SPYRO_ST_FALL);
      }
    } else {
      goto done;
    }
    stateFlags = g_anSpyroStateFlagsBlock;
    if (stateFlags[0] == 0) {
      camRecord = g_anSpyroDeathCamRecordBlock;
      CopyVector(camRecord, g_anCameraPos);
      ActivateScriptedCameraView((int)(camRecord - (0x18 / 4)));
    }
    stateFlags[0] += g_nFrameStep;
  }
done:
  animBlock = g_abSpyroAnimPrevBlock;
  mesh = animBlock[0] * 4 + (int)g_apActorMeshTable;
  frame = g_bSpyroFramePrev * 4;
  sfx[0] = FRAME_SFX(FRAMEREC(mesh, frame));
  if (sfx[0] != 0xFF) {
    ptr = ANIM_SLOT(mesh);
    rec = frame + ptr;
    if (g_dwSpyroFootstepPrimarySfxKey != FRAME_KEY(rec)) {
      PlaySoundEffect(sfx[0], animBlock - 0x18, 4, animBlock + 0x288);
    }
  }
  key = FRAME_KEY(FRAMEREC(g_bSpyroAnimPrev * 4 + (int)g_apActorMeshTable,
                           g_bSpyroFramePrev * 4));
  footstep = g_adwSpyroFootstepSfxBlock;
  footstep[0] = key;
  sfx[1] =
      FRAME_SFX(FRAMEREC(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable,
                         g_bSpyroAnimLayer1FramePrev * 4));
  if ((sfx[1] != 0xFF) && (sfx[0] != sfx[1])) {
    ptr = ANIM_SLOT(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable);
    rec = g_bSpyroAnimLayer1FramePrev * 4 + ptr;
    if (g_dwSpyroFootstepSecondarySfxKey != FRAME_KEY(rec)) {
      PlaySoundEffect(sfx[1], (unsigned char *)footstep - FOOTSTEP_TO_SPYRO_POS,
                      4, (unsigned char *)footstep + FOOTSTEP_TO_VOICE_MARKER);
    }
  }
  key2 =
      FRAME_KEY(FRAMEREC(g_bSpyroAnimLayer1Prev * 4 + (int)g_apActorMeshTable,
                         g_bSpyroAnimLayer1FramePrev * 4));
  g_dwSpyroRequestMask &= 0x7FFFFFFF;
  g_dwSpyroFootstepSecondarySfxKey = key2;
  if (g_bSpyroPadSnapshotReverseFlag != 0) {
    SnapshotPadInputState(&g_dwPad2Buttons, &g_dwPadPressed);
  }
}
