#include "actor.h"
#include "actor_states.h"
#include "dragon_rescue.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "hud.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "match_asm.h"
#include "model.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "vecmath.h"

/* Matching notes. Several accesses are spelled to control the scheduler's
   memory disambiguation (sched.c true/anti dependence: a non-QImode store
   through a varying in-struct address never conflicts with a fixed-address
   scalar access, and nothing else is disambiguated):
   - the case-4 sample-bank entry is written through a struct view (SBE), so
     the voice-attr globals that follow do not chain behind it; those
     globals and the source-actor read before them are in turn written as
     one-field struct views, so they keep the original's order (and the
     StopSoundVoiceByIndex `li a1,4` keeps the call's delay slot);
   - the case-0 timer base is the function-scope `i` (it lives in s1, as the
     original's base does), read into a block-local before the rumble store;
   - both voice-marker arguments are `&GB[11]`, which gives the case-6 timer
     base its a3 preference and lets the case-0 and case-6 PlaySoundEffect
     tails cross-jump;
   - the case-2 5-holder is asm-opaque so it lands in the AllocActorRecordSlot
     delay slot, and case 4's `done` doubles as the case-5 table pointer (two
     sets, so sched1 does not boost it above AdvanceSpyroAnimFrame). */

/* The dragon-rescue state record at 0x80077058: [0] substate, [1] stream state,
   [5] timer, [6] sweep length, ... [24] statue, [25] mirror, [26] source. */

#define GB g_anDragonRescueBlock
#define EUL2 (((unsigned char *)g_anSpyroWorldPosBlock)[0xE])
#define SRC (*(Actor *volatile *)&g_pDragonRescueSourceActor)
#define STATUE (*(Actor *volatile *)&g_pDragonRescueStatueActor)
#define MIRROR (*(Actor *volatile *)&g_pDragonRescueSpyroMirrorActor)
#define SBE ((SAMPLEENTRY *)g_apSampleBankEntriesBlock[0])
/* One 0x18-byte key of the dragon-rescue camera path
 * (g_pDragonRescueCameraPath). */
typedef struct CamPathKey {
  int x, y, z;
  int roll, pitch, yaw;
} CamPathKey;
/* A key field as a plain byte-offset int reference; the struct member form
 * reschedules the six camera stores. */
#define CAM_KEY_RAW(p, field)                                                  \
  (*(int *)((char *)(p) + (int)&((CamPathKey *)0)->field))

/* The first AnimationHeader of a Model. */
#define FIRST_ANIM(m) MODEL_RAW(m, int, m_Animations[0])
#define W32(p, off) (*(int *)((char *)(p) + (off)))
#define H16(p, off) (*(short *)((char *)(p) + (off)))
#define U8(p, off) (*(unsigned char *)((char *)(p) + (off)))
#define REC(p) (*(int **)(p))
/* The source statue's DragonStatueState (REC(p) as the typed record). */
#define STATUE_ST(p) ((DragonStatueState *)REC(p))
#define SRCS (((struct { char *volatile p; } *)&g_pDragonRescueSourceActor)->p)
#define OTY ((int)g_dwDragonRescueOrbitTargetYaw)

/* Frames between keyframes of a mesh's animation for its frame-rate code. */
#define FRAME_STEP(model, step)                                                \
  {                                                                            \
    int rate = ((AnimationHeader *)(FIRST_ANIM(model)))->m_ProgressPerTick;    \
                                                                               \
    if (rate == 0x10) {                                                        \
      step = 8;                                                                \
    } else if (rate == 0x15) {                                                 \
      step = 6;                                                                \
    } else {                                                                   \
      step = 2;                                                                \
      if (rate == 0x20) {                                                      \
        step = 4;                                                              \
      }                                                                        \
    }                                                                          \
  }

/* 0x8002f3e4 (0x20d0): the dragon-rescue gamestate (GS 8). Substates:
   0 the aura star swells while Spyro keeps playing, 1 Spyro is swept round
   the statue and the camera eases in, 2 waits for the dragon assets and
   spawns the statue and Spyro's stand-in, 3 the statue turns to face its
   heading, 4 the camera path plays (with the dragon's voice line), 5 the
   statue lights up and the HUD icons roll in, 6 the dragon counter ticks,
   7 the fade back to the world. */
void DragonRescue_Update(void) {
  int vec[3];
  short ang[4];
  int t;
  int a;
  int d;
  int i;
  int done;

  GB[5] += g_nFrameStep;
  g_nDragonRescueSecondaryTimer += g_nFrameStep;
  switch (g_nDragonRescueSubstate) {
  case DRAGON_RESCUE_AURA: {
    int now;

    g_dwGamestateFrames++;
    g_pfnLevelUpdateActors();
    TickSpyroGameplayFrame();
    UpdateCameraFrame();
    g_nDragonRescueAuraStarEnabled = 1;
    g_bDragonRescueAuraStarColor = g_nDragonRescueTimer * 4 + 0x40;
    g_nDragonRescueAuraStarRadius = g_nDragonRescueTimer * 0x10;
    g_abDragonRescueAuraStarEuler[2] +=
        ((g_nDragonRescueTimer >> 4) + 2) * g_nFrameStep;
    if (g_nDragonRescueTimer > 0x20) {
      g_nDragonRescueFullscreenTintLevel = (g_nDragonRescueTimer - 0x20) * 0x10;
    } else {
      g_nDragonRescueFullscreenTintLevel = 0;
    }
    if (g_nPulseRumbleTimer < 8) {
      g_nPulseRumbleTimer = 8;
    }
    i = (int)&GB[5];
    now = *(int *)i;
    g_nPulseRumbleAmount = 0x46;
    if (now >= 0x30) {
      int *rec;

      g_nDragonRescueSubstate = DRAGON_RESCUE_SWEEP;
      *(int *)i = 0;
      g_nDragonRescueFullscreenTintLevel = 0xFF;
      ACTOR_RAW(SRC, int, posZ) -= 0x78;
      {
        Actor *src = SRC;

        g_nDragonRescueOrbitStartYaw =
            ArcTan2(g_anSpyroWorldPos[0] - src->posX,
                    g_nSpyroWorldPosY - src->posY, ATAN_ANGLE12);
      }
      SubtractVector(vec, g_anSpyroWorldPos, &SRC->posX);
      g_nDragonRescueOrbitStartRadius = VectorLength(vec, VEC_LEN_XY);
      {
        Actor *src = SRC;

        g_dwDragonRescueOrbitTargetYaw = STATUE_ST(src)->orbitYaw;
        g_nDragonRescueOrbitTargetRadius = STATUE_ST(src)->orbitRadius;
        g_nDragonRescueCameraStartYaw =
            ArcTan2(*(int *)(int)g_anCameraPos - ACTOR_RAW(src, int, posX),
                    g_anCameraPos[1] - ACTOR_RAW(src, int, posY), ATAN_ANGLE12);
      }
      SubtractVector(vec, g_anCameraPos, &SRC->posX);
      g_nDragonRescueCameraStartRadius = VectorLength(vec, VEC_LEN_XY);
      g_nDragonRescueCameraStartZ = g_anCameraPos[2];
      {
        Actor *src = SRC;

        rec = REC(src); /* DragonStatueState.camPos */
        g_nDragonRescueCameraTargetYaw = ArcTan2(
            W32(rec, 0) - src->posX, W32(rec, 4) - src->posY, ATAN_ANGLE12);
      }
      SubtractVector(vec, rec, &SRC->posX);
      g_nDragonRescueCameraTargetRadius = VectorLength(vec, VEC_LEN_XY);
      g_nDragonRescueCameraTargetZ = W32(rec, 8);
      g_nDragonRescueSweepDurationFrames =
          (AbsAngleDelta12(g_nDragonRescueOrbitStartYaw,
                           g_dwDragonRescueOrbitTargetYaw) >>
           4) +
          0x28;
      if (g_nDragonRescueSweepDurationFrames < 0x40) {
        g_nDragonRescueSweepDurationFrames = 0x40;
      }
      if (g_nDragonRescueSweepDurationFrames > 0x80) {
        g_nDragonRescueSweepDurationFrames = 0x80;
      }
      g_nSpyroSuperflameTimerSave = g_nSpyroSuperflameTimer;
      ResetSpyroEntity(RESET_SPYRO_FULL);
      g_bSpyroAnimPrev = 1;
      g_bSpyroAnimCurrent = 1;
      g_bSpyroAnimFrame = 1;
      g_bSpyroFramePrev = 0;
      g_bSpyroSubFrameTimer = 0;
      g_pEmitListWriteCursor = g_pEmitListBase;
      ((unsigned char *)g_pEmitListBase)[1] = 0xFF;
      StopAllSoundExceptMask(1 << SRC->voiceRow);
      PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_SWEEP), 0, SFX_MODE_PANNED, &GB[11]);
    }
  } break;
  case DRAGON_RESCUE_SWEEP:
    if (g_nDragonRescueTimer < 0x20) {
      int k = 0x20 - g_nDragonRescueTimer;

      g_nDragonRescueAuraStarEnabled = 1;
      g_bDragonRescueAuraStarColor = k * 4 + 0x40;
      g_nDragonRescueAuraStarRadius = k * 0x10;
      g_abDragonRescueAuraStarEuler[2] += ((k >> 4) + 2) * g_nFrameStep;
    } else {
      g_nDragonRescueAuraStarEnabled = 0;
    }
    if (g_nDragonRescueTimer < 8) {
      g_nDragonRescueFullscreenTintLevel = (8 - g_nDragonRescueTimer) << 5;
    } else {
      g_nDragonRescueFullscreenTintLevel = 0;
    }
    {
      Actor *s = SRC;

      s->subMode = (s->subMode + 1) & 3;
    }
    {
      Actor *s = SRC;

      s->rotX = g_abDragonRescueFlickerPairs1[s->subMode * 2];
    }
    {
      Actor *s = SRC;

      s->rotY = g_abDragonRescueFlickerPairs1Y[s->subMode * 2];
    }
    if (g_nPulseRumbleTimer < 8) {
      g_nPulseRumbleTimer = 8;
    }
    g_nPulseRumbleAmount = 0x46;
    g_pfnLevelSpawnActor(ACTOR_TYPE_STATUE_SHARD, g_pDragonRescueSourceActor);
    if (g_nDragonRescueTimer < g_nDragonRescueSweepDurationFrames) {
      t = LookupSine((g_nDragonRescueTimer << 11) /
                         g_nDragonRescueSweepDurationFrames -
                     0x400) +
          0x1000;
      if (((OTY - g_nDragonRescueOrbitStartYaw) & 0xFFF) < 0x801) {
        a = g_nDragonRescueOrbitStartYaw +
            ((AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) * t) >> 13);
      } else {
        a = g_nDragonRescueOrbitStartYaw -
            ((AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) * t) >> 13);
      }
      d = g_nDragonRescueOrbitStartRadius +
          (((g_nDragonRescueOrbitTargetRadius -
             g_nDragonRescueOrbitStartRadius) *
            (LookupSine((g_nDragonRescueTimer << 10) /
                            g_nDragonRescueSweepDurationFrames -
                        0x400) +
             0x1000)) >>
           12);
      CopyVector(vec, g_anSpyroWorldPos);
      g_anSpyroWorldPos[0] = SRC->posX + ((LookupCosine(a) * d) >> 12);
      g_nSpyroWorldPosY = SRC->posY + ((LookupSine(a) * d) >> 12);
      SubtractVector(vec, g_anSpyroWorldPos, vec);
      {
        int dir = ArcTan2(vec[0], vec[1], ATAN_ANGLE8);

        if (AbsAngleDelta8(dir, g_abSpyroPersistentEuler[2]) >= 4) {
          if (((dir - g_abSpyroPersistentEuler[2]) & 0xFF) < 0x81) {
            g_abSpyroPersistentEuler[2] =
                g_abSpyroPersistentEuler[2] +
                (AbsAngleDelta8(dir, g_abSpyroPersistentEuler[2]) >> 2);
          } else {
            g_abSpyroPersistentEuler[2] =
                g_abSpyroPersistentEuler[2] -
                (AbsAngleDelta8(dir, g_abSpyroPersistentEuler[2]) >> 2);
          }
        }
      }
      g_nSpyroAnimPlayRate_blk[0] = VectorLength(vec, VEC_LEN_XY) >> 1;
      if (g_nSpyroAnimPlayRate_blk[0] < 4) {
        g_nSpyroAnimPlayRate_blk[0] = 4;
      }
      if (g_nSpyroAnimPlayRate_blk[0] > 0x10) {
        g_nSpyroAnimPlayRate_blk[0] = 0x10;
      }
      if (((GB[21] - GB[18]) & 0xFFF) < 0x801) {
        a = GB[18] + ((AbsAngleDelta12(GB[21], GB[18]) * t) >> 13);
      } else {
        a = GB[18] - ((AbsAngleDelta12(GB[21], GB[18]) * t) >> 13);
      }
      d = g_nDragonRescueCameraStartRadius +
          (((g_nDragonRescueCameraTargetRadius -
             g_nDragonRescueCameraStartRadius) *
            t) >>
           13);
      g_anCameraPos[0] = SRC->posX + ((LookupCosine(a) * d) >> 12);
      g_anCameraPos[1] = SRC->posY + ((LookupSine(a) * d) >> 12);
      g_anCameraPos[2] =
          g_nDragonRescueCameraStartZ +
          (((g_nDragonRescueCameraTargetZ - g_nDragonRescueCameraStartZ) * t) >>
           13);
      {
        int land = g_nDragonRescueSweepDurationFrames - 0x10;

        if (g_nDragonRescueTimer >= land &&
            g_nDragonRescueTimer - g_nFrameStep < land) {
          g_bSpyroFramePrev = g_bSpyroAnimFrame;
          g_bSpyroAnimCurrent = 3;
          g_bSpyroAnimFrame = 0;
          g_bSpyroSubFrameTimer = 0;
          g_nSpyroAnimPlayRate = 0xC;
        }
      }
    } else if (g_nDragonRescueTimer <
               g_nDragonRescueSweepDurationFrames + 0x20) {
      if (g_nDragonRescueTimer - g_nFrameStep <
          g_nDragonRescueSweepDurationFrames) {
        g_anSpyroWorldPos[0] =
            SRC->posX + ((LookupCosine(g_dwDragonRescueOrbitTargetYaw) *
                          g_nDragonRescueOrbitTargetRadius) >>
                         12);
        {
          int s = LookupSine(g_dwDragonRescueOrbitTargetYaw) *
                  g_nDragonRescueOrbitTargetRadius;
          Actor *src = SRC;
          int y = ACTOR_RAW(src, int, posY);

          g_nDragonRescueOrbitStartYaw = g_abSpyroPersistentEuler[2] << 4;
          g_dwDragonRescueOrbitTargetYaw =
              (g_dwDragonRescueOrbitTargetYaw + 0x800) & 0xFFF;
          g_nSpyroWorldPosY = y + (s >> 12);
          CopyVector(g_anCameraPos, REC(src));
        }
      }
      if (((OTY - g_nDragonRescueOrbitStartYaw) & 0xFFF) < 0x801) {
        a = g_nDragonRescueOrbitStartYaw +
            (AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) *
             (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames)) /
                32;
      } else {
        a = g_nDragonRescueOrbitStartYaw -
            (AbsAngleDelta12(OTY, g_nDragonRescueOrbitStartYaw) *
             (g_nDragonRescueTimer - g_nDragonRescueSweepDurationFrames)) /
                32;
      }
      g_abSpyroPersistentEuler[2] = a >> 4;
      g_nSpyroAnimPlayRate = 8;
    } else {
      g_nDragonRescueSubstate = DRAGON_RESCUE_LOAD_WAIT;
      g_bSpyroSubFrameTimer = 4;
      g_nDragonRescueTimer = 0;
      g_bSpyroAnimCurrent = 0;
      g_bSpyroAnimFrame = 0;
      g_nSpyroAnimPlayRate = 4;
      g_abSpyroPersistentEuler[2] = (int)g_dwDragonRescueOrbitTargetYaw >> 4;
    }
    if (g_nDragonRescueSubstate == DRAGON_RESCUE_SWEEP) {

      AddVector(vec, g_anSpyroWorldPos, &SRC->posX);
      RShiftVector3(vec, 1);
      SubtractVector(vec, vec, g_anCameraPos);
      if (g_nDragonRescueTimer < 0x10) {
        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        ang[2] = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
        g_nCameraEulerRoll = (g_nCameraEulerRoll +
                              ((SignedAngleDelta12(ang[0], g_nCameraEulerRoll) *
                                g_nDragonRescueTimer) >>
                               4)) &
                             0xFFF;
        g_nCameraEulerPitch =
            (g_nCameraEulerPitch +
             ((SignedAngleDelta12(ang[1], g_nCameraEulerPitch) *
               g_nDragonRescueTimer) >>
              4)) &
            0xFFF;
        g_nCameraEulerYaw = (g_nCameraEulerYaw +
                             ((SignedAngleDelta12(ang[2], g_nCameraEulerYaw) *
                               g_nDragonRescueTimer) >>
                              4)) &
                            0xFFF;
      } else if (g_nDragonRescueTimer <
                 g_nDragonRescueSweepDurationFrames - 0x20) {
        g_nCameraEulerRoll = 0;
        g_nCameraEulerPitch =
            ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        g_nCameraEulerYaw = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
      } else {
        /* DragonStatueState.camEuler (+0x0C..+0x14) as byte-offset ints: the
           struct member form reschedules the three blends (A400) */
        int *rec = REC(SRC);

        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), -vec[2], ATAN_ANGLE12);
        ang[2] = ArcTan2(vec[0], vec[1], ATAN_ANGLE12);
        g_nCameraEulerRoll =
            (ang[0] + ((SignedAngleDelta12(W32(rec, 0xc), ang[0]) *
                        (g_nDragonRescueTimer -
                         g_nDragonRescueSweepDurationFrames + 0x20)) >>
                       6)) &
            0xFFF;
        g_nCameraEulerPitch =
            (ang[1] + ((SignedAngleDelta12(W32(rec, 0x10), ang[1]) *
                        (g_nDragonRescueTimer -
                         g_nDragonRescueSweepDurationFrames + 0x20)) >>
                       6)) &
            0xFFF;
        g_nCameraEulerYaw =
            (ang[2] + ((SignedAngleDelta12(W32(rec, 0x14), ang[2]) *
                        (g_nDragonRescueTimer -
                         g_nDragonRescueSweepDurationFrames + 0x20)) >>
                       6)) &
            0xFFF;
      }
    }
    {
      vec[0] = (g_nSpyroGroundNormalX * LookupCosine(EUL2 << 4) +
                g_nSpyroGroundNormalY * LookupSine(EUL2 << 4)) >>
               12;
      vec[1] = (g_nSpyroGroundNormalY * LookupCosine(EUL2 << 4) -
                g_nSpyroGroundNormalX * LookupSine(EUL2 << 4)) >>
               12;
      vec[2] = g_anSpyroGroundNormal[2];
      g_abSpyroPersistentEuler[0] = -ArcTan2(
          IntegerSqrt(vec[0] * vec[0] + vec[2] * vec[2]), vec[1], ATAN_ANGLE12);
      g_abSpyroPersistentEuler[1] = -ArcTan2(vec[2], vec[0], ATAN_ANGLE12);
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      g_nSpyroWorldPosZ += 0x400;
      g_nSpyroGroundHeightZ = FindWorldFloorBelowPoint(g_anSpyroWorldPosBlock);
      g_nSpyroWorldPosZ = g_nSpyroGroundHeightZ + 0x164;
      SampleSpyroShadowRingHeights(0);
    }
    break;
  case DRAGON_RESCUE_LOAD_WAIT: {
    Actor *s = SRC;

    s->subMode = (s->subMode + 1) & 3;
  }
    {
      Actor *s = SRC;

      s->rotX = g_abDragonRescueFlickerPairs2[s->subMode * 2];
    }
    {
      Actor *s = SRC;

      s->rotY = g_abDragonRescueFlickerPairs2Y[s->subMode * 2];
    }
    if (g_nPulseRumbleTimer < 8) {
      g_nPulseRumbleTimer = 8;
    }
    g_nPulseRumbleAmount = 0x46;
    g_pfnLevelSpawnActor(ACTOR_TYPE_STATUE_SHARD, g_pDragonRescueSourceActor);
    if (g_nDragonRescueTimer < 0x10 ||
        g_nDragonRescueStreamState < RESCUE_STREAM_RELOAD_LEVEL) {
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    } else {
      g_nDragonRescueSubstate = DRAGON_RESCUE_TURN;
      g_nDragonRescueTimer = 0;
      {
        int n;
        int five;

        for (n = 0; n < 0x20; n++) {
          g_pfnLevelSpawnActor(ACTOR_TYPE_STATUE_SHARD,
                               g_pDragonRescueSourceActor);
        }
        OPAQUE_COPY(five, 5);
        GB[24] = (int)AllocActorRecordSlot();
        InitActorRenderDefaults((void *)GB[24]);
        ACTOR_RAW(STATUE, short, type) = ACTOR_TYPE_RESCUED_DRAGON;
        CopyVector(&((Actor *)GB[24])->posX, &SRC->posX);
        {
          Actor *s = SRC;
          Actor *t = STATUE;

          t->rotZ = s->rotZ;
        }
        STATUE->scale = 0x60;
        STATUE->otBias = five;
        ACTOR_RAW(STATUE, int, groundCode) = -1;
        STATUE->posZ += 0x400;
        FindWorldFloorBelowPoint(&((Actor *)GB[24])->posX);
        STATUE->posZ -= 0x400;
        EncodeCachedVecToActorDirCode((void *)GB[24]);
        InsertActorIntoSpatialGrid((void *)GB[24]);
        g_pDragonRescueSpyroMirrorActor = AllocActorRecordSlot();
        InitActorRenderDefaults(g_pDragonRescueSpyroMirrorActor);
        ACTOR_RAW(MIRROR, short, type) = ACTOR_TYPE_RESCUE_SPYRO;
        CopyVector(&MIRROR->posX, g_anSpyroWorldPos);
        MIRROR->otBias = five;
        InsertActorIntoSpatialGrid(MIRROR);
        StopSoundVoiceByIndex(GB[11], 4);
        PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_STATUE), 0, SFX_MODE_PANNED,
                        &GB[11]);
        ACTOR_RAW(SRC, int, posZ) += 0x78;
        DespawnActorRecord(SRC);
      }
    }
    break;
  case DRAGON_RESCUE_TURN:
    if (GB[5] < 0x18) {
      STATUE->scale = 0x60 - GB[5] * 3;
      {
        Actor *s = SRC;
        int turn;

        turn = SignedAngleDelta8(STATUE_ST(s)->dragonYaw >> 4, s->rotZ) *
               GB[5] / 24;
        {
          Actor *s2 = SRC;
          Actor *t = STATUE;

          t->rotZ = s2->rotZ + turn;
        }
      }
    } else {
      Actor *s = SRC;
      Actor *t;

      g_nDragonRescueSubstate = DRAGON_RESCUE_CAMERA_PATH;
      GB[5] = 0;
      t = STATUE;
      t->rotZ = STATUE_ST(s)->dragonYaw >> 4;
      STATUE->scale = 0;
      MIRROR->rotX = g_abSpyroPersistentEuler[0];
      MIRROR->rotY = g_abSpyroPersistentEuler[1];
      MIRROR->rotZ = g_abSpyroPersistentEuler[2];
    }
    break;
  case DRAGON_RESCUE_CAMERA_PATH: {
    int save[5];
    int step;

    done = 0;
    if (GB[5] < g_nDragonRescueCameraPathCount * 2) {
      if (g_nDragonRescueAudioFiredFlag == 0 &&
          GB[5] >= STATUE_ST(SRC)->speechFrame) {
        CopyWords(save, *(void *volatile *)g_apSampleBankEntriesBlock, 0x14);
        SBE->nSampleAddr = 0x80000 - g_nDragonRescueAudioSampleSize;
        SBE->nLoopAddr = -1;
        SBE->nVolume = 0x32;
        {
          int rate = (STATUE_ST(SRCS)->speechRate << 10) / 11025;

          SBE->nPitch = rate;
        }
        SBE->nPitchSpread = 0;
        SBE->nPitchStep = 0;
        SBE->nPitchMode = 0;
        ((struct { int v; } *)&g_nSfxOverrideFlags)->v = 1;
        ((struct { short v; } *)&g_nSfxOverrideVolL)->v = 0x3FFF;
        ((struct { short v; } *)&g_nSfxOverrideVolR)->v = 0x3FFF;
        StopSoundVoiceByIndex(GB[11], 4);
        PlaySoundEffect(0, 0, SFX_MODE_PANNED, &GB[11]);
        CopyWords(*(void *volatile *)g_apSampleBankEntriesBlock, save, 0x14);
        g_nDragonRescueAudioFiredFlag = 1;
      }
      {
        int *p = (int *)((g_nDragonRescueTimer >> 1) * 0x18 +
                         (int)g_pDragonRescueCameraPath);

        g_anCameraPos[0] = CAM_KEY_RAW(p, x);
        g_anCameraPos[1] = CAM_KEY_RAW(p, y);
        g_anCameraPos[2] = CAM_KEY_RAW(p, z);
        g_nCameraEulerRoll = CAM_KEY_RAW(p, roll);
        g_nCameraEulerPitch = CAM_KEY_RAW(p, pitch);
        g_nCameraEulerYaw = CAM_KEY_RAW(p, yaw);
      }
      FRAME_STEP(g_pDragonRescueStatueModelData, step);
      STATUE->frame = GB[5] / step;
      {
        Actor *p = STATUE;

        p->nextFrame = p->frame + 1;
      }
      STATUE->frameProgress = (GB[5] % step << 6) / step;
      {
        short n =
            ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueStatueModelData)))
                ->m_NumFrames;
        Actor *p = STATUE;

        if (p->nextFrame >= n) {
          p->frame = n - 1;
          STATUE->frameProgress = 0;
        }
      }
      FRAME_STEP(g_pDragonRescueMirrorModelData, step);
      MIRROR->frame = GB[5] / step;
      {
        Actor *p = MIRROR;

        p->nextFrame = p->frame + 1;
      }
      MIRROR->frameProgress = (GB[5] % step << 6) / step;
      {
        short n =
            ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueMirrorModelData)))
                ->m_NumFrames;
        Actor *p = MIRROR;

        if (p->nextFrame >= n) {
          p->frame = n - 1;
          MIRROR->frameProgress = 0;
        }
      }
      if (g_dwPadPressed & PAD_CROSS) {
        if (STATUE_ST(SRC)->noSkip != 0) {
          if (g_nDragonRescuePadHoldTimer == 0) {
            g_nDragonRescuePadHoldTimer = 1;
          }
        } else {
          done = 1;
        }
      }
    }
    if (done != 0 ||
        g_nDragonRescueTimer >= g_nDragonRescueCameraPathCount * 2) {
      int *p;

      g_nDragonRescueSubstate = DRAGON_RESCUE_LIGHT_UP;
      g_nDragonRescueTimer = 0;
      p = (int *)(g_nDragonRescueCameraPathCount * 0x18 +
                  (int)g_pDragonRescueCameraPath);
      g_anCameraPos[0] = CAM_KEY_RAW((CamPathKey *)p - 1, x);
      g_anCameraPos[1] = CAM_KEY_RAW((CamPathKey *)p - 1, y);
      g_anCameraPos[2] = CAM_KEY_RAW((CamPathKey *)p - 1, z);
      g_nCameraEulerRoll = CAM_KEY_RAW((CamPathKey *)p - 1, roll);
      g_nCameraEulerPitch = CAM_KEY_RAW((CamPathKey *)p - 1, pitch);
      g_nCameraEulerYaw = CAM_KEY_RAW((CamPathKey *)p - 1, yaw);
      STATUE->frame =
          ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueStatueModelData)))
              ->m_NumFrames -
          1;
      STATUE->frameProgress = 0;
      MIRROR->frame =
          ((AnimationHeader *)(FIRST_ANIM(g_pDragonRescueMirrorModelData)))
              ->m_NumFrames -
          1;
      MIRROR->frameProgress = 0;
      g_bSpyroAnimPrev = 0;
      g_bSpyroAnimCurrent = 0;
      g_bSpyroFramePrev = 0;
      g_bSpyroAnimFrame = 1;
      g_bSpyroSubFrameTimer = 0;
      g_nSpyroAnimPlayRate = 4;
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      StopSoundVoiceByIndex(*(int *)&g_bSceneSfxVoiceMarker, 2);
      TickSpuPerFrame();
      if (g_nDragonRescueSplitFlag == 0) {
        SetSpuTransferStartAddr(0x80000 - g_nDragonRescueAudioSampleSize);
        WriteSpuRam(g_pWorldUnpackHead + g_nDragonRescueAudioSampleOffset,
                    g_nDragonRescueAudioSampleSize);
        while (GetSpuTransferStatus(0) == 0) {
        }
      }
    }
    if (GB[13] != 0) {
      GB[13] += g_nFrameStep;
      if (GB[13] >= 0x78) {
        GB[13] = 0;
      }
    }
  } break;
  case DRAGON_RESCUE_LIGHT_UP: {
    int *icon;
    int k;
    int off;

    for (i = 0; i < ANIM_HEADER_RAW(FIRST_ANIM(g_pDragonRescueStatueModelData),
                                    short, m_NumColors);
         i++) {
      unsigned int c =
          ComputeDepthCuedColor(((int *)((AnimationHeader *)(FIRST_ANIM(
                                             g_pDragonRescueStatueModelData)))
                                     ->m_Colors)[i],
                                0xFFFFFF, 0x200);
      int *dst = (int *)(i * 4 + ANIM_HEADER_RAW(
                                     FIRST_ANIM(g_pDragonRescueStatueModelData),
                                     int, m_Colors));

      *dst = c;
    }
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    i = 0;
    done = (int)g_anHudRollInStepTable; /* shares s4 with case 4 */
    icon = &HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS)->posY;
    k = 0;
    off = 0;
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
  roll_in:
    CopyVector(&HUD_ICON(5 * ACTOR_SIZE + k)->posX,
               (int *)((char *)g_anDragonRescueIconRestPos + off));
    if (g_nDragonRescueTimer < 0x19) {
      *icon += (((short *)done)[g_nDragonRescueTimer >> 1] * 3) >> 1;
    } else {
      *icon += (g_anHudRollInStepTable[0xC] * 3) >> 1;
    }
    icon += 0x16;
    k += ACTOR_SIZE;
    i++;
    off += 0xC;
    if (i < 3) {
      goto roll_in;
    }
    if (GB[5] >= 0x19) {
      g_nDragonRescueSubstate = DRAGON_RESCUE_COUNT;
      GB[5] = 0;
      g_bHudDragonRollPhase = 0;
      CopyVector((int *)ang, &g_pDragonRescueStatueActor->posX);
      ((int *)ang)[2] += 0x40;
      g_pfnLevelEmitParticles(0x20, EMIT_RESCUE_SWIRL, (int *)ang, 0);
      CopyVector(g_anDragonRescueAuraStarAnchor, (int *)ang);
      DespawnActorRecord(STATUE);
      DespawnActorRecord(MIRROR);
      PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_DEPART), 0, SFX_MODE_PANNED,
                      &GB[11]);
    }
  } break;
  case DRAGON_RESCUE_COUNT:
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    if (g_anHudDragonCachedCountBlock[0] != g_nTotalDragonsRescued ||
        g_bHudDragonRollPhase != 0) {
      g_bHudDragonRollPhase += 0x10;
      SetHudCounterDigits(HUD_SLOT_DRAGON_DIGITS, 2,
                          g_anHudDragonCachedCountBlock[0] + 1, 0);
      CommitHudCounterAnimState(5, 2, g_bHudDragonRollPhase);
      if (g_bHudDragonRollPhase == 0xC0) {
        g_anHudDragonCachedCountBlock[0]++;
      }
      if (g_bHudDragonRollPhase == 0) {
        PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_COUNT), 0, SFX_MODE_PANNED,
                        &g_bSceneSfxVoiceMarker);
      }
    }
    if (GB[5] >= 0x60 && g_nDragonRescueStreamState == RESCUE_STREAM_DONE) {
      g_nDragonRescueSubstate = DRAGON_RESCUE_FADE_OUT;
      GB[5] = 0;
      g_pEmitListWriteCursor = g_pEmitListBase;
      ((unsigned char *)g_pEmitListBase)[1] = 0xFF;
      PlaySoundEffect(LEVEL_SFX(SFX_RESCUE_FADE), 0, SFX_MODE_PANNED, &GB[11]);
    }
    break;
  case DRAGON_RESCUE_FADE_OUT: {
    int k;

    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    if (g_nDragonRescueTimer < 0x10) {
      int n;
      int *icon;
      int j;
      int off;

      n = 0;
      icon = &HUD_ICON_SLOT(HUD_SLOT_DRAGON_DIGITS)->posY;
      j = 0;
      off = 0;
    roll_out:
      CopyVector(&HUD_ICON(5 * ACTOR_SIZE + j)->posX,
                 (int *)((char *)g_anDragonRescueIconRestPos + off));
      j += ACTOR_SIZE;
      off += 0xC;
      n++;
      *icon += (0x11 - g_nDragonRescueTimer) * 3;
      icon += 0x16;
      if (n < 3) {
        goto roll_out;
      }
    }
    if (g_nDragonRescueTimer < 0x10) {
      k = g_nDragonRescueTimer;
    } else {
      k = 0x20 - g_nDragonRescueTimer;
    }
    g_nDragonRescueFullscreenTintLevel = k * 0x10 - 1;
    g_anDragonRescueAuraStarBlock[0] = 1;
    g_bDragonRescueAuraStarColor = k * 8 + 0x40;
    g_nDragonRescueAuraStarRadius = k * 0x50;
    g_abDragonRescueAuraStarEuler[2] += 8;
    if (g_nDragonRescueTimer >= 0x20) {
      g_anDragonRescueAuraStarBlock[0] = 0;
      EndDragonRescueToWorld();
    }
  } break;
  }
  g_pfnLevelAdvanceEmitList(g_nFrameStep);
  if (g_nDragonRescueSubstate > DRAGON_RESCUE_AURA) {
    TickDragonRescueAssetStream();
  }
  {
    Actor *actor = g_pActorListBase;

    while (actor->mode != ACTOR_MODE_END) {
      if (actor->type == ACTOR_TYPE_STATUE_SHARD &&
          actor->mode < ACTOR_MODE_DORMANT) {
        TickStatueBurstShard(actor);
      }
      actor++;
    }
  }
}
