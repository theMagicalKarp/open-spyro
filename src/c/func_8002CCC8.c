#include "globals.h"

extern int PlaySoundEffect(unsigned int sample, int a, unsigned int b, void *c);
extern void ResetSpyroEntity(int hard);
extern int ArcTan2(int y, int x, int high_precision);
extern void SubtractVector(int *dst, int *a, int *b);
extern unsigned int VectorLength(int *vec, int include_z);
extern int AbsAngleDelta12(int a, int b);

/* alias block as an INT array — +0 anim-prev byte, -6 words
   g_anSpyroWorldPos[0], -4 words g_anSpyroWorldPos[2]; the whole block must
   come off one held base. */
extern int g_abSpyroAnimPrevBlock[];
extern int g_nSpyroWorldPosY; /* scalar alias — g_anSpyroWorldPos[1] */
extern int D_80078D14;
extern int D_80078D2C;
extern unsigned int D_80078D30;
extern int D_80078D34;
extern int D_80078D38;
extern int D_80078D3C;
extern int D_80078D40;
extern int D_80078D44;
extern unsigned int D_80078D48;
extern int D_80078D4C;
extern int D_80078D50;
extern int D_80078D54;
extern unsigned int D_80078D58;
extern int D_80078D5C;
extern int D_80078D60;
extern int D_80078D64;
extern int D_80078D68;
extern int D_80078D6C;
extern int D_80078D70;
extern int D_80078D74;

/* 0x8002ccc8 (868 bytes) — enter the save-menu gamestate (0xB) from the world,
   snapshotting the scene around the actor the player interacted with.  Freezes
   gameplay, picks the initial cursor (0 when D_80078D14 is set, else 2), plays
   the level's menu sound, resets Spyro and forces anim state 1, then captures
   the yaw and distance from Spyro to the actor's pickup target, to the actor
   itself, to the level-overlay init actor (when present) and to the camera into
   the D_80078D2C..D70 scripted-camera block, and seeds the menu pan targets.

   The g_abSpyroAnimPrevBlock alias is an incomplete INT
   array reached by element index, and the empty do/while right after the
   offset-0 `sb` that materialises its base ends cse's ebb so the -24/-16
   accesses stay off the held base (A200).  Everything the original emits above
   that store (the yaw load) is computed above it in source (A253), and the
   four byte stores below it are volatile so their order is pinned.  The second
   barrier keeps the -16 read below the batch-read D_80078D34/38 stores.
   `rec` is built in int arithmetic so the addu keeps idx*88 as its first
   operand (A149).  The rec+0x10 read goes through a `$2` register local:
   as a pseudo it is the subu's first dying input and local-alloc ties the
   ArcTan2 y argument to it; as a hard register the tie falls to posY, which
   then lands in $a1 as in the original. */
void func_8002CCC8(int actor) {
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
  if (D_80078D14 != 0) {
    g_nSaveMenuCursor = 0;
  } else {
    g_nSaveMenuCursor = 2;
  }
  D_80078D74 = actor;
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
  D_80078D6C = yaw;
  rec = (char *)(*(int *)(owner + 4) * 88 + (int)g_pActorListBase);
  {
    register int ry asm("$2");
    ry = *(int *)(rec + 0x10);
    D_80078D70 =
        ArcTan2(*(int *)(rec + 0xC) - g_abSpyroAnimPrevBlock[-6], ry - py, 1);
  }
  D_80078D2C = ArcTan2(*(int *)(actor + 0xC) - g_abSpyroAnimPrevBlock[-6],
                       *(int *)(actor + 0x10) - g_nSpyroWorldPosY, 1);
  SubtractVector(vec, (int *)(actor + 0xC), &g_abSpyroAnimPrevBlock[-6]);
  D_80078D30 = VectorLength(vec, 0);
  z = *(int *)(actor + 0x14);
  yw = D_80078D70;
  D_80078D3C = 0x480;
  D_80078D34 = z;
  D_80078D38 = yw;
  do {
  } while (0);
  D_80078D40 = g_abSpyroAnimPrevBlock[-4] + 0x170;
  if (g_pLevelOverlayInitRecord != 0) {
    D_80078D44 = ArcTan2(*(int *)((char *)g_pLevelOverlayInitRecord + 0xC) -
                             g_abSpyroAnimPrevBlock[-6],
                         *(int *)((char *)g_pLevelOverlayInitRecord + 0x10) -
                             g_nSpyroWorldPosY,
                         1);
    SubtractVector(vec, (int *)((char *)g_pLevelOverlayInitRecord + 0xC),
                   &g_abSpyroAnimPrevBlock[-6]);
    D_80078D48 = VectorLength(vec, 0);
    D_80078D50 = 0x600;
    D_80078D4C = (D_80078D70 + 0x800) & 0xFFF;
  }
  D_80078D54 = ArcTan2(g_anCameraPos[0] - g_abSpyroAnimPrevBlock[-6],
                       g_anCameraPos[1] - g_nSpyroWorldPosY, 1);
  SubtractVector(vec, g_anCameraPos, &g_abSpyroAnimPrevBlock[-6]);
  D_80078D58 = VectorLength(vec, 0);
  g_nSaveMenuXOffset = 0xB0;
  D_80078D5C = g_anCameraPos[2];
  camYaw = (D_80078D70 + 0x30B) & 0xFFF;
  D_80078D60 = camYaw;
  if (AbsAngleDelta12(D_80078D54, camYaw) > 0x400) {
    g_nSaveMenuXOffset = 0;
    D_80078D60 = (D_80078D70 - 0x30B) & 0xFFF;
  }
  D_80078D64 = 0x4C9;
  D_80078D68 = g_abSpyroAnimPrevBlock[-4] + 0x56B;
}
