#include "globals.h"

extern void RotateVectorByMatrix();
extern void AddVector(int *dst, int *a, int *b);
extern void CopyVector(int *dst, int *src);
extern int FindGroundHeightBelow(int *pt, int range);

/* the matrix symbol itself as an incomplete array: the matrix pointer, the
   -0x34 (== -13 words) g_anSpyroWorldPos AddVector argument and the +0x13C
   (== +0x4F words) trigger read all come off one held base. */
extern int g_anSpyroBodyMtx[];
/* shared-base view of g_nSpyroShadowRingIndex (0x8007aa30): the index RMW plus
   the -0x18 no-ground-flag and -0x20 height byte rings. */
extern int g_anSpyroShadowRingIndexBlock[];
/* sized-array alias of g_nSpyroShadowHiddenFlag (0x8007aa34) — see A134: only
   an array lvalue keeps the death-plane table load below this store. */
extern int g_anSpyroShadowHidden[1];

/* 0x80049fac (596 bytes) — per-tick Spyro shadow-ring sampler. Computes the
   shadow centre ((0,0,-0x164) through the body matrix + world pos) into
   g_anSpyroShadowPos with Z = ground height, refreshes the shadow OT-bin
   biases and the hidden flag, then advances the 8-slot ring index and probes
   ONE ring vertex: ground within +/-0x80 of the probe Z stores the scaled
   height (diff * 0xC2 / 0x200), else raises the no-ground flag. probe == 0
   clears both slots without sampling. Consumed by DrawSpyroDropShadow. */
void SampleSpyroShadowRingHeights(int probe) {
  int v[3];
  int *ring;
  int gz;
  int idx;
  int ground;
  int d;

  v[0] = 0;
  v[1] = 0;
  v[2] = -0x164;
  RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
  AddVector(v, v, g_anSpyroBodyMtx - 13);
  CopyVector(g_anSpyroShadowPos, v);
  gz = g_nSpyroGroundHeightZ;
  g_anSpyroShadowPos[2] = gz;
  if (g_bSpyroOtBinBias < 0x7F) {
    g_bSpyroOtBinBias = 5;
  }
  g_nSpyroShadowOtBinBias = 3;
  if ((g_anSpyroWorldPos[2] - gz) >= 0x365) {
    g_nSpyroShadowOtBinBias = 5;
  }
  /* The empty loop is load-bearing: its NOTE_INSN_LOOP_BEG ends cse's extended
     basic block, so the held matrix base below has no constant equivalence and
     the +0x13C read keeps the `lw v0,0x13C(s0)` register form instead of
     folding to an absolute address (see the cookbook's B-i fold). */
  do {
  } while (0);
  g_anSpyroShadowHidden[0] = 0;
  if ((g_anLevelDeathPlaneZTable[g_nLevelIntroIndex] >= gz) ||
      (g_anSpyroBodyMtx[0x4F] != 0)) {
    g_anSpyroShadowHidden[0] = 1;
  }
  ring = g_anSpyroShadowRingIndexBlock;
  idx = (ring[0] + 1) & 7;
  ring[0] = idx;
  if (probe != 0) {
    RotateVectorByMatrix(g_anSpyroBodyMtx,
                         &g_anSpyroShadowProbeOffsetTable[idx * 3], v);
    AddVector(v, v, g_anSpyroBodyMtx - 13);
    v[2] += 0x200;
    ground = FindGroundHeightBelow(v, 0x400);
    v[2] -= 0x200;
    if (((unsigned int)((v[2] - ground) + 0x80)) >= 0x100) {
      char *flags = (char *)ring - 0x18;
      *((unsigned char *)(g_nSpyroShadowRingIndex + (int)flags)) = 1;
    } else {
      char *flags = (char *)ring - 0x18;
      char *heights = (char *)ring - 0x20;
      *((unsigned char *)(g_nSpyroShadowRingIndex + (int)flags)) = 0;
      d = (g_anSpyroShadowPos[2] - ground) * 0xC2;
      *((unsigned char *)(g_nSpyroShadowRingIndex + (int)heights)) = d / 0x200;
      return;
    }
  } else {
    char *flags = (char *)ring - 0x18;
    *((unsigned char *)(g_nSpyroShadowRingIndex + (int)flags)) = 0;
  }
  {
    char *heights = (char *)ring - 0x20;
    *((unsigned char *)(g_nSpyroShadowRingIndex + (int)heights)) = 0;
  }
}
