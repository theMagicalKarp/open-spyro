/* dragon_rescue.h — GS_DRAGON_RESCUE (BeginDragonRescue, DragonRescue_Update,
 * DragonRescue_Draw). g_nDragonRescueSubstate is word [0] of the rescue
 * record at 0x80077058 (g_anDragonRescueBlock). */

#ifndef OPEN_SPYRO_DRAGON_RESCUE_H
#define OPEN_SPYRO_DRAGON_RESCUE_H

/* Word indices into g_anDragonRescueBlock (the held-base view at
 * g_nDragonRescueSubstate, 0x80077058), each named after the global there. */
enum DragonRescueWord {
  DRW_TYPE_SNAPSHOT = -1,  /* g_nDragonRescueTypeSnapshot */
  DRW_SUBSTATE = 0,        /* g_nDragonRescueSubstate */
  DRW_STREAM_STATE = 1,    /* g_nDragonRescueStreamState */
  DRW_TIMER = 5,           /* g_nDragonRescueTimer */
  DRW_SECONDARY_TIMER = 10, /* g_nDragonRescueSecondaryTimer */
  DRW_AUDIO_FIRED = 12,    /* g_nDragonRescueAudioFiredFlag */
  DRW_PAD_HOLD_TIMER = 13, /* g_nDragonRescuePadHoldTimer */
  DRW_SOURCE_ACTOR = 26    /* g_pDragonRescueSourceActor */
};

enum DragonRescueSubstate {
  DRAGON_RESCUE_AURA = 0,        /* aura star swells while Spyro keeps playing */
  DRAGON_RESCUE_SWEEP = 1,       /* Spyro swept round the statue, camera eases in */
  DRAGON_RESCUE_LOAD_WAIT = 2,   /* wait for the dragon assets; spawn statue + stand-in */
  DRAGON_RESCUE_TURN = 3,        /* the statue turns to face its heading */
  DRAGON_RESCUE_CAMERA_PATH = 4, /* camera path plays with the dragon's voice line */
  DRAGON_RESCUE_LIGHT_UP = 5,    /* statue lights up, HUD icons roll in */
  DRAGON_RESCUE_COUNT = 6,       /* the dragon counter ticks */
  DRAGON_RESCUE_FADE_OUT = 7,    /* fade back to the world */
};

/* g_nDragonRescueStreamState (rescue record word [1]):
 * TickDragonRescueAssetStream's stages. */
enum DragonRescueStreamState {
  RESCUE_STREAM_READ = 0,          /* read the dragon asset (in two parts if it must split) */
  RESCUE_STREAM_RELOCATE = 1,      /* relocate statue / mirror / camera path; voice or part 2 */
  RESCUE_STREAM_UPLOAD_VOICE = 2,  /* voice sample to SPU RAM in 32 KB chunks */
  RESCUE_STREAM_RELOAD_LEVEL = 3,  /* from DRAGON_RESCUE_COUNT: re-read the level's TIM/audio tail */
  RESCUE_STREAM_LEVEL_AUDIO = 4,   /* re-upload it, re-read the ext chunk */
  RESCUE_STREAM_LEVEL_MESHES = 5,  /* re-relocate the level's moby meshes */
  RESCUE_STREAM_DONE = 6,
};

/* DRAGON_RESCUE_AURA: for AURA_FRAMES the aura star grows RADIUS_STEP and
 * brightens COLOR_STEP from COLOR_BASE a frame, the screen tinting up
 * TINT_STEP a frame after TINT_START; then the statue sinks SOURCE_SINK
 * (restored when it is despawned) and the sweep is timed from the orbit's
 * angle (>> 4, plus SWEEP_BASE_FRAMES, clamped to SWEEP_MIN..MAX_FRAMES).
 * DRAGON_RESCUE_SWEEP: the star shrinks back over AURA_FADE_FRAMES; Spyro
 * plays his stop animation (at LAND_ANIM_RATE) LAND_LEAD_FRAMES before the
 * sweep ends, and everything settles SETTLE_FRAMES after it. Floors are
 * probed from FLOOR_PROBE_RAISE above. DRAGON_RESCUE_LOAD_WAIT waits at least
 * LOAD_MIN_FRAMES, then bursts SHARD_BURST statue shards and spawns the
 * dragon at START_SCALE (1/3 size); DRAGON_RESCUE_TURN grows it GROW_STEP a
 * frame while it turns to face its heading over TURN_FRAMES. */
#define RESCUE_AURA_FRAMES 0x30
#define RESCUE_AURA_RADIUS_STEP 0x10
#define RESCUE_AURA_COLOR_BASE 0x40
#define RESCUE_AURA_COLOR_STEP 4
#define RESCUE_AURA_TINT_START 0x20
#define RESCUE_AURA_TINT_STEP 0x10
#define RESCUE_AURA_FADE_FRAMES 0x20
#define RESCUE_SOURCE_SINK 0x78
#define RESCUE_SWEEP_BASE_FRAMES 0x28
#define RESCUE_SWEEP_MIN_FRAMES 0x40
#define RESCUE_SWEEP_MAX_FRAMES 0x80
#define RESCUE_LAND_LEAD_FRAMES 0x10
#define RESCUE_LAND_ANIM_RATE 0xC
#define RESCUE_SETTLE_FRAMES 0x20
#define RESCUE_FLOOR_PROBE_RAISE 0x400
#define RESCUE_LOAD_MIN_FRAMES 0x10
#define RESCUE_SHARD_BURST 0x20
#define RESCUE_DRAGON_START_SCALE 0x60
#define RESCUE_DRAGON_GROW_STEP 3
#define RESCUE_TURN_FRAMES 0x18

#endif /* OPEN_SPYRO_DRAGON_RESCUE_H */
