#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "spyro_state.h"
#include "vecmath.h"

/* One-field record view: an aggregate access lets sched hoist the flags
   load above the scalar data-pointer store. */
typedef struct {
  unsigned int v;
} TriggerFlags;

#define ENT (((int **)g_apPortals)[mob])
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
   2 = inside). The region record's type byte (TriggerRegionType) picks the
   action: hazard and lift regions hand their data to Spyro and raise their
   event bit, the ground latch arms a proximity flag, charge-hold / rebound /
   damage raise their trigger-event bits (rebound also spawns type 0xEA), and
   the portal region warps: it either starts the level transition or
   repositions the destination actor on the near side of its portal plane. */
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
  case TRIGGER_HAZARD:
    if (event == 1 && g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ > 0x200) {
      g_nSpyroTriggerCurrentEventId = event;
    } else if (event == 2) {
      g_pSpyroHazardRegionData = data;
      FLAGS = (FLAGS & 0x3FF) | SPYRO_EV_HAZARD;
      SCHED_FENCE();
      g_nSpyroTriggerEntryZ = g_nSpyroGroundHeightZ;
      g_nSpyroHazardKind = data[1];
    }
    break;
  case TRIGGER_LIFT:
    g_pSpyroLiftRegionData = data;
    FLAGS = (FLAGS & 0x3FF) | SPYRO_EV_LIFT;
    break;
  case TRIGGER_GROUND_LATCH:
    if ((unsigned int)event < 2 &&
        g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ <= 0x190) {
      data[0] = 1;
    }
    break;
  case TRIGGER_CHARGE_HOLD:
    FLAGS = (FLAGS & 0x3FF) | SPYRO_EV_CHARGE_HOLD;
    break;
  case TRIGGER_REBOUND:
    FLAGS = (FLAGS & 0x3FF) | SPYRO_EV_REBOUND;
    ((void (*)(int, int))g_pfnLevelSpawnActor)(0xEA, 0);
    break;
  case TRIGGER_PORTAL:
    ready = g_anSparxHealthBlock;
    if (*ready < 0) {
      break;
    }
    g_nCurrentLevelId = data[0];
    if (g_nCurrentLevelId > 0x40) {
      g_nCurrentLevelId = g_nActiveLevelId;
      break;
    }
    one = 1;
    g_nPortalTransitionActive = one;
    mob = data[1];
    g_nEnteredPortalIdx = mob;
    if (g_nActiveLevelId % 10 != 0) {
      g_nReturnPortalLevelId = g_nActiveLevelId;
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
  case TRIGGER_DAMAGE:
    if (event == 2 && g_anLevelTriggerStateSlots[data[0]] != 0) {
      FLAGS = (FLAGS & 0x3FF) | (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT_TINT);
    }
    break;
  }
}
