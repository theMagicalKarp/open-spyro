#include "globals.h"
#include "main_shared.h"

extern int PlaySoundEffect(unsigned int sample, int a, unsigned int b, void *c);
extern void ResetSpyroEntity(int hard);
extern unsigned int VectorLength(int *vec, int include_z);
extern int AbsAngleDelta12(int a, int b);

/* alias block as an INT array — +0 anim-prev byte, -6 words
   g_anSpyroWorldPos[0], -4 words g_anSpyroWorldPos[2]; the whole block must
   come off one held base. */
extern int g_abSpyroAnimPrevBlock[];
extern int g_nSaveMenuDragonFromAngle;
extern unsigned int g_nSaveMenuDragonFromDist;
extern unsigned int g_nSaveMenuOverlayFromDist;
extern int g_nSaveMenuCameraFromAngle;
extern unsigned int g_nSaveMenuCameraFromDist;
extern int g_nSaveMenuSpyroFromYaw;
extern int g_pSaveMenuDragonActor;

/* 0x8002ccc8 (868 bytes) — enter the save-menu gamestate (0xB) from the world,
   snapshotting the scene around the actor the player interacted with.  Freezes
   gameplay, picks the initial cursor (0 when g_nSaveSlotLoaded is set, else 2),
   plays the level's menu sound, resets Spyro and forces anim state 1, then
   captures the yaw and distance from Spyro to the actor's pickup target, to the
   actor itself, to the level-overlay init actor (when present) and to the
   camera into the g_nSaveMenu*From/To keyframe block (0x80078D2C..0x80078D74:
   polar angle/distance/z about Spyro for the dragon, the overlay record and
   the camera, plus Spyro's from/to yaw), and seeds the menu pan targets.

   The g_abSpyroAnimPrevBlock alias is an incomplete INT
   array reached by element index, and the empty do/while right after the
   offset-0 `sb` that materialises its base ends cse's ebb so the -24/-16
   accesses stay off the held base (A200).  Everything the original emits above
   that store (the yaw load) is computed above it in source (A253), and the
   four byte stores below it are volatile so their order is pinned.  The second
   barrier keeps the -16 read below the batch-read g_nSaveMenuDragonFromZ /
   g_nSaveMenuDragonToAngle stores. `rec` is built in int arithmetic so the addu
   keeps idx*88 as its first operand (A149).  The rec+0x10 read goes through a
   `$2` register local: as a pseudo it is the subu's first dying input and
   local-alloc ties the ArcTan2 y argument to it; as a hard register the tie
   falls to posY, which then lands in $a1 as in the original. */
void BeginSaveMenuFromWorld(int actor) {
  int vec[3];
  int owner;
  char *rec;
  int camYaw;
  int yaw;
  int py;
  int z;
  int yw;

  owner = *(int *)actor;
  g_nGamestate = 0xB;
  g_nGameplayBlocked = 1;
  g_nGamePaused = 1;
  g_nSaveMenuMode = 0;
  g_nSaveMenuAnimPhase = 0;
  g_nSaveMenuSubstate = 0;
  if (g_nSaveSlotLoaded != 0) {
    g_nSaveMenuCursor = 0;
  } else {
    g_nSaveMenuCursor = 2;
  }
  g_pSaveMenuDragonActor = actor;
  g_nSpyroFlameBreathTimerSave = g_nSpyroFlameBreathTimer;
  PlaySoundEffect(*(unsigned char *)((char *)g_pLevelSampleBankHeader + 0x2C),
                  0, 0x10, 0);
  ResetSpyroEntity(1);
  yaw = g_nSpyroBodyYaw;
  *(unsigned char *)g_abSpyroAnimPrevBlock = 1;
  do {
  } while (0);
  py = g_nSpyroWorldPosY;
  *(volatile unsigned char *)&g_bSpyroAnimCurrent = 1;
  *(volatile unsigned char *)&g_bSpyroFramePrev = 0;
  *(volatile unsigned char *)&g_bSpyroAnimFrame = 1;
  *(volatile unsigned char *)&g_bSpyroSubFrameTimer = 0;
  g_nSaveMenuSpyroFromYaw = yaw;
  rec = (char *)(*(int *)(owner + 4) * 88 + (int)g_pActorListBase);
  {
    register int ry asm("$2");
    ry = *(int *)(rec + 0x10);
    g_nSaveMenuSpyroToYaw =
        ArcTan2(*(int *)(rec + 0xC) - g_abSpyroAnimPrevBlock[-6], ry - py, 1);
  }
  g_nSaveMenuDragonFromAngle =
      ArcTan2(*(int *)(actor + 0xC) - g_abSpyroAnimPrevBlock[-6],
              *(int *)(actor + 0x10) - g_nSpyroWorldPosY, 1);
  SubtractVector(vec, (int *)(actor + 0xC), &g_abSpyroAnimPrevBlock[-6]);
  g_nSaveMenuDragonFromDist = VectorLength(vec, 0);
  z = *(int *)(actor + 0x14);
  yw = g_nSaveMenuSpyroToYaw;
  g_nSaveMenuDragonToDist = 0x480;
  g_nSaveMenuDragonFromZ = z;
  g_nSaveMenuDragonToAngle = yw;
  do {
  } while (0);
  g_nSaveMenuDragonToZ = g_abSpyroAnimPrevBlock[-4] + 0x170;
  if (g_pLevelOverlayInitRecord != 0) {
    g_nSaveMenuOverlayFromAngle = ArcTan2(
        *(int *)((char *)g_pLevelOverlayInitRecord + 0xC) -
            g_abSpyroAnimPrevBlock[-6],
        *(int *)((char *)g_pLevelOverlayInitRecord + 0x10) - g_nSpyroWorldPosY,
        1);
    SubtractVector(vec, (int *)((char *)g_pLevelOverlayInitRecord + 0xC),
                   &g_abSpyroAnimPrevBlock[-6]);
    g_nSaveMenuOverlayFromDist = VectorLength(vec, 0);
    g_nSaveMenuOverlayToDist = 0x600;
    g_nSaveMenuOverlayToAngle = (g_nSaveMenuSpyroToYaw + 0x800) & 0xFFF;
  }
  g_nSaveMenuCameraFromAngle =
      ArcTan2(g_anCameraPos[0] - g_abSpyroAnimPrevBlock[-6],
              g_anCameraPos[1] - g_nSpyroWorldPosY, 1);
  SubtractVector(vec, g_anCameraPos, &g_abSpyroAnimPrevBlock[-6]);
  g_nSaveMenuCameraFromDist = VectorLength(vec, 0);
  g_nSaveMenuXOffset = 0xB0;
  g_nSaveMenuCameraFromZ = g_anCameraPos[2];
  camYaw = (g_nSaveMenuSpyroToYaw + 0x30B) & 0xFFF;
  g_nSaveMenuCameraToAngle = camYaw;
  if (AbsAngleDelta12(g_nSaveMenuCameraFromAngle, camYaw) > 0x400) {
    g_nSaveMenuXOffset = 0;
    g_nSaveMenuCameraToAngle = (g_nSaveMenuSpyroToYaw - 0x30B) & 0xFFF;
  }
  g_nSaveMenuCameraToDist = 0x4C9;
  g_nSaveMenuCameraToZ = g_abSpyroAnimPrevBlock[-4] + 0x56B;
}
