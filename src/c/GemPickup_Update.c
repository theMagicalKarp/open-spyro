#include "actor.h"
#include "funcs.h"
#include "globals.h"
#include "hud.h"
#include "main_shared.h"
#include "model.h"
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

/* The gem-pickup state record at 0x80077058: [0] substate, [1] stream state,
   [5] timer, [6] sweep length, ... [24] statue, [25] mirror, [26] source. */

#define GB g_anGemPickupBlock
#define EUL2 (((unsigned char *)g_anSpyroWorldPosBlock)[0xE])
#define SRC (*(char *volatile *)&g_pGemPickupSourceActor)
#define STATUE (*(char *volatile *)&g_pGemPickupStatueActor)
#define MIRROR (*(char *volatile *)&g_pGemPickupSpyroMirrorActor)
struct SampleBankEntry {
  int addr;
  int loop;
  short vol;
  short rate;
  short h0;
  short h1;
  int w4;
};
#define SBE ((struct SampleBankEntry *)g_apSampleBankEntriesBlock[0])
/* One 0x18-byte key of the gem-pickup camera path (g_pGemPickupCameraPath). */
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
#define SRCS (((struct { char *volatile p; } *)&g_pGemPickupSourceActor)->p)
#define OTY ((int)g_dwGemPickupOrbitTargetYaw)

/* Frames between keyframes of a mesh's animation for its frame-rate code. */
#define FRAME_STEP(model, step)                                                \
  {                                                                            \
    int rate =                                                                 \
        ANIM_HEADER_RAW(FIRST_ANIM(model), unsigned char, m_ProgressPerTick);  \
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

/* 0x8002f3e4 (0x20d0): the dragon-rescue (gem pickup) gamestate. Substates:
   0 the aura star swells while Spyro keeps playing, 1 Spyro is swept round
   the statue and the camera eases in, 2 waits for the dragon assets and
   spawns the statue and Spyro's stand-in, 3 the statue turns to face its
   heading, 4 the camera path plays (with the dragon's voice line), 5 the
   statue lights up and the HUD icons roll in, 6 the dragon counter ticks,
   7 the fade back to the world. */
void GemPickup_Update(void) {
  int vec[3];
  short ang[4];
  int t;
  int a;
  int d;
  int i;
  int done;

  GB[5] += g_nFrameStep;
  g_nGemPickupSecondaryTimer += g_nFrameStep;
  switch (g_nGemPickupSubstate) {
  case 0: {
    int now;

    g_dwGamestateFrames++;
    ((void (*)(void))g_pfnLevelUpdateActors)();
    TickSpyroGameplayFrame();
    UpdateCameraFrame();
    g_nGemPickupAuraStarEnabled = 1;
    g_bGemPickupAuraStarColor = g_nGemPickupTimer * 4 + 0x40;
    g_nGemPickupAuraStarRadius = g_nGemPickupTimer * 0x10;
    g_abGemPickupAuraStarEuler[2] +=
        ((g_nGemPickupTimer >> 4) + 2) * g_nFrameStep;
    if (g_nGemPickupTimer > 0x20) {
      g_nGemPickupFullscreenTintLevel = (g_nGemPickupTimer - 0x20) * 0x10;
    } else {
      g_nGemPickupFullscreenTintLevel = 0;
    }
    if (g_nPulseRumbleTimer < 8) {
      g_nPulseRumbleTimer = 8;
    }
    i = (int)&GB[5];
    now = *(int *)i;
    g_nPulseRumbleAmount = 0x46;
    if (now >= 0x30) {
      int *rec;

      g_nGemPickupSubstate = 1;
      *(int *)i = 0;
      g_nGemPickupFullscreenTintLevel = 0xFF;
      ACTOR_RAW(SRC, int, posZ) -= 0x78;
      {
        char *src = SRC;

        g_nGemPickupOrbitStartYaw =
            ArcTan2(g_anSpyroWorldPos[0] - ACTOR_RAW(src, int, posX),
                    g_nSpyroWorldPosY - ACTOR_RAW(src, int, posY), 1);
      }
      SubtractVector(vec, g_anSpyroWorldPos, &((Actor *)SRC)->posX);
      g_nGemPickupOrbitStartRadius = VectorLength(vec, 0);
      {
        char *src = SRC;

        g_dwGemPickupOrbitTargetYaw = W32(REC(src), 0x28);
        g_nGemPickupOrbitTargetRadius = W32(REC(src), 0x2c);
        g_nGemPickupCameraStartYaw =
            ArcTan2(*(int *)(int)g_anCameraPos - ACTOR_RAW(src, int, posX),
                    g_anCameraPos[1] - ACTOR_RAW(src, int, posY), 1);
      }
      SubtractVector(vec, g_anCameraPos, &((Actor *)SRC)->posX);
      g_nGemPickupCameraStartRadius = VectorLength(vec, 0);
      g_nGemPickupCameraStartZ = g_anCameraPos[2];
      {
        char *src = SRC;

        rec = REC(src);
        g_nGemPickupCameraTargetYaw =
            ArcTan2(W32(rec, 0) - ACTOR_RAW(src, int, posX),
                    W32(rec, 4) - ACTOR_RAW(src, int, posY), 1);
      }
      SubtractVector(vec, rec, &((Actor *)SRC)->posX);
      g_nGemPickupCameraTargetRadius = VectorLength(vec, 0);
      g_nGemPickupCameraTargetZ = W32(rec, 8);
      g_nGemPickupSweepDurationFrames =
          (AbsAngleDelta12(g_nGemPickupOrbitStartYaw,
                           g_dwGemPickupOrbitTargetYaw) >>
           4) +
          0x28;
      if (g_nGemPickupSweepDurationFrames < 0x40) {
        g_nGemPickupSweepDurationFrames = 0x40;
      }
      if (g_nGemPickupSweepDurationFrames > 0x80) {
        g_nGemPickupSweepDurationFrames = 0x80;
      }
      g_nSpyroFlameBreathTimerSave = g_nSpyroFlameBreathTimer;
      ResetSpyroEntity(1);
      g_bSpyroAnimPrev = 1;
      g_bSpyroAnimCurrent = 1;
      g_bSpyroAnimFrame = 1;
      g_bSpyroFramePrev = 0;
      g_bSpyroSubFrameTimer = 0;
      g_pEmitListWriteCursor = g_pEmitListBase;
      ((unsigned char *)g_pEmitListBase)[1] = 0xFF;
      StopAllSoundExceptMask(1 << ACTOR_RAW(SRC, unsigned char, sfxSlot));
      PlaySoundEffect(LEVEL_SFX(0x32), 0, 0x10, &GB[11]);
    }
  } break;
  case 1:
    if (g_nGemPickupTimer < 0x20) {
      int k = 0x20 - g_nGemPickupTimer;

      g_nGemPickupAuraStarEnabled = 1;
      g_bGemPickupAuraStarColor = k * 4 + 0x40;
      g_nGemPickupAuraStarRadius = k * 0x10;
      g_abGemPickupAuraStarEuler[2] += ((k >> 4) + 2) * g_nFrameStep;
    } else {
      g_nGemPickupAuraStarEnabled = 0;
    }
    if (g_nGemPickupTimer < 8) {
      g_nGemPickupFullscreenTintLevel = (8 - g_nGemPickupTimer) << 5;
    } else {
      g_nGemPickupFullscreenTintLevel = 0;
    }
    {
      char *s = SRC;

      ACTOR_RAW(s, unsigned char, subMode) =
          (ACTOR_RAW(s, unsigned char, subMode) + 1) & 3;
    }
    {
      char *s = SRC;

      ACTOR_RAW(s, unsigned char, rotX) =
          g_abGemPickupFlickerPairs1[ACTOR_RAW(s, unsigned char, subMode) * 2];
    }
    {
      char *s = SRC;

      ACTOR_RAW(s, unsigned char, rotY) =
          g_abGemPickupFlickerPairs1Y[ACTOR_RAW(s, unsigned char, subMode) * 2];
    }
    if (g_nPulseRumbleTimer < 8) {
      g_nPulseRumbleTimer = 8;
    }
    g_nPulseRumbleAmount = 0x46;
    ((void (*)(int, void *))g_pfnLevelSpawnActor)(0xFB,
                                                  g_pGemPickupSourceActor);
    if (g_nGemPickupTimer < g_nGemPickupSweepDurationFrames) {
      t = LookupSine((g_nGemPickupTimer << 11) /
                         g_nGemPickupSweepDurationFrames -
                     0x400) +
          0x1000;
      if (((OTY - g_nGemPickupOrbitStartYaw) & 0xFFF) < 0x801) {
        a = g_nGemPickupOrbitStartYaw +
            ((AbsAngleDelta12(OTY, g_nGemPickupOrbitStartYaw) * t) >> 13);
      } else {
        a = g_nGemPickupOrbitStartYaw -
            ((AbsAngleDelta12(OTY, g_nGemPickupOrbitStartYaw) * t) >> 13);
      }
      d = g_nGemPickupOrbitStartRadius +
          (((g_nGemPickupOrbitTargetRadius - g_nGemPickupOrbitStartRadius) *
            (LookupSine((g_nGemPickupTimer << 10) /
                            g_nGemPickupSweepDurationFrames -
                        0x400) +
             0x1000)) >>
           12);
      CopyVector(vec, g_anSpyroWorldPos);
      g_anSpyroWorldPos[0] =
          ACTOR_RAW(SRC, int, posX) + ((LookupCosine(a) * d) >> 12);
      g_nSpyroWorldPosY =
          ACTOR_RAW(SRC, int, posY) + ((LookupSine(a) * d) >> 12);
      SubtractVector(vec, g_anSpyroWorldPos, vec);
      {
        int dir = ArcTan2(vec[0], vec[1], 0);

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
      g_nSpyroAnimPlayRate_blk[0] = VectorLength(vec, 0) >> 1;
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
      d = g_nGemPickupCameraStartRadius +
          (((g_nGemPickupCameraTargetRadius - g_nGemPickupCameraStartRadius) *
            t) >>
           13);
      g_anCameraPos[0] =
          ACTOR_RAW(SRC, int, posX) + ((LookupCosine(a) * d) >> 12);
      g_anCameraPos[1] =
          ACTOR_RAW(SRC, int, posY) + ((LookupSine(a) * d) >> 12);
      g_anCameraPos[2] =
          g_nGemPickupCameraStartZ +
          (((g_nGemPickupCameraTargetZ - g_nGemPickupCameraStartZ) * t) >> 13);
      {
        int land = g_nGemPickupSweepDurationFrames - 0x10;

        if (g_nGemPickupTimer >= land &&
            g_nGemPickupTimer - g_nFrameStep < land) {
          g_bSpyroFramePrev = g_bSpyroAnimFrame;
          g_bSpyroAnimCurrent = 3;
          g_bSpyroAnimFrame = 0;
          g_bSpyroSubFrameTimer = 0;
          g_nSpyroAnimPlayRate = 0xC;
        }
      }
    } else if (g_nGemPickupTimer < g_nGemPickupSweepDurationFrames + 0x20) {
      if (g_nGemPickupTimer - g_nFrameStep < g_nGemPickupSweepDurationFrames) {
        g_anSpyroWorldPos[0] = ACTOR_RAW(SRC, int, posX) +
                               ((LookupCosine(g_dwGemPickupOrbitTargetYaw) *
                                 g_nGemPickupOrbitTargetRadius) >>
                                12);
        {
          int s = LookupSine(g_dwGemPickupOrbitTargetYaw) *
                  g_nGemPickupOrbitTargetRadius;
          char *src = SRC;
          int y = ACTOR_RAW(src, int, posY);

          g_nGemPickupOrbitStartYaw = g_abSpyroPersistentEuler[2] << 4;
          g_dwGemPickupOrbitTargetYaw =
              (g_dwGemPickupOrbitTargetYaw + 0x800) & 0xFFF;
          g_nSpyroWorldPosY = y + (s >> 12);
          CopyVector(g_anCameraPos, REC(src));
        }
      }
      if (((OTY - g_nGemPickupOrbitStartYaw) & 0xFFF) < 0x801) {
        a = g_nGemPickupOrbitStartYaw +
            (AbsAngleDelta12(OTY, g_nGemPickupOrbitStartYaw) *
             (g_nGemPickupTimer - g_nGemPickupSweepDurationFrames)) /
                32;
      } else {
        a = g_nGemPickupOrbitStartYaw -
            (AbsAngleDelta12(OTY, g_nGemPickupOrbitStartYaw) *
             (g_nGemPickupTimer - g_nGemPickupSweepDurationFrames)) /
                32;
      }
      g_abSpyroPersistentEuler[2] = a >> 4;
      g_nSpyroAnimPlayRate = 8;
    } else {
      g_nGemPickupSubstate = 2;
      g_bSpyroSubFrameTimer = 4;
      g_nGemPickupTimer = 0;
      g_bSpyroAnimCurrent = 0;
      g_bSpyroAnimFrame = 0;
      g_nSpyroAnimPlayRate = 4;
      g_abSpyroPersistentEuler[2] = (int)g_dwGemPickupOrbitTargetYaw >> 4;
    }
    if (g_nGemPickupSubstate == 1) {

      AddVector(vec, g_anSpyroWorldPos, &((Actor *)SRC)->posX);
      RShiftVector3(vec, 1);
      SubtractVector(vec, vec, g_anCameraPos);
      if (g_nGemPickupTimer < 0x10) {
        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, 0), -vec[2], 1);
        ang[2] = ArcTan2(vec[0], vec[1], 1);
        g_nCameraEulerRoll = (g_nCameraEulerRoll +
                              ((SignedAngleDelta12(ang[0], g_nCameraEulerRoll) *
                                g_nGemPickupTimer) >>
                               4)) &
                             0xFFF;
        g_nCameraEulerPitch =
            (g_nCameraEulerPitch +
             ((SignedAngleDelta12(ang[1], g_nCameraEulerPitch) *
               g_nGemPickupTimer) >>
              4)) &
            0xFFF;
        g_nCameraEulerYaw = (g_nCameraEulerYaw +
                             ((SignedAngleDelta12(ang[2], g_nCameraEulerYaw) *
                               g_nGemPickupTimer) >>
                              4)) &
                            0xFFF;
      } else if (g_nGemPickupTimer < g_nGemPickupSweepDurationFrames - 0x20) {
        g_nCameraEulerRoll = 0;
        g_nCameraEulerPitch = ArcTan2(VectorLength(vec, 0), -vec[2], 1);
        g_nCameraEulerYaw = ArcTan2(vec[0], vec[1], 1);
      } else {
        int *rec = REC(SRC);

        ang[0] = 0;
        ang[1] = ArcTan2(VectorLength(vec, 0), -vec[2], 1);
        ang[2] = ArcTan2(vec[0], vec[1], 1);
        g_nCameraEulerRoll =
            (ang[0] +
             ((SignedAngleDelta12(W32(rec, 0xc), ang[0]) *
               (g_nGemPickupTimer - g_nGemPickupSweepDurationFrames + 0x20)) >>
              6)) &
            0xFFF;
        g_nCameraEulerPitch =
            (ang[1] +
             ((SignedAngleDelta12(W32(rec, 0x10), ang[1]) *
               (g_nGemPickupTimer - g_nGemPickupSweepDurationFrames + 0x20)) >>
              6)) &
            0xFFF;
        g_nCameraEulerYaw =
            (ang[2] +
             ((SignedAngleDelta12(W32(rec, 0x14), ang[2]) *
               (g_nGemPickupTimer - g_nGemPickupSweepDurationFrames + 0x20)) >>
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
      g_abSpyroPersistentEuler[0] =
          -ArcTan2(IntegerSqrt(vec[0] * vec[0] + vec[2] * vec[2]), vec[1], 1);
      g_abSpyroPersistentEuler[1] = -ArcTan2(vec[2], vec[0], 1);
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      g_nSpyroWorldPosZ += 0x400;
      g_nSpyroGroundHeightZ = FindWorldFloorBelowPoint(g_anSpyroWorldPosBlock);
      g_nSpyroWorldPosZ = g_nSpyroGroundHeightZ + 0x164;
      SampleSpyroShadowRingHeights(0);
    }
    break;
  case 2: {
    char *s = SRC;

    ACTOR_RAW(s, unsigned char, subMode) =
        (ACTOR_RAW(s, unsigned char, subMode) + 1) & 3;
  }
    {
      char *s = SRC;

      ACTOR_RAW(s, unsigned char, rotX) =
          g_abGemPickupFlickerPairs2[ACTOR_RAW(s, unsigned char, subMode) * 2];
    }
    {
      char *s = SRC;

      ACTOR_RAW(s, unsigned char, rotY) =
          g_abGemPickupFlickerPairs2Y[ACTOR_RAW(s, unsigned char, subMode) * 2];
    }
    if (g_nPulseRumbleTimer < 8) {
      g_nPulseRumbleTimer = 8;
    }
    g_nPulseRumbleAmount = 0x46;
    ((void (*)(int, void *))g_pfnLevelSpawnActor)(0xFB,
                                                  g_pGemPickupSourceActor);
    if (g_nGemPickupTimer < 0x10 || g_nGemPickupStreamState < 3) {
      AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
    } else {
      g_nGemPickupSubstate = 3;
      g_nGemPickupTimer = 0;
      {
        int n;
        int five;

        for (n = 0; n < 0x20; n++) {
          ((void (*)(int, void *))g_pfnLevelSpawnActor)(
              0xFB, g_pGemPickupSourceActor);
        }
        __asm__("" : "=r"(five) : "0"(5));
        GB[24] = (int)AllocActorRecordSlot();
        InitActorRenderDefaults((void *)GB[24]);
        ACTOR_RAW(STATUE, short, type) = 0x1FE;
        CopyVector(&((Actor *)GB[24])->posX, &((Actor *)SRC)->posX);
        {
          char *s = SRC;
          char *t = STATUE;

          ACTOR_RAW(t, unsigned char, rotZ) = ACTOR_RAW(s, unsigned char, rotZ);
        }
        ACTOR_RAW(STATUE, unsigned char, scale) = 0x60;
        ACTOR_RAW(STATUE, unsigned char, otBias) = five;
        ACTOR_RAW(STATUE, int, groundCode) = -1;
        ACTOR_RAW(STATUE, int, posZ) += 0x400;
        FindWorldFloorBelowPoint(&((Actor *)GB[24])->posX);
        ACTOR_RAW(STATUE, int, posZ) -= 0x400;
        EncodeCachedVecToActorDirCode((void *)GB[24]);
        InsertActorIntoSpatialGrid((void *)GB[24]);
        g_pGemPickupSpyroMirrorActor = AllocActorRecordSlot();
        InitActorRenderDefaults(g_pGemPickupSpyroMirrorActor);
        ACTOR_RAW(MIRROR, short, type) = 0x1FF;
        CopyVector(&((Actor *)MIRROR)->posX, g_anSpyroWorldPos);
        ACTOR_RAW(MIRROR, unsigned char, otBias) = five;
        InsertActorIntoSpatialGrid(MIRROR);
        StopSoundVoiceByIndex(GB[11], 4);
        PlaySoundEffect(LEVEL_SFX(0x31), 0, 0x10, &GB[11]);
        ACTOR_RAW(SRC, int, posZ) += 0x78;
        DespawnActorRecord(SRC);
      }
    }
    break;
  case 3:
    if (GB[5] < 0x18) {
      ACTOR_RAW(STATUE, unsigned char, scale) = 0x60 - GB[5] * 3;
      {
        char *s = SRC;
        int turn;

        turn = SignedAngleDelta8(W32(REC(s), 0x30) >> 4,
                                 ACTOR_RAW(s, unsigned char, rotZ)) *
               GB[5] / 24;
        {
          char *s2 = SRC;
          char *t = STATUE;

          ACTOR_RAW(t, unsigned char, rotZ) =
              ACTOR_RAW(s2, unsigned char, rotZ) + turn;
        }
      }
    } else {
      char *s = SRC;
      char *t;

      g_nGemPickupSubstate = 4;
      GB[5] = 0;
      t = STATUE;
      ACTOR_RAW(t, unsigned char, rotZ) = W32(REC(s), 0x30) >> 4;
      ACTOR_RAW(STATUE, unsigned char, scale) = 0;
      ACTOR_RAW(MIRROR, unsigned char, rotX) = g_abSpyroPersistentEuler[0];
      ACTOR_RAW(MIRROR, unsigned char, rotY) = g_abSpyroPersistentEuler[1];
      ACTOR_RAW(MIRROR, unsigned char, rotZ) = g_abSpyroPersistentEuler[2];
    }
    break;
  case 4: {
    int save[5];
    int step;

    done = 0;
    if (GB[5] < g_nGemPickupCameraPathCount * 2) {
      if (g_nGemPickupAudioFiredFlag == 0 && GB[5] >= W32(REC(SRC), 0x40)) {
        CopyWords(save, *(void *volatile *)g_apSampleBankEntriesBlock, 0x14);
        SBE->addr = 0x80000 - g_nGemPickupAudioSampleSize;
        SBE->loop = -1;
        SBE->vol = 0x32;
        {
          int rate = (W32(REC(SRCS), 0x3c) << 10) / 11025;

          SBE->rate = rate;
        }
        SBE->h0 = 0;
        SBE->h1 = 0;
        SBE->w4 = 0;
        ((struct { int v; } *)&g_nSfxOverrideFlags)->v = 1;
        ((struct { short v; } *)&g_nSfxOverrideVolL)->v = 0x3FFF;
        ((struct { short v; } *)&g_nSfxOverrideVolR)->v = 0x3FFF;
        StopSoundVoiceByIndex(GB[11], 4);
        PlaySoundEffect(0, 0, 0x10, &GB[11]);
        CopyWords(*(void *volatile *)g_apSampleBankEntriesBlock, save, 0x14);
        g_nGemPickupAudioFiredFlag = 1;
      }
      {
        int *p = (int *)((g_nGemPickupTimer >> 1) * 0x18 +
                         (int)g_pGemPickupCameraPath);

        g_anCameraPos[0] = CAM_KEY_RAW(p, x);
        g_anCameraPos[1] = CAM_KEY_RAW(p, y);
        g_anCameraPos[2] = CAM_KEY_RAW(p, z);
        g_nCameraEulerRoll = CAM_KEY_RAW(p, roll);
        g_nCameraEulerPitch = CAM_KEY_RAW(p, pitch);
        g_nCameraEulerYaw = CAM_KEY_RAW(p, yaw);
      }
      FRAME_STEP(g_pGemPickupStatueModelData, step);
      ACTOR_RAW(STATUE, unsigned char, frame) = GB[5] / step;
      {
        char *p = STATUE;

        ACTOR_RAW(p, unsigned char, nextFrame) =
            ACTOR_RAW(p, unsigned char, frame) + 1;
      }
      ACTOR_RAW(STATUE, unsigned char, frameProgress) =
          (GB[5] % step << 6) / step;
      {
        short n = ANIM_HEADER_RAW(FIRST_ANIM(g_pGemPickupStatueModelData),
                                  short, m_NumFrames);
        char *p = STATUE;

        if (ACTOR_RAW(p, unsigned char, nextFrame) >= n) {
          ACTOR_RAW(p, unsigned char, frame) = n - 1;
          ACTOR_RAW(STATUE, unsigned char, frameProgress) = 0;
        }
      }
      FRAME_STEP(g_pGemPickupMirrorModelData, step);
      ACTOR_RAW(MIRROR, unsigned char, frame) = GB[5] / step;
      {
        char *p = MIRROR;

        ACTOR_RAW(p, unsigned char, nextFrame) =
            ACTOR_RAW(p, unsigned char, frame) + 1;
      }
      ACTOR_RAW(MIRROR, unsigned char, frameProgress) =
          (GB[5] % step << 6) / step;
      {
        short n = ANIM_HEADER_RAW(FIRST_ANIM(g_pGemPickupMirrorModelData),
                                  short, m_NumFrames);
        char *p = MIRROR;

        if (ACTOR_RAW(p, unsigned char, nextFrame) >= n) {
          ACTOR_RAW(p, unsigned char, frame) = n - 1;
          ACTOR_RAW(MIRROR, unsigned char, frameProgress) = 0;
        }
      }
      if (g_dwPadPressed & 0x40) {
        if (W32(REC(SRC), 0x34) != 0) {
          if (g_nGemPickupPadHoldTimer == 0) {
            g_nGemPickupPadHoldTimer = 1;
          }
        } else {
          done = 1;
        }
      }
    }
    if (done != 0 || g_nGemPickupTimer >= g_nGemPickupCameraPathCount * 2) {
      int *p;

      g_nGemPickupSubstate = 5;
      g_nGemPickupTimer = 0;
      p = (int *)(g_nGemPickupCameraPathCount * 0x18 +
                  (int)g_pGemPickupCameraPath);
      g_anCameraPos[0] = CAM_KEY_RAW((CamPathKey *)p - 1, x);
      g_anCameraPos[1] = CAM_KEY_RAW((CamPathKey *)p - 1, y);
      g_anCameraPos[2] = CAM_KEY_RAW((CamPathKey *)p - 1, z);
      g_nCameraEulerRoll = CAM_KEY_RAW((CamPathKey *)p - 1, roll);
      g_nCameraEulerPitch = CAM_KEY_RAW((CamPathKey *)p - 1, pitch);
      g_nCameraEulerYaw = CAM_KEY_RAW((CamPathKey *)p - 1, yaw);
      ACTOR_RAW(STATUE, unsigned char, frame) =
          ANIM_HEADER_RAW(FIRST_ANIM(g_pGemPickupStatueModelData),
                          unsigned char, m_NumFrames) -
          1;
      ACTOR_RAW(STATUE, unsigned char, frameProgress) = 0;
      ACTOR_RAW(MIRROR, unsigned char, frame) =
          ANIM_HEADER_RAW(FIRST_ANIM(g_pGemPickupMirrorModelData),
                          unsigned char, m_NumFrames) -
          1;
      ACTOR_RAW(MIRROR, unsigned char, frameProgress) = 0;
      g_bSpyroAnimPrev = 0;
      g_bSpyroAnimCurrent = 0;
      g_bSpyroFramePrev = 0;
      g_bSpyroAnimFrame = 1;
      g_bSpyroSubFrameTimer = 0;
      g_nSpyroAnimPlayRate = 4;
      TickSpyroAnimLayer1();
      TickSpyroAnimLayer2();
      StopSoundVoiceByIndex(*(int *)&g_bGemPickupSfxVoiceMarker, 2);
      TickSpuPerFrame();
      if (g_nGemPickupSplitFlag == 0) {
        SetSpuTransferStartAddr(0x80000 - g_nGemPickupAudioSampleSize);
        WriteSpuRam((char *)g_pWorldUnpackHead + g_nGemPickupAudioSampleOffset,
                    g_nGemPickupAudioSampleSize);
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
  case 5: {
    int *icon;
    int k;
    int off;

    for (i = 0; i < ANIM_HEADER_RAW(FIRST_ANIM(g_pGemPickupStatueModelData),
                                    short, m_NumColors);
         i++) {
      unsigned int c = ComputeDepthCuedColor(
          ((int *)ANIM_HEADER_RAW(FIRST_ANIM(g_pGemPickupStatueModelData), int,
                                  m_Colors))[i],
          0xFFFFFF, 0x200);
      int *dst = (int *)(i * 4 + ANIM_HEADER_RAW(
                                     FIRST_ANIM(g_pGemPickupStatueModelData),
                                     int, m_Colors));

      *dst = c;
    }
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    i = 0;
    done = (int)g_anHudRollInStepTable; /* shares s4 with case 4 */
    icon = &HUD_ICON(5 * ACTOR_SIZE)->posY;
    k = 0;
    off = 0;
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
  roll_in:
    CopyVector(&HUD_ICON(5 * ACTOR_SIZE + k)->posX,
               (int *)((char *)g_anGemPickupIconRestPos + off));
    if (g_nGemPickupTimer < 0x19) {
      *icon += (((short *)done)[g_nGemPickupTimer >> 1] * 3) >> 1;
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
      g_nGemPickupSubstate = 6;
      GB[5] = 0;
      g_bHudDragonRollPhase = 0;
      CopyVector((int *)ang, &((Actor *)g_pGemPickupStatueActor)->posX);
      ((int *)ang)[2] += 0x40;
      ((void (*)(int, int, int *, int))g_pfnLevelEmitParticles)(0x20, 0x18,
                                                                (int *)ang, 0);
      CopyVector(g_anGemPickupAuraStarAnchor, (int *)ang);
      DespawnActorRecord(STATUE);
      DespawnActorRecord(MIRROR);
      PlaySoundEffect(LEVEL_SFX(0x34), 0, 0x10, &GB[11]);
    }
  } break;
  case 6:
    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    if (g_anHudDragonCachedCountBlock[0] != g_nWorldDragonsRescued ||
        g_bHudDragonRollPhase != 0) {
      g_bHudDragonRollPhase += 0x10;
      SetHudCounterDigits(5, 2, g_anHudDragonCachedCountBlock[0] + 1, 0);
      CommitHudCounterAnimState(5, 2, g_bHudDragonRollPhase);
      if (g_bHudDragonRollPhase == 0xC0) {
        g_anHudDragonCachedCountBlock[0]++;
      }
      if (g_bHudDragonRollPhase == 0) {
        PlaySoundEffect(LEVEL_SFX(0x35), 0, 0x10, &g_bGemPickupSfxVoiceMarker);
      }
    }
    if (GB[5] >= 0x60 && g_nGemPickupStreamState == 6) {
      g_nGemPickupSubstate = 7;
      GB[5] = 0;
      g_pEmitListWriteCursor = g_pEmitListBase;
      ((unsigned char *)g_pEmitListBase)[1] = 0xFF;
      PlaySoundEffect(LEVEL_SFX(0x33), 0, 0x10, &GB[11]);
    }
    break;
  case 7: {
    int k;

    AdvanceSpyroAnimFrame(g_nSpyroAnimPlayRate);
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2();
    if (g_nGemPickupTimer < 0x10) {
      int n;
      int *icon;
      int j;
      int off;

      n = 0;
      icon = &HUD_ICON(5 * ACTOR_SIZE)->posY;
      j = 0;
      off = 0;
    roll_out:
      CopyVector(&HUD_ICON(5 * ACTOR_SIZE + j)->posX,
                 (int *)((char *)g_anGemPickupIconRestPos + off));
      j += ACTOR_SIZE;
      off += 0xC;
      n++;
      *icon += (0x11 - g_nGemPickupTimer) * 3;
      icon += 0x16;
      if (n < 3) {
        goto roll_out;
      }
    }
    if (g_nGemPickupTimer < 0x10) {
      k = g_nGemPickupTimer;
    } else {
      k = 0x20 - g_nGemPickupTimer;
    }
    g_nGemPickupFullscreenTintLevel = k * 0x10 - 1;
    g_anGemPickupAuraStarBlock[0] = 1;
    g_bGemPickupAuraStarColor = k * 8 + 0x40;
    g_nGemPickupAuraStarRadius = k * 0x50;
    g_abGemPickupAuraStarEuler[2] += 8;
    if (g_nGemPickupTimer >= 0x20) {
      g_anGemPickupAuraStarBlock[0] = 0;
      EndGemPickupToWorld();
    }
  } break;
  }
  ((void (*)(int))g_pfnLevelAdvanceEmitList)(g_nFrameStep);
  if (g_nGemPickupSubstate > 0) {
    TickGemPickupAssetStream();
  }
  {
    char *actor = g_pActorListBase;

    while (ACTOR_RAW(actor, unsigned char, mode) != 0xFF) {
      if (ACTOR_RAW(actor, short, type) == 0xFB &&
          ACTOR_RAW(actor, unsigned char, mode) < 0x80) {
        TickGemPickupBurstEntity(actor);
      }
      actor += ACTOR_SIZE;
    }
  }
}
