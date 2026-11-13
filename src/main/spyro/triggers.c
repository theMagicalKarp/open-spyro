#include "actor_type.h"
#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "match_asm.h"
#include "portal.h"
#include "sample_bank.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

/* Map this frame's trigger events to Spyro state transitions (0x80040F68,
   0x308).

   `events` is the mask of events the calling state accepts; it is first gated
   by the post-hit invulnerability (SPYRO_HIT_INVULN_FRAMES, which drops the
   damage events 0x1F1, and the hazard 0x400 too while Spyro is still rising)
   and then by g_dwSpyroTriggerEventFlags, the events raised this frame. The
   0x5F1 group is damage: a one-of ladder that costs one g_nSparxHealth, picks
   a hurt state (or, once health is gone, a death state), and re-arms the
   invulnerability. The remaining bits (0xFC00) are independent: hazard (0x400,
   SPYRO_ST_HAZARD), headbutt rebound (0x8000), lift hold (0x800,
   SPYRO_ST_LIFT) and the charge-hold flag (0x2000). Returns 1 when a
   transition was dispatched. */

int ApplySpyroTriggerEvents(int events) {
  int *ev;
  int *lock;
  int *ready;
  int ret;
  int nonHit;
  int next;

  ret = 0;
  if (g_nNextLevelId != g_nActiveLevelId) {
    return 0;
  }
  if (g_nSpyroInvulnFrames != 0) {
    events &= ~SPYRO_EV_HITS;
    if (g_anSpyroMotionVec[2] > 0) {
      events &= ~SPYRO_EV_HAZARD;
    }
  }
  ev = g_adwSpyroTriggerEventBlock;
  events &= ev[0];

  if ((events & (SPYRO_EV_HITS | SPYRO_EV_HAZARD)) &&
      g_nSparxHealth >= SPARX_GONE) {
    if (g_bDebugNoDepletion == 0) {
      g_nSparxHealth = g_nSparxHealth - 1;
    }
    if (events & SPYRO_EV_HIT_PINNED) {
      if (g_nSparxHealth < SPARX_GONE) {
        next = SPYRO_ST_DEATH_PINNED;
      } else {
        next = SPYRO_ST_HURT_PINNED;
      }
      ChangeSpyroState(next);
    } else if (events & SPYRO_EV_HIT_TINT) {
      PlaySoundEffect(LEVEL_SFX(SFX_SPYRO_HURT_FLASH),
                      (Actor *)&ev[-SPY_TRIGGER_FLAGS], SFX_MODE_FLAT,
                      (unsigned char *)ev +
                          (SPYB_VOICE_MARKER - SPY_TRIGGER_FLAGS * 4));
      ChangeSpyroState(SPYRO_ST_HURT_TINT);
    } else if (events & SPYRO_EV_HIT_LONG) {
      ChangeSpyroState(SPYRO_ST_HURT_LONG);
    } else if (events & SPYRO_EV_KNOCKBACK_2) {
      ChangeSpyroState(SPYRO_ST_KNOCKBACK_2);
    } else if (events & SPYRO_EV_KNOCKBACK) {
      ChangeSpyroState(SPYRO_ST_KNOCKBACK);
    } else if (events & SPYRO_EV_HAZARD) {
      ChangeSpyroState(SPYRO_ST_HAZARD);
      if (*(int *)g_pSpyroHazardRegionData != 0 && g_bDebugNoDepletion == 0) {
        g_nSparxHealth = SPARX_FATAL;
      }
    } else {
      ChangeSpyroState(SPYRO_ST_HURT);
    }
    lock = g_anSpyroInvulnFramesBlock;
    if (lock[0] < SPYRO_HIT_INVULN_FRAMES) {
      lock[0] = SPYRO_HIT_INVULN_FRAMES;
    }
    return 1;
  }

  nonHit = events & 0xFC00;
  if (events & SPYRO_EV_HAZARD) {
    ready = g_anSparxHealthBlock;
    if (ready[0] >= 0) {
      ResetSpyroLinearMotion();
      if (g_bDebugNoDepletion == 0) {
        ready[0] = ready[0] - 1;
      }
      if (g_nSpyroInvulnFrames < SPYRO_HIT_INVULN_FRAMES) {
        g_nSpyroInvulnFrames = SPYRO_HIT_INVULN_FRAMES;
      }
      ChangeSpyroState(SPYRO_ST_HAZARD);
    }
    ret = 1;
  }

  if (nonHit & SPYRO_EV_REBOUND) {
    ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
    SetVector3Magnitude(g_anSpyroFirstContactNormal, 0x800);
    CopyVector(
        SPY_VIA(g_anSpyroFirstContactNormal, SPY_FIRST_NORMAL, SPY_VELOCITY),
        g_anSpyroFirstContactNormal);
    ret = 1;
  }
  if (nonHit & SPYRO_EV_LIFT) {
    if (g_nSpyroState != SPYRO_ST_LIFT) {
      ChangeSpyroState(SPYRO_ST_LIFT);
      ret = 1;
    }
  } else if (g_nSpyroState == SPYRO_ST_LIFT) {
    ChangeSpyroState(SPYRO_ST_GLIDE);
  }
  if (g_nSpyroState != SPYRO_ST_SUPERCHARGE) {
    if (nonHit & SPYRO_EV_CHARGE_HOLD) {
      g_nSpyroChargeHoldFlag = 1;
    } else {
      g_nSpyroChargeHoldFlag = 0;
    }
  }
  return ret;
}

/* One-field record view: an aggregate access lets sched hoist the flags
   load above the scalar data-pointer store. */
typedef struct {
  unsigned int v;
} TriggerFlags;

#define ENT (g_apPortals[portal])
#define FLAGS (((TriggerFlags *)g_adwSpyroTriggerEventBlock)->v)

/* 0x80056f64 (0x41c): fires trigger region `idx` for `event` (TRIGGER_EV_TOUCH
   / _OVER / _ON). The region record's type byte (TriggerRegionType) picks the
   action: hazard and lift regions hand their data to Spyro and raise their
   event bit, the ground latch arms a proximity flag, charge-hold / rebound /
   damage raise their trigger-event bits (rebound also spawns
   ACTOR_TYPE_REBOUND_FX), and the portal region warps: it either starts the
   level transition or repositions the destination actor on the near side of its
   portal plane. */
void DispatchSpyroTriggerEvent(int idx, int event) {
  int fromPlane[3];
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
    if (event == TRIGGER_EV_OVER &&
        g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ > TRIGGER_ON_HEIGHT) {
      g_nSpyroTriggerCurrentEventId = event;
    } else if (event == TRIGGER_EV_ON) {
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
    if ((unsigned int)event < TRIGGER_EV_ON &&
        g_nSpyroWorldPosZ - g_nSpyroGroundHeightZ <= GROUND_LATCH_HEIGHT) {
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
    SubtractVector(fromPlane, ready - (SPY_SPARX_HEALTH - SPY_POS),
                   ENT->pts[1]);
    d = -fromPlane[0] * ENT->nx - fromPlane[1] * ENT->ny -
        fromPlane[2] * ENT->nz;
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
    if (event == TRIGGER_EV_ON && g_anLevelTriggerStateSlots[data[0]] != 0) {
      FLAGS = (FLAGS & 0x3FF) | (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT_TINT);
    }
    break;
  }
}

#undef ENT
#undef FLAGS

/* Resolve the current ground surface to its trigger-region type byte. The low 6
   bits of g_nGroundSurfaceType index a table of region records; 0x3F is the
   "no region" sentinel. For a real region, dereference its record and return
   the leading type byte; otherwise return -1. */
int GetGroundTriggerRegionType(void) {
  int surface = g_nGroundSurfaceType & TRIGGER_REGION_MASK;

  if (surface != TRIGGER_REGION_NONE) {
    return *((unsigned char **)g_pTriggerRegionTable)[surface];
  }
  return -1;
}
