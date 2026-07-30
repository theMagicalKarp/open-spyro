#include "globals.h"

extern void SnapshotPadInputState(unsigned int *dst, unsigned int *src);
extern void AdvanceSpyroFrameMotion();
extern void UpdateSpyroStateBehavior();
extern void TickSpyroAnimStateMachine(void);
extern void TickSpyroHornStrikeAttack(void);
extern void TickSpyroAnimLayer1(void);
extern void TickSpyroAnimLayer2GateTimer(void);
extern void TickSpyroAnimLayer2(void);
extern void ApplyEulerRotation(unsigned char *euler, int *mtx, int *scale);
extern void MulMatrix0(int *a, int *b, int *out);
extern void SampleSpyroShadowRingHeights(int mode);
extern void TickSpyroAttackEffects(int steps);
extern void TriggerRespawnOrGameOver(void);
extern void ZeroVector(int *vec);
extern void CopyVector(int *dst, int *src);
extern void ActivateScriptedCameraView(int record);
extern void ChangeSpyroState();
extern unsigned int PlaySoundEffect(unsigned int sampleId, unsigned char *owner,
                                    unsigned int mode, unsigned char *pPlaying);

/* Held-base alias views (see config/symbol_addrs.txt). */
extern int g_anSpyroPadSnapshotBlock[];               /* 0x80078c48 */
extern unsigned char g_abSpyroPersistentEulerBlock[]; /* 0x80078a64 */
extern int g_anSpyroWorldPosZBlock[];                 /* 0x80078a60 */
extern int g_anSpyroStateFlagsBlock[];                /* 0x80078ad4 */
extern int g_anSpyroDeathCamRecordBlock[];            /* 0x8006ebe4 */
extern unsigned char g_abSpyroAnimPrevBlock[];        /* 0x80078a70 */
extern unsigned int g_adwSpyroFootstepSfxBlock[];     /* 0x80078cbc */

extern int g_nSpyroWorldPosY; /* 0x80078a5c — g_anSpyroWorldPos[1] */
extern int g_nSpyroWorldPosZ; /* 0x80078a60 — g_anSpyroWorldPos[2] */

extern unsigned char D_80078A72; /* layer-1 anim index */
extern unsigned char D_80078A78; /* layer-1 frame index */

/* One frame record of an anim's frame table: g_apActorMeshTable[anim] holds a
   pointer to the table at +0x38, stride 4 per frame (+0x24 = sfx key,
   +0x27 = sfx id).  The frame-table pointer is re-read per use — the original
   never keeps it live across the sfx-id test. */
#define FRAMEREC(mesh, frame) ((frame) + *(volatile int *)((mesh) + 0x38))

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

  if (((g_dwSpyroRequestMask & 0x2000) != 0) ||
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
  if (g_nSpyroState == 0x1D) {
    if (g_nSpyroBreathTimer <= 0) {
      g_nSpyroBreathTimer = 1;
    }
  } else {
    g_nSpyroBreathTimer = 0;
  }
  if (((g_dwSpyroRequestMask & 0x100) == 0) &&
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
  bodyMtx = (int *)(euler + 0x28);
  euler[0] = g_nSpyroBodyPitch >> 4;
  g_abSpyroPersistentEuler[1] = g_nSpyroBodyRoll >> 4;
  g_abSpyroPersistentEuler[2] = g_nSpyroBodyYaw >> 4;
  ApplyEulerRotation(euler, bodyMtx, 0);
  composedMtx = (int *)(euler + 0x1BC);
  ApplyEulerRotation(euler + 4, composedMtx, 0);
  MulMatrix0(bodyMtx, composedMtx, composedMtx);
  SampleSpyroShadowRingHeights(1);
  TickSpyroAttackEffects(g_nFrameStep);
  g_dwSpyroTriggerEventFlags = 0;
  if (g_nLevelReadyFlag < 0) {
    switch (g_nSpyroState) {
    case 0x1D:
      if (g_nSpyroBreathTimer == 0x240) {
        if (g_nFlightLevelActive != 0) {
          (*(void (*)(void))g_pfnLevelFallImpactCallback)();
          (*(void (*)(void))g_pfnGamestate0EarlyHook)();
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
  } else if ((g_nSpyroWorldPosZ < 0x400) ||
             ((g_nSpyroState == 6) && (g_nSpyroStateFlags >= 0x79))) {
    TriggerRespawnOrGameOver();
  } else {
    pos = g_anSpyroWorldPosZBlock;
    if (pos[0] < g_anLevelDeathPlaneZTable[g_nLevelIntroIndex]) {
      if ((g_nSpyroState != 6) && (g_nSpyroState != 0x10)) {
        ChangeSpyroState(6);
      }
    } else if ((g_anSpyroWorldPos[0] < 0x800) || (g_nSpyroWorldPosY < 0x800)) {
      if ((g_nSpyroState != 6) && (g_nSpyroState != 0x10)) {
        ZeroVector(pos + (0x104 / 4));
        ZeroVector(pos + (0xEC / 4));
        ChangeSpyroState(6);
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
  sfx[0] = *(unsigned char *)(FRAMEREC(mesh, frame) + 0x27);
  if (sfx[0] != 0xFF) {
    ptr = *(volatile int *)(mesh + 0x38);
    rec = frame + ptr;
    if (g_dwSpyroFootstepPrimarySfxKey != *(unsigned int *)(rec + 0x24)) {
      PlaySoundEffect(sfx[0], animBlock - 0x18, 4, animBlock + 0x288);
    }
  }
  key =
      *(unsigned int *)(FRAMEREC(g_bSpyroAnimPrev * 4 + (int)g_apActorMeshTable,
                                 g_bSpyroFramePrev * 4) +
                        0x24);
  footstep = g_adwSpyroFootstepSfxBlock;
  footstep[0] = key;
  sfx[1] = *(unsigned char *)(FRAMEREC(D_80078A72 * 4 + (int)g_apActorMeshTable,
                                       D_80078A78 * 4) +
                              0x27);
  if ((sfx[1] != 0xFF) && (sfx[0] != sfx[1])) {
    ptr = *(volatile int *)(D_80078A72 * 4 + (int)g_apActorMeshTable + 0x38);
    rec = D_80078A78 * 4 + ptr;
    if (g_dwSpyroFootstepSecondarySfxKey != *(unsigned int *)(rec + 0x24)) {
      PlaySoundEffect(sfx[1], (unsigned char *)footstep - 0x264, 4,
                      (unsigned char *)footstep + 0x3C);
    }
  }
  key2 = *(unsigned int *)(FRAMEREC(D_80078A72 * 4 + (int)g_apActorMeshTable,
                                    D_80078A78 * 4) +
                           0x24);
  g_dwSpyroRequestMask &= 0x7FFFFFFF;
  g_dwSpyroFootstepSecondarySfxKey = key2;
  if (g_bSpyroPadSnapshotReverseFlag != 0) {
    SnapshotPadInputState(&g_dwPad2Buttons, &g_dwPadPressed);
  }
}
