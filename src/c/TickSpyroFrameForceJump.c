#include "globals.h"

extern void SnapshotPadInputState();
extern void ChangeSpyroState(int state);
extern void ZeroVector();
extern void IntegrateSpyroMotionForSubstep(int substep);
extern void AdvanceSpyroSubstepState(void);
extern void RShiftVector3();
extern void CopyVector();
extern void AddVector();
extern void TickSpyroAnimStateMachine(void);
extern void TickSpyroHornStrikeAttack(void);
extern void TickSpyroAnimLayer1(void);
extern void TickSpyroAnimLayer2GateTimer(void);
extern void TickSpyroAnimLayer2(void);
extern void ApplyEulerRotation();
extern void MulMatrix0();

/* shared-base view of g_nSpyroState (0x80078ad0): the state read and the
   ZeroVector arg at +0x88 (g_anSpyroVelocityFp6) share one register. */
extern int g_anSpyroStateBlock[];
/* alias for g_abSpyroPersistentEuler (0x80078a64): the byte[3] scalar view
   is sdata-flagged and would hijack the held-base [0] store. +0x28 is
   g_anSpyroBodyMtx, +0x1BC the world transform scratch matrix. */
extern unsigned char g_abSpyroPersistentEulerBlock[];

/* 0x8004a7ec (544 bytes) — Spyro frame tick that forces the jump state
   (0xF): snapshot the pad, ChangeSpyroState(0xF) unless already there,
   disable charge via the state flags, then the same substep-integrate /
   position-step / finalize / anim-tick / body-matrix-rebuild sequence as
   TickSpyroFrameNoInput. Called from Gamestate01_Update (post-level-load
   intro). */
void TickSpyroFrameForceJump(void) {
  int i;
  int *sblock;
  int *vel;
  int *pos;
  unsigned char *euler;
  unsigned char *e28;
  unsigned char *m;
  int r;
  int y;
  int pad[8];

  g_bSpyroPadSnapshotReverseFlag = 0;
  SnapshotPadInputState(&g_dwPadPressed, &g_dwPad2Buttons);
  sblock = g_anSpyroStateBlock;
  if (sblock[0] != 0xF) {
    ChangeSpyroState(0xF);
  }
  g_nSpyroStateFlags = 8;
  if (g_nSpyroPadSnapshotCountdown <= 0) {
    g_nSpyroPadSnapshotCountdown = 1;
  }
  ZeroVector(&sblock[34]);
  for (i = 0; i < g_nFrameStep; i++) {
    IntegrateSpyroMotionForSubstep(i);
  }
  vel = g_anSpyroVelocityFp6;
  RShiftVector3(vel, 6);
  pos = (int *)((char *)vel - 0x100);
  CopyVector((int *)((char *)vel - 0x74), pos);
  AddVector(pos, pos, vel);
  for (i = 0; i < g_nFrameStep; i++) {
    AdvanceSpyroSubstepState();
  }
  for (i = 0; i < g_nFrameStep; i++) {
    TickSpyroAnimStateMachine();
    TickSpyroHornStrikeAttack();
    TickSpyroAnimLayer1();
    TickSpyroAnimLayer2GateTimer();
    TickSpyroAnimLayer2();
  }
  euler = g_abSpyroPersistentEulerBlock;
  e28 = euler + 0x28;
  euler[0] = g_nSpyroBodyPitch >> 4;
  r = g_nSpyroBodyRoll >> 4;
  y = g_nSpyroBodyYaw >> 4;
  euler[1] = r;
  euler[2] = y;
  ApplyEulerRotation(euler, e28, 0);
  m = euler + 0x1BC;
  ApplyEulerRotation(euler + 4, m, 0);
  MulMatrix0(e28, m, m);
  g_dwSpyroRequestMask = 0;
  SnapshotPadInputState(&g_dwPad2Buttons, &g_dwPadPressed);
}
