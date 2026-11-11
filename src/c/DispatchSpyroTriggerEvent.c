#include "actor_type.h"
#include "cd_stream.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "match_asm.h"
#include "portal.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* One-field record view: an aggregate access lets sched hoist the flags
   load above the scalar data-pointer store. */
typedef struct {
  unsigned int v;
} TriggerFlags;

#define ENT (g_apPortals[portal])
#define FLAGS (((TriggerFlags *)g_adwSpyroTriggerEventBlock)->v)

/* 0x80056f64 (0x41c): fires trigger region `idx` for `event` (1 = enter,
   2 = inside). The region record's type byte (TriggerRegionType) picks the
   action: hazard and lift regions hand their data to Spyro and raise their
   event bit, the ground latch arms a proximity flag, charge-hold / rebound /
   damage raise their trigger-event bits (rebound also spawns
   ACTOR_TYPE_REBOUND_FX), and the portal region warps: it either starts the
   level transition or repositions the destination actor on the near side of its
   portal plane. */
void DispatchSpyroTriggerEvent(int idx, int event) {
  int tmp[3];
  unsigned char *rec;
  int *data;
  int *ready;
  int one;
  int portal;
  int d;
  int a;
  PortalState *p;
  void (*hook)(int, int);

  rec = ((unsigned char **)g_pTriggerRegionTable)[idx];
  data = TRIGGER_REGION_DATA(rec);
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
    g_pfnLevelSpawnActor(ACTOR_TYPE_REBOUND_FX, 0);
    break;
  case TRIGGER_PORTAL:
    ready = g_anSparxHealthBlock;
    if (*ready < 0) {
      break;
    }
    g_nNextLevelId = data[0];
    if (g_nNextLevelId > LEVEL_GNASTYS_LOOT) {
      g_nNextLevelId = g_nActiveLevelId;
      break;
    }
    one = 1;
    g_nPortalTransitionActive = one;
    portal = data[1];
    g_nEnteredPortalIdx = portal;
    if (LEVEL_IN_WORLD(g_nActiveLevelId) != LEVEL_HOME_SLOT) {
      g_nReturnPortalLevelId = g_nActiveLevelId;
    }
    SubtractVector(tmp, ready - (SPY_SPARX_HEALTH - SPY_POS), ENT->pts[1]);
    d = -tmp[0] * ENT->nx - tmp[1] * ENT->ny - tmp[2] * ENT->nz;
    a = ENT->actor;
    if (a < 0) {
      g_nCdStreamState = LSTREAM_PRESERVE;
      g_nGamestate = one;
      g_nLevelTransitionTallyActive = one;
      g_nLevelTransitionPhase = 0;
      break;
    }
    g_pActorListBase[a].mode = PORTALGUIDE_ENTER;
    g_pActorListBase[a].updateDist = ACTOR_UPDATE_ALWAYS;
    p = g_pActorListBase[a].state;
    if (d < 0) {
      g_pActorListBase[a].subMode = PORTAL_SIDE_REVERSED;
      p->offset[0] = -ENT->nx >> 1;
      p->offset[1] = -ENT->ny >> 1;
      p->offset[2] = -ENT->nz >> 1;
    } else {
      g_pActorListBase[a].subMode = PORTAL_SIDE_FORWARD;
      p->offset[0] = ENT->nx >> 1;
      p->offset[1] = ENT->ny >> 1;
      p->offset[2] = ENT->nz >> 1;
    }
    break;
  case TRIGGER_DAMAGE:
    if (event == 2 && g_anLevelTriggerStateSlots[data[0]] != 0) {
      FLAGS = (FLAGS & 0x3FF) | (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT_TINT);
    }
    break;
  }
}
