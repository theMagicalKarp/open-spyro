#include "globals.h"

extern void SubtractVector(int *dst, int *a, int *b);

extern int g_nSpyroWorldPosZ;         /* g_anSpyroWorldPos[2] */
extern int g_anLevelReadyFlagBlock[]; /* held-base view; world pos at -0x164 */
extern int D_80075728;                /* orb-drop trigger parameter */
extern unsigned int
    g_adwSpyroTriggerEventBlock[]; /* held-base view of the flags */

/* One-field record view: an aggregate access lets sched hoist the flags
   load above the scalar data-pointer store. */
typedef struct {
  unsigned int v;
} TriggerFlags;

#define ENT (((int **)&g_apScriptedMobEntries)[mob])
#define FLAGS (((TriggerFlags *)g_adwSpyroTriggerEventBlock)->v)

/* Moby record reached through g_pActorListBase (0x58 bytes). */
typedef struct {
  int *pos; /* 0x00 */
  unsigned char pad04[0x44];
  unsigned char active;  /* 0x48 */
  unsigned char flipped; /* 0x49 */
  unsigned char pad4a[8];
  unsigned char fade; /* 0x52 */
  unsigned char pad53[5];
} TriggerActor;

/* 0x80056f64 (0x41c): fires trigger region `idx` for `event` (1 = enter,
   2 = inside). The region record's type byte picks the action: orb drop and
   gem fly hand their data to Spyro, type 2 arms a ground-proximity latch,
   4/5/7 raise trigger flags (5 also calls the level overlay hook), and 6 is
   a scripted level warp that either starts the level transition or
   repositions the destination moby on the near side of its portal plane. */
void DispatchSpyroTriggerEvent(int idx, int event) {
  int tmp[3];
  unsigned char *rec;
  int *data;
  int *ready;
  int one;
  int mob;
  int d;
  int a;
  int *p;
  void (*hook)(int, int);

  rec = ((unsigned char **)g_pTriggerRegionTable)[idx];
  data = (int *)(rec + 4);
  switch (rec[0]) {
  case 0:
    if (event == 1 && g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ > 0x200) {
      g_nSpyroTriggerCurrentEventId = event;
    } else if (event == 2) {
      g_pSpyroOrbDropTriggerData = data;
      FLAGS = (FLAGS & 0x3FF) | 0x400;
      __asm__ volatile("");
      g_nSpyroTriggerEntryZ = g_nSpyroGroundHeightZ;
      D_80075728 = data[1];
    }
    break;
  case 1:
    g_pSpyroGemFlyTriggerData = data;
    FLAGS = (FLAGS & 0x3FF) | 0x800;
    break;
  case 2:
    if ((unsigned int)event < 2 &&
        g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ <= 0x190) {
      data[0] = 1;
    }
    break;
  case 4:
    FLAGS = (FLAGS & 0x3FF) | 0x2000;
    break;
  case 5:
    FLAGS = (FLAGS & 0x3FF) | 0x8000;
    ((void (*)(int, int))g_pfnLevelOverlayInitHook)(0xEA, 0);
    break;
  case 6:
    ready = g_anLevelReadyFlagBlock;
    if (*ready < 0) {
      break;
    }
    g_nCurrentLevelId = data[0];
    if (g_nCurrentLevelId > 0x40) {
      g_nCurrentLevelId = g_nActiveLevelId;
      break;
    }
    one = 1;
    g_nScriptedRespawnFlag = one;
    mob = data[1];
    g_nPreserveMobIdx = mob;
    if (g_nActiveLevelId % 10 != 0) {
      g_nScriptedMobTag = g_nActiveLevelId;
    }
    SubtractVector(tmp, (int *)((char *)ready - 0x164), ENT + 0xB);
    d = -tmp[0] * ENT[2] - tmp[1] * ENT[3] - tmp[2] * ENT[4];
    a = ENT[6];
    if (a < 0) {
      g_nCdStreamState = 0;
      g_nGamestate = one;
      g_nLevelTransitionTallyActive = one;
      g_nLevelTransitionPhase = 0;
      break;
    }
    ((TriggerActor *)g_pActorListBase)[a].active = 1;
    ((TriggerActor *)g_pActorListBase)[a].fade = 0xFF;
    p = ((TriggerActor *)g_pActorListBase)[a].pos;
    if (d < 0) {
      ((TriggerActor *)g_pActorListBase)[a].flipped = 1;
      p[1] = -ENT[2] >> 1;
      p[2] = -ENT[3] >> 1;
      p[3] = -ENT[4] >> 1;
    } else {
      ((TriggerActor *)g_pActorListBase)[a].flipped = 0;
      p[1] = ENT[2] >> 1;
      p[2] = ENT[3] >> 1;
      p[3] = ENT[4] >> 1;
    }
    break;
  case 7:
    if (event == 2 && g_anLevelTriggerStateSlots[data[0]] != 0) {
      FLAGS = (FLAGS & 0x3FF) | 0x26;
    }
    break;
  }
}
