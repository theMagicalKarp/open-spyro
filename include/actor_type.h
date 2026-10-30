/* actor_type.h — the 0x58-byte actor ("moby") record on its own.
 *
 * actor.h includes this and adds the Actor * API prototypes. Files whose
 * matched source declares those helpers with int * parameters include just
 * this header to view a record through the struct. */

#ifndef OPEN_SPYRO_ACTOR_TYPE_H
#define OPEN_SPYRO_ACTOR_TYPE_H

typedef struct Actor {
  void *state;         /* 0x00 class-specific state block */
  int pad04;           /* 0x04 */
  int collision;       /* 0x08 collision record; < 0 (a KSEG0 pointer) when the
                        * actor can be hit, 0 / >= 0 when not (overlay target
                        * scans skip those) */
  int posX;            /* 0x0C position */
  int posY;            /* 0x10 */
  int posZ;            /* 0x14 */
  int flags;           /* 0x18 damage flags */
  int groundCode;      /* 0x1C packed ground contact, written by
                        * EncodeCachedVecToActorDirCode from the last ground
                        * probe: bit 31 valid, two 6-bit floor-normal angles
                        * at bits 22 and 16, floor z >> 2 below. Overlays
                        * store -1 / 0 to reset it and toggle bit 31 (bit 31
                        * also puts the actor on BuildActorDrawList's near
                        * list). */
  int mtx[5];          /* 0x20 rotation matrix (packed GTE, see RefreshActorDerivedState) */
  short gridCell;      /* 0x34 spatial-grid cell (RefreshActorDerivedState bit 2) */
  short type;          /* 0x36 actor class */
  short floorOffset;   /* 0x38 signed offset above the ground (SnapActorToGround) */
  unsigned char killGroup; /* 0x3A low 7 bits: kill group (< 0x20 a group bit,
                            * 0x7D/0x7E special; BuildLoadedLevelRuntime);
                            * bit 7 latches the death drop (SpawnActorDeathDrop) */
  unsigned char probeDist; /* 0x3B DriveActorTrackedMove ground-probe distance / 4;
                           * 0 default 0x12C, 0xFF none, 0xFE probe mode 1 */
  unsigned char anim;  /* 0x3C current animation */
  unsigned char nextAnim; /* 0x3D next animation */
  unsigned char frame; /* 0x3E current frame */
  unsigned char nextFrame; /* 0x3F next frame */
  unsigned char frameProgress; /* 0x40 frame progress */
  unsigned char frameRate; /* 0x41 progress per frame */
  unsigned char animFlags; /* 0x42 bit 1 done (g_nActorAnimDone), bit 0 new frame */
  unsigned char group; /* 0x43 activation group (BuildActiveActorList) */
  unsigned char rotX;  /* 0x44 euler x (8-bit angle) */
  unsigned char rotY;  /* 0x45 euler y */
  unsigned char rotZ;  /* 0x46 euler z (yaw) */
  unsigned char otBias; /* 0x47 signed OT-bin bias: RasterizeSpritePrimQueue
                        * bins at (MAC3 >> 6) - otBias, clamped >= 0. Default 4
                        * (InitActorRenderDefaults), 0x20 HUD/screen actors,
                        * 0x7F sprite records, 0xFE (-2) pushes it one bin pair deeper */
  unsigned char mode;  /* 0x48 behaviour state; >= ACTOR_MODE_DORMANT dormant,
                          ACTOR_MODE_END ends the pool */
  unsigned char subMode; /* 0x49 sub-state */
  unsigned char visZone; /* 0x4A visibility-gate index into g_abVisZoneGates;
                          * 0xFF = always drawn (BuildRenderEntityLists) */
  unsigned char renderList; /* 0x4B sign picks the BuildRenderEntityLists
                             * bucket: 0, < 0, > 0 (default 0x20) */
  /* 0x4C..0x4F tint. Read as one word, 0x4C..0x4E pack three 6-bit levels
   * under a 0xA tag: 0xA << 20 | c2 << 12 | c1 << 6 | c0. SelectActorLodBand
   * builds it with 0x18 as neutral, which gives the 0xA18618 the overlays
   * store, and the flame band raises c2 while lowering c0/c1. With 0x4C..0x4E
   * zeroed, tintMode picks a preset instead: gem colours 1..5 (HUD icons set
   * type - 0x52) and 0xB for HUD sprites. TickActorFlameGlow drives tint2 as a
   * 0..0x60 glow on its own. These names describe the byte layout only; the
   * renderer that reads them is hand-written asm and not decoded yet. */
  unsigned char tint0; /* 0x4C */
  unsigned char tint1; /* 0x4D */
  unsigned char tint2; /* 0x4E */
  unsigned char tintMode; /* 0x4F */
  unsigned char cullRadius; /* 0x50 view-space cull bound >> 8 (BuildActorDrawList
                            * loads it as the halfword with ->drawn) */
  unsigned char drawn; /* 0x51 drawn last frame: always kept active */
  unsigned char updateDist; /* 0x52 activation radius >> 10 */
  unsigned char dropKind; /* 0x53 death-drop item id (& 0x7F, 1..0x7E fixed;
                           * 0xFF = random gem roll) — SpawnActorDeathDrop,
                           * IsActorInRunningKillSet */
  unsigned char sfxSlot; /* 0x54 sound owner key (PlaySoundEffect) */
  unsigned char soundRange; /* 0x55 voice audible range >> 10 (TickActiveSoundVoices) */
  unsigned char poolIndex; /* 0x56 id byte for actors outside the pool */
  unsigned char scale; /* 0x57 draw scale override, 0 = mesh default (likely; overlay
                        * grow/shrink tweens drive it 0x20..0x60, BuildActorDrawList
                        * packs it into the draw word) */
} Actor; /* 0x58 */

/* Actor.flags hit bits that Spyro's attacks raise through
 * DispatchActorContactAtSphere (AdvanceSpyroPhysics, TickSpyroAttackEffects). */
#define ACTOR_HIT_FLAME 0x10000  /* the flame breath */
#define ACTOR_HIT_CHARGE 0x20000 /* a charge (SPYRO_ST_CHARGE) */
#define ACTOR_HIT_BLAST 0x40000   /* another actor's blast or shot: a cracking boulder's
                                  * burst (with FLAME), level 19's aimed bolt; boulders and
                                  * roaming rams react to it */
#define ACTOR_HIT_SUPER 0x80000  /* a supercharge; a superflame breath adds it to FLAME */
#define ACTOR_HITS_SPYRO (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)
#define ACTOR_HIT_ANY (ACTOR_HITS_SPYRO | ACTOR_HIT_BLAST)

/* Size of one actor record: the stride of the actor pool, the sprite-record
 * queue and the HUD icon table. (The typedef fails to compile if the struct
 * ever drifts from it; it emits nothing.) */
/* Actor.mode sentinels: UpdateActors and the scans skip dormant records, and
 * the pool ends at the first record whose mode is 0xFF. */
#define ACTOR_MODE_DORMANT 0x80
#define ACTOR_MODE_END 0xFF

#define ACTOR_SIZE 0x58
typedef char ActorSizeCheck[sizeof(Actor) == ACTOR_SIZE ? 1 : -1];

/* Byte offset of an Actor field. */
#define ACTOR_OFFSET(field) ((int)&((Actor *)0)->field)

/* An Actor field accessed as a plain byte-offset memory reference rather
 * than a struct member. Same address and width as `actor->field`, but gcc
 * 2.7 does not mark it in-struct, so the scheduler and cse keep it ordered
 * against scalar global stores; a few matched functions need that order.
 * Use it only where `actor->field` was measured to change the output. */
#define ACTOR_RAW(actor, type, field) (*(type *)((char *)(actor) + ACTOR_OFFSET(field)))

/* Actor->type values that mean the same class in every level (one global
 * g_apActorMeshTable index space). Only classes whose behaviour is pinned
 * down are named; the rest stay numeric in the per-level switches. */
enum ActorType {
  ACTOR_TYPE_PORTAL_NAME = 1,          /* hub portal name text, shown while the camera faces the portal */
  ACTOR_TYPE_LEVEL_EXIT = 9,       /* exit whirlwind: lifts Spyro back to the hub */
  ACTOR_TYPE_PATH_PATROLLER = 10, /* walks its path, turning back at the ends unless it loops (PathState) */
  ACTOR_TYPE_TIMED_EMITTER = 11, /* particle emitter on a random timer */
  ACTOR_TYPE_FOLLOWER = 13,        /* rides a parent actor, released in turn when it dies (FollowState) */
  ACTOR_TYPE_EXTRA_LIFE = 14,      /* collected: g_nExtraLives + 1 */
  ACTOR_TYPE_SPARX_FOOD = 15,      /* collected: g_nSparxFoodCount + 1 (20: a life) */
  ACTOR_TYPE_BUTTERFLY = 16,       /* eaten by Sparx: g_nSparxHealth + 1 */
  ACTOR_TYPE_AIMED_SHOT = 17, /* levels 5/17/23/34: leaves its parent's facing aimed at Spyro (speed 0xA0); despawns on timeout, a world hit or reaching Spyro (DriftState) */
  ACTOR_TYPE_PRESSURE_SWITCH = 18, /* level 0: pressure switch (SwitchState) */
  ACTOR_TYPE_PATROL_WARD = 23, /* level 3: patrol guard (WardState) */
  ACTOR_TYPE_GUNNER = 28, /* level 20: wakes its partner, turns to it, fires type 400 shots (GunnerState) */
  ACTOR_TYPE_CARRIED_EGG = 32, /* level 19: rides its carrier until the carrier reaches pose 4, then behaves as a dragon egg */
  ACTOR_TYPE_EGG_THIEF = 33,       /* runs off with a dragon egg (34), drops it when hit (ThiefState) */
  ACTOR_TYPE_DRAGON_EGG = 34,      /* recovered egg: rises, rolls the HUD egg tally */
  ACTOR_TYPE_BOLT_GUARD = 35, /* level 19: scatters when hit, fires type 494 bolts (BoltGuardState) */
  ACTOR_TYPE_PATH_WALKER = 36, /* waits for Spyro, then paces its path (PathState) */
  ACTOR_TYPE_RIM_RIDER = 37, /* level 16: rides its parent's rim, drifts toward Spyro, spawns ACTOR_TYPE_RIM_RIDER_HELPER (RimState) */
  ACTOR_TYPE_SPLIT_SHOT = 38, /* levels 12/15/16/27: launched at Spyro on its thrower's release frame; splits into four more of itself when it reaches him */
  ACTOR_TYPE_RIM_RIDER_HELPER = 39, /* level 16: follows its owner facing the camera until released (HelperState) */
  ACTOR_TYPE_BIG_CHARGING_RAM = 45, /* level 28: ACTOR_TYPE_CHARGING_RAM's big kind (shorter reach; only a charge hurts it) */
  ACTOR_TYPE_CHARGING_RAM = 46, /* level 28: paces, charges when Spyro is close and not looking (ChargingRamState) */
  ACTOR_TYPE_THROWN_ROCK = 47, /* level 28: carried and thrown at Spyro's feet by Jacques; smashes on landing (CarriedRockState) */
  ACTOR_TYPE_METALHEAD = 48, /* level 22 boss (MetalheadState) */
  ACTOR_TYPE_EXIT_GATED_PROP = 49, /* level 0: sequenced prop gated on the exit vortices (AnimState) */
  ACTOR_TYPE_POST_GRABBER = 50, /* level 25: seizes Spyro in front of it under a scripted camera, then walks back to its post */
  ACTOR_TYPE_GRABBER_LOOKOUT = 51, /* level 25: bolts when struck; becomes ACTOR_TYPE_POST_GRABBER once its partner is roused */
  ACTOR_TYPE_BOMB_LOBBER = 52, /* levels 25/26: hides until Spyro is in range, then lobs ACTOR_TYPE_LOBBED_BOMB at him */
  ACTOR_TYPE_ALARM_SENTRY = 53, /* level 25: raises the alarm when it sees Spyro; becomes ACTOR_TYPE_ROUSED_SENTRY once its partner is roused */
  ACTOR_TYPE_ROUSED_SENTRY = 54, /* level 25: backs away when struck; otherwise watches and fires */
  ACTOR_TYPE_LOBBED_BOMB = 55, /* levels 25/26: bursts on landing, on a timer or against Spyro */
  ACTOR_TYPE_FALLING_ROCK = 56, /* level 25: drops until it lands, hits Spyro or times out, then shatters */
  ACTOR_TYPE_ROLLING_BOULDER = 57, /* levels 21/22: rolls to the floor and bursts into more of itself (state 1) (BoulderState) */
  ACTOR_TYPE_SINKING_BLOCK = 61, /* level 26: struck, frees its partner and sinks into the floor */
  ACTOR_TYPE_CHASING_PATH_GUARD = 62, /* level 26: patrols its path, turns on Spyro and chases; knocked away when hit */
  ACTOR_TYPE_PLATFORM_POD = 65, /* level 22: raises four ACTOR_TYPE_BOBBING_PLATFORMs and lobs ACTOR_TYPE_POD_SHOTs (PlatformPodState) */
  ACTOR_TYPE_TETHERED_GUARD = 66, /* level 15: tethered guard (TetherState) */
  ACTOR_TYPE_SEED_POD_SHARD = 67,    /* 67..69: seed pod burst (67/68 bounce, 69 tumbles) */
  ACTOR_TYPE_LOBBED_BOLT = 70, /* level 22: arcs along its heading, bursts into sparks on landing (LobbedBoltState) */
  ACTOR_TYPE_LETTER_OTHER = 0x4C,  /* a non-A..Z glyph of a floating name */
  ACTOR_TYPE_COMET_SPARK = 77, /* level 24: dropped by ACTOR_TYPE_COMET; replays its animation subMode times, then is removed */
  ACTOR_TYPE_RIDING_FLIGHT_TARGET = 78, /* flight levels 5/29: rides its host's attach point 0; flamed, pays out its reward, tallies, blasts (ACTOR_HIT_SUPER) and bursts into tumble shards (RiderState) */
  ACTOR_TYPE_POD_SHOT = 82, /* level 22: flies until it hits the floor, a wall or times out (PodShotState) */
  ACTOR_TYPE_GEM_1 = 0x53,         /* gems, by CollectPickupActor's value */
  ACTOR_TYPE_GEM_2 = 0x54,
  ACTOR_TYPE_GEM_5 = 0x55,
  ACTOR_TYPE_GEM_10 = 0x56,
  ACTOR_TYPE_GEM_25 = 0x57,
  ACTOR_TYPE_FORMATION_ESCORT = 88, /* level 26: follows its leader on a ring, flies off when the leader breaks away */
  ACTOR_TYPE_COMET_CANNON = 91, /* level 24: blasts Spyro in its sights, or lobs ACTOR_TYPE_COMET at its trigger's target (CometCannonState) */
  ACTOR_TYPE_LUNGING_GUARD = 92, /* level 24: wakes its partner, paces, lunges; spins away when struck */
  ACTOR_TYPE_GROUND_BOUNCE_SHARD = 93, /* levels 19/22: explosion shard with a ground bounce (ShardState) */
  ACTOR_TYPE_BOULDER_THROWER = 100, /* level 21: picks up and hurls ACTOR_TYPE_ROLLING_BOULDER, rides its partner, charges (BoulderThrowerState) */
  ACTOR_TYPE_GATEKEEPER = 101, /* level 21: releases its run of keys as Spyro approaches (GatekeeperState) */
  ACTOR_TYPE_BOULDER_THROWER_B = 102, /* level 22's ACTOR_TYPE_BOULDER_THROWER */
  ACTOR_TYPE_COMET = 104, /* level 24: flies its arc with a fading trail, dropping ACTOR_TYPE_COMET_SPARK (CometState) */
  ACTOR_TYPE_CARRIER = 109, /* level 14: flying carrier that follows Spyro and can be ridden (CarrierState) */
  ACTOR_TYPE_SAVE_HOST = 110,      /* hovers near its anchor, opens the save menu (SaveHostState) */
  ACTOR_TYPE_REARING_CRITTER = 113, /* level 1: idles, rears up at Spyro, spins away when knocked back (RearState) */
  ACTOR_TYPE_PATH_ENEMY = 114, /* level 0: path-following enemy (WalkerState) */
  ACTOR_TYPE_GUARD_PAIR = 115, /* level 2: charges Spyro when near and level, cues its partner (GuardPairState) */
  ACTOR_TYPE_SPARX = 0x78,         /* g_pSparxActor (SparxState) */
  ACTOR_TYPE_SENTRY = 121, /* level 1: sentry (SentryState) */
  ACTOR_TYPE_LANTERN = 122, /* level 25: lit and snuffed by Spyro; its glow node follows */
  ACTOR_TYPE_LANTERN_FLAME = 124, /* level 25: removed when its animation ends */
  ACTOR_TYPE_SINGING_PATH_WALKER = 126, /* levels 24/28: walks its path, sings while it rests, can be carried and thrown by its rider (PathWalkerState) */
  ACTOR_TYPE_CHARGE_REACT_PROP = 129, /* levels 2/25: a charge plays its reaction animation once */
  ACTOR_TYPE_BIG_LUNGING_GUARD = 130, /* level 24: ACTOR_TYPE_LUNGING_GUARD plus two followers and a growl */
  ACTOR_TYPE_RUBBLE_SHARD = 133, /* level 27: explosion shard with a ground bounce (ShardState); also 215 */
  ACTOR_TYPE_ROCK_THROWER = 134, /* level 27: throws ACTOR_TYPE_FALLING_CHUNK rocks at Spyro (RockThrowerState) */
  ACTOR_TYPE_FALLING_CHUNK = 135, /* level 27: rides the floor or falls; bursts into rubble (FallingChunkState) */
  ACTOR_TYPE_BREAKABLE_WALL = 136, /* level 27: four parts; striking one blows the wall into rubble (BreakableWallState) */
  ACTOR_TYPE_CHARGING_GUARD = 137, /* levels 18/20: turns on Spyro in range and charges; the camera pulls in after a charge */
  ACTOR_TYPE_SUPERFLAME_GIVER_B = 142, /* level 27: second superflame giver (ACTOR_TYPE_SUPERFLAME_GIVER's body) */
  ACTOR_TYPE_SHOCKWAVE = 143, /* level 33: set off where a trail bomb lands (ShockwaveState) */
  ACTOR_TYPE_BURST_GUNNER = 145, /* level 32: fires bursts of type 156 shots (BurstGunnerState) */
  ACTOR_TYPE_BREAKABLE_PROP = 146, /* level 6: a flame breaks it; a charge needs a second hit within half a second */
  ACTOR_TYPE_SWEEP_GUNNER = 147, /* level 32: sweeps its aim between two yaw limits (SweepGunnerState) */
  ACTOR_TYPE_LOB_GUNNER = 148, /* level 32: throws a held type 176 lob (LobGunnerState) */
  ACTOR_TYPE_JACQUES = 150, /* level 28 boss: hops around his path ring throwing ACTOR_TYPE_THROWN_ROCK (JacquesState) */
  ACTOR_TYPE_BOULDER_DEBRIS = 151,   /* 151..153: cracking boulder burst (151 tumbles, 152/153 spark) */
  ACTOR_TYPE_HATCH = 154, /* level 31: opens when struck, releasing its occupant; recloses once Spyro is gone */
  ACTOR_TYPE_FLYING_EMBER = 155, /* level 25: rides the floor trailing sparks; bursts on landing, Spyro or timeout */
  ACTOR_TYPE_BURST_GUNNER_SHOT = 156, /* level 32: the burst gunner's shot */
  ACTOR_TYPE_FLAME_CHUNK_TRIGGER = 157, /* level 2: flamed once, starts its world chunk animation (ModifyWorldChunkAnimFlags) */
  ACTOR_TYPE_LANE_PATROLLER = 159, /* level 33: path patroller that turns back toward Spyro (LanePatrollerState) */
  ACTOR_TYPE_WANDERING_CRITTER = 160, /* wanders; knocked away when hit */
  ACTOR_TYPE_GNASTY_GNORC = 161, /* level 33 boss (GnastyState) */
  ACTOR_TYPE_CYCLING_FLIGHT_TARGET = 162, /* level 23: rolls over four paths; flamed, pays out and tallies (CyclerState) */
  ACTOR_TYPE_BOBBING_FLIGHT_TARGET = 163, /* level 29: flamed, pays out, tallies and throws off ACTOR_TYPE_BOBBER_SPINOFF (BobberState) */
  ACTOR_TYPE_POST_GUARD = 165, /* level 2: steps out to the post nearer Spyro and hops at him with its partner (PostState) */
  ACTOR_TYPE_PATH_CHARGER = 166, /* level 2: walks its path, charges Spyro down it (PatrolState) */
  ACTOR_TYPE_TRAIL_BOMB_PUFF = 168, /* level 33: dropped by ACTOR_TYPE_TRAIL_BOMB; replays its animation subMode times, then is removed */
  ACTOR_TYPE_TRAIL_BOMB = 169, /* level 33: Gnasty's rolling trail bomb (TrailBombState) */
  ACTOR_TYPE_GATEKEEPER_B = 170, /* level 22's ACTOR_TYPE_GATEKEEPER */
  ACTOR_TYPE_FLAME_CHUNK_SWITCH = 171, /* level 32: flamed, toggles its world chunk animation; re-arms after 0x50 frames (FlameChunkSwitchState) */
  ACTOR_TYPE_KEY = 173,            /* the level key: g_nLevelKeyState = 1 */
  ACTOR_TYPE_LOCKED_CHEST = 174,   /* opened with the key, drops a reward */
  ACTOR_TYPE_LOB_GUNNER_LOB = 176, /* level 32: the lob gunner's lob */
  ACTOR_TYPE_LOOT_THIEF = 177, /* level 34: treasure thief (LootThiefState) */
  ACTOR_TYPE_LOOT_CRITTER = 179, /* level 34: loot critter (CritterState) */
  ACTOR_TYPE_LOOT_VORTEX = 181, /* levels 33/34 (LootVortexState) */
  ACTOR_TYPE_BRAWLER = 184, /* level 7: stationary brawler (BrawlerState) */
  ACTOR_TYPE_SHOOTER = 186, /* level 7: blinking sentry that fires type 197 (ShooterState) */
  ACTOR_TYPE_HOME_TALKER = 187, /* 187..192: the homeworld talkers (levels 0/6/12/18/24/30); walking up facing one starts its actor cutscene */
  ACTOR_TYPE_WANDERING_CRITTER_B = 193, /* levels 6/7: wanders; knocked away when hit (CritterKnockState) */
  ACTOR_TYPE_WOOD_CHEST = 194,       /* breaks on the first hit into 0xFF..0x101 shards (BreakState) */
  ACTOR_TYPE_WOBBLE_CHEST = 195,     /* shakes and recoils when hit, then breaks (WobbleState) */
  ACTOR_TYPE_SHOOTER_SHOT = 197, /* level 7: the shooter's knock-back shot (ShooterShotState) */
  ACTOR_TYPE_CHARGING_YAK = 198, /* level 9: grazes, lowers its horns and charges when Spyro comes near */
  ACTOR_TYPE_MOUNT = 199, /* level 31: carries a rider; knocked away (dropping it) when struck */
  ACTOR_TYPE_HATCH_CARRIER = 200, /* level 31: lifts ACTOR_TYPE_HATCH's occupant out and throws it */
  ACTOR_TYPE_CARRIED_THROWER = 201, /* level 31: walks up and throws the actor in its hands */
  ACTOR_TYPE_FLOATING_BARREL = 202, /* level 31: bobs on the water; struck, bursts with a push-back blast and sinks */
  ACTOR_TYPE_FENCE_GUARD = 203, /* level 27: sent in by ACTOR_TYPE_FENCE_CAPTAIN (FenceGuardState) */
  ACTOR_TYPE_FENCE_CAPTAIN = 204, /* level 27: wakes and recalls its fence guards (FenceCaptainState) */
  ACTOR_TYPE_OVERHEAD_THROWER = 205, /* levels 9/10: holds its projectile overhead, throws it at Spyro on its throw frame; stunned when hit */
  ACTOR_TYPE_BELL_A = 206, /* levels 7/8: rings when charged, spawning ACTOR_TYPE_BELL_RIPPLE (also 239/242) */
  ACTOR_TYPE_FLOATING_MINE = 207, /* level 31: drifts like the barrel; flamed, runs at the nearest target and bursts */
  ACTOR_TYPE_HUB_AMBIENCE = 208, /* level 30: starts and keeps alive three voice channels (MusicState) */
  ACTOR_TYPE_THROWN_SNOWBALL = 209, /* level 9: flies its arc; bursts on a timer, landing or reaching Spyro */
  ACTOR_TYPE_BOBBING_PLATFORM = 210, /* level 22: spinning platform that bobs on a fixed period (BobbingPlatformState) */
  ACTOR_TYPE_PATH_SNOWBALL_THROWER = 211, /* 211..212, level 9: walks its path and throws ACTOR_TYPE_THROWN_SNOWBALL at Spyro */
  ACTOR_TYPE_CIRCLING_BIRD = 213, /* level 9: circles over Spyro, then flies back to its perch */
  ACTOR_TYPE_SOLDIER = 214, /* level 6: patrols, guards or escorts by `kind`; gunners lob ACTOR_TYPE_FIREBALL (also 216) */
  ACTOR_TYPE_RUBBLE_SHARD_B = 215, /* level 27: ACTOR_TYPE_RUBBLE_SHARD's twin */
  ACTOR_TYPE_SOLDIER_B = 216, /* level 6: ACTOR_TYPE_SOLDIER's body */
  ACTOR_TYPE_VAULT_TIMER = 217, /* level 34: counts the vault down on the HUD while Spyro rides the lift */
  ACTOR_TYPE_CATAPULT = 218, /* level 8: catapult (CatapultState) */
  ACTOR_TYPE_CAMERA_FACING_PROP = 222, /* 222..224, level 1: turns toward the view once, on its first frame */
  ACTOR_TYPE_FIREBALL_CANNON = 225, /* level 6: blasts Spyro in its sights, then lobs ACTOR_TYPE_FIREBALL */
  ACTOR_TYPE_TWO_PATH_RUNNER = 226, /* level 8: two-path patroller (RunnerState) */
  ACTOR_TYPE_SUPERFLAME_GIVER = 227, /* level 14: grants the superflame (SuperflameGiverState) */
  ACTOR_TYPE_GUARD_LOOKOUT = 229, /* level 8: rouses its linked guard when Spyro comes in range; swipes up close (LookoutState) */
  ACTOR_TYPE_SWOOPER = 230, /* swooping guard (SwoopState) */
  ACTOR_TYPE_TOUGH_RAM = 231, /* level 9: ACTOR_TYPE_CHARGING_YAK that takes four hits, thrown up by each */
  ACTOR_TYPE_BOBBER_SPINOFF = 232, /* level 29: spins off ACTOR_TYPE_BOBBING_FLIGHT_TARGET until it lands or times out (SpinRunState) */
  ACTOR_TYPE_REBOUND_FX = 234, /* TRIGGER_REBOUND's impact: placed on Spyro's first contact point, facing the wall normal; removed when its animation ends */
  ACTOR_TYPE_WANDERING_CRITTER_C = 236, /* levels 30/31/32: UpdateActorWander roamer; knocked away with a death drop when struck */
  ACTOR_TYPE_FIREBALL = 237, /* level 6: flies under gravity, bursts on walls or Spyro, wakes its linked actor */
  ACTOR_TYPE_ROAMING_CRITTER = 238, /* level 8: roams its patch; knocked away (stunned by a charge) when struck (RoamingCritterState) */
  ACTOR_TYPE_BELL_B = 239, /* levels 7/8: ACTOR_TYPE_BELL_A's body */
  ACTOR_TYPE_SUPERFLAME_GIVER_ALT = 241, /* level 27's superflame giver: wider reach, later grant frame */
  ACTOR_TYPE_BELL_C = 242, /* levels 7/8: ACTOR_TYPE_BELL_A's body */
  ACTOR_TYPE_DRAGON_STATUE = 0xFA, /* trapped dragon: counts the rescue */
  ACTOR_TYPE_STATUE_SHARD = 0xFB,  /* its burst shards (BurstShardState) */
  ACTOR_TYPE_GUARD_CAPTAIN = 253, /* level 13: guard captain (CaptainState) */
  ACTOR_TYPE_WOOD_CHEST_SHARD = 255, /* 255..257: wood chest burst (255/256 bounce, 257 tumbles) */
  ACTOR_TYPE_GEM_DIGIT_0 = 0x104,  /* floating gem-value digits 0..9: 0x104..0x10D */
  ACTOR_TYPE_VILLAGER = 270, /* levels 12/13 (VillagerState) */
  ACTOR_TYPE_SHEPHERD = 271, /* levels 12/13 (ShepherdState) */
  ACTOR_TYPE_PATH_GUARD = 283, /* walks its waypoint ring, waits on its partner */
  ACTOR_TYPE_TETHER_KEEPER = 284, /* level 15: tether keeper (KeeperState) */
  ACTOR_TYPE_SENTINEL = 285, /* level 15: sentinel (SentinelState) */
  ACTOR_TYPE_AMBIENT_SOUND = 286,  /* plays random ambient voices, mode 2 along a path (AmbientSoundState) */
  ACTOR_TYPE_BOUNCE_DEBRIS = 288, /* 288..289, levels 7/8: bouncing debris (BounceState) */
  ACTOR_TYPE_PATH_RUNNER = 293, /* 293..294, level 21: runs its path ahead of Spyro, raises its goal flag on arrival */
  ACTOR_TYPE_HOMING_WISP = 295, /* homing wisp (WispState) */
  ACTOR_TYPE_PARTNER_PLATFORM = 296, /* level 26: bobbing platform tied to a partner actor */
  ACTOR_TYPE_PARTNER_BOUNCER = 298, /* level 26: rides its partner; thrown when the partner is charged */
  ACTOR_TYPE_FLIGHT_CHEST = 299,     /* flight levels: flame or charge frees its reward */
  ACTOR_TYPE_WHIRLWIND = 300,      /* lifts Spyro to its top (WhirlwindState, SPYRO_ST_LIFT) */
  ACTOR_TYPE_BOLT_CASTER = 301, /* level 14: wakes in range and fires type 295 bolts (BoltCasterState) */
  ACTOR_TYPE_CHUNK_OPERATOR = 302, /* level 14: drives its world chunk's animation (ChunkOperatorState) */
  ACTOR_TYPE_RUNNER_SPAWNER = 303, /* level 14: spawns type 304 runners (RunnerSpawnerState) */
  ACTOR_TYPE_WAYPOINT_RUNNER = 304, /* level 14 (WaypointRunnerState) */
  ACTOR_TYPE_FLEEING_WALKER = 305, /* level 14: walks one of three paths, flees Spyro (FleeingWalkerState) */
  ACTOR_TYPE_BANKING_FLIGHT_TARGET = 308, /* flight levels 5/17/23: tweens its path banking into turns; flamed, pays out, tallies and runs off until it splashes down (RollerState) */
  ACTOR_TYPE_SPARKLE_SHARD = 309,    /* one puff a frame; with 310/311 the burst of wobble, spin and
                                      * locked chests and breakable rocks */
  ACTOR_TYPE_WOOD_SHARD = 310,       /* 310..311: bounces and throws three sparks a frame */
  ACTOR_TYPE_CRACKING_BOULDER = 312, /* shakes and smokes when hit, then bursts */
  ACTOR_TYPE_ALARM_POST_GUARD = 314, /* level 4: patrols between posts, raises the alarm, hands off to its partner (GuardState) */
  ACTOR_TYPE_SHEPHERD_THROW = 315, /* levels 12/13: the shepherd releases it on frame 0xE; once live it flies its heading under gravity until its timer runs out (CrookState / FallState) */
  ACTOR_TYPE_FALLING_DEBRIS = 318, /* levels 13/14/15: tumbles along its velocity until its timer runs out or it leaves the screen */
  ACTOR_TYPE_RESPAWNER = 323,      /* puts dormant actors of one type back once Spyro is far (RespawnState) */
  ACTOR_TYPE_SORCERER = 326, /* level 13 (SorcererState) */
  ACTOR_TYPE_FLOOR_MARKER = 328, /* drops onto the floor triangle under it once, then sits there */
  ACTOR_TYPE_SEED_POD = 329,         /* wobbles when struck, holds a floating prize above itself */
  ACTOR_TYPE_CHECKPOINT = 331,     /* 331..333: sets the respawn anchor (SetSpawnAnchorFromActor) */
  ACTOR_TYPE_LUNGER = 335, /* level 4: charges and leaps at Spyro, takes a few hits (LungeState) */
  ACTOR_TYPE_ANIMATED_SCENERY = 336, /* level 0: re-randomised speed; plays its hit animation when damaged (also 342) */
  ACTOR_TYPE_FLOCK_LEADER = 339, /* level 0: flock leader (FlockState) */
  ACTOR_TYPE_FLIGHT_RING = 340, /* flight levels 11/17: spinning ring; flying through its plane tallies it (TargetState) */
  ACTOR_TYPE_ANIMATED_SCENERY_B = 342, /* level 0: ACTOR_TYPE_ANIMATED_SCENERY's body */
  ACTOR_TYPE_ATTACH_BEAD = 343, /* level 19: streams between its owner's attach points (AttachBeadState) */
  ACTOR_TYPE_TIME_BONUS = 345,       /* flight time bonus 345..347: flies to the HUD clock */
  ACTOR_TYPE_SPINNER = 349, /* level 4: idles, rears up at Spyro, spins away when knocked back (SpinnerState) */
  ACTOR_TYPE_SEQUENCED_PROP = 350, /* level 0: sequenced prop (PropAnimState) */
  ACTOR_TYPE_FLIGHT_TARGET = 353,    /* breakable flight target: bursts into shards 374..386 when flown through */
  ACTOR_TYPE_FLIGHT_HUD = 358,       /* flight-level HUD controller (clock digits, target tally) */
  ACTOR_TYPE_TUMBLE_SHARD = 359, /* 359..361: falling, spinning burst shards of the riding target and level 23's cycler (TumbleState) */
  ACTOR_TYPE_WRECK_EMBER = 362, /* flight levels 5/29: an ember from a wrecked path smoker/prop; rebounds off walls (EmberState) */
  ACTOR_TYPE_HAZARD1_ENTER_FX = 363, /* g_anHazardEnterSpawnType[1]: one-shot effect at Spyro as he enters a kind-1 hazard */
  ACTOR_TYPE_HAZARD1_DEATH_FX = 364, /* g_anHazardTimeoutSpawnType[1]: one-shot effect when the kind-1 hazard kills him */
  ACTOR_TYPE_SHEMP_GUARD = 370, /* level 10: Doctor Shemp's guard (ShempGuardState) */
  ACTOR_TYPE_DOCTOR_SHEMP = 371, /* level 10 boss (ShempState) */
  ACTOR_TYPE_SHEMP_LOOKOUT = 372, /* level 10: Doctor Shemp's lookout (LookoutState) */
  ACTOR_TYPE_FLIGHT_TARGET_SHARD = 374, /* 374..386: a flight target's burst shards */
  ACTOR_TYPE_SPIN_CHEST = 390,       /* each charge spins it faster; spun fast enough, or flamed, it bursts */
  ACTOR_TYPE_PATCH_RIDER = 391, /* level 0: rides a collision triangle (PatchRideState) */
  ACTOR_TYPE_HOMING_SPARK = 392,     /* launched at the nearest enemy in front of Spyro */
  ACTOR_TYPE_LINKED_FOLLOWER = 395, /* level 3: follows its linked actor's mode; knocked away when hit (Follower) */
  ACTOR_TYPE_FLAME_FLIGHT_TARGET = 397, /* flight levels 11/29: flamed, tallies, pays out and becomes ACTOR_TYPE_FLAME_FLIGHT_TARGET_SPENT */
  ACTOR_TYPE_PORTAL_GUIDE = 398,   /* flies Spyro through a portal (PortalState) */
  ACTOR_TYPE_FLAME_FLIGHT_TARGET_SPENT = 399, /* the flame flight target after it paid out */
  ACTOR_TYPE_SPLASH = 400, /* one-shot splash where an actor lands in water (spawned with SFX_SPLASH) */
  ACTOR_TYPE_BREAKABLE_ROCK = 401,   /* shakes when struck; a charge shatters it */
  ACTOR_TYPE_CANNON = 402, /* smokes while it builds up, fires, then flies its ball along a path */
  ACTOR_TYPE_LURKER = 403, /* level 20: drags Spyro with a tether actor (LurkerState) */
  ACTOR_TYPE_HAZARD0_DEATH_FX = 405, /* g_anHazardTimeoutSpawnType[0] */
  ACTOR_TYPE_BLASTABLE_PATH_SMOKER = 407, /* flight levels 5/29: tweens its path puffing smoke from attach point 3; a blast (ACTOR_HIT_SUPER) wrecks it (SmokerState) */
  ACTOR_TYPE_BLASTABLE_PATH_PROP = 408, /* flight levels 5/29: ACTOR_TYPE_BLASTABLE_PATH_SMOKER without the smoke */
  ACTOR_TYPE_HOPPING_CRITTER = 412, /* level 2: wanders; knocked back when hit, then recovers (HopState) */
  ACTOR_TYPE_GRAZING_CRITTER = 413, /* level 3: roams and bleats; bursts into fluff when struck (GrazerState) */
  ACTOR_TYPE_FOLLOW_FACER = 414, /* level 33: turns toward the actor it follows */
  ACTOR_TYPE_BALLOON = 416,        /* bobs about its rest height; only in balloonist levels */
  ACTOR_TYPE_REWARD_BURST = 417, /* level 6: once triggered, plays its sound and throws up to four sets of sparks */
  ACTOR_TYPE_LIFE_CHEST = 421,       /* shakes and pops its lid on a timer, spawning a 0x1A6 rider (LiftState) */
  ACTOR_TYPE_LIFE_CHEST_SHARD = 423, /* 423..425: life chest burst (423/424 bounce, 425 tumbles) */
  ACTOR_TYPE_LETTER_A = 0x1AA,     /* floating-name letters A..Z: 0x1AA..0x1C3 */
  ACTOR_TYPE_ROAMING_RAM_QUIET = 453, /* ACTOR_TYPE_ROAMING_RAM without the voice channel (level 21) */
  ACTOR_TYPE_FALLING_SPARK = 454, /* 454..456, level 6: fall until they land, time out or leave the visible set */
  ACTOR_TYPE_REBOUND_DEBRIS = 458, /* 458..459, levels 6/7/27/32: rebounds off walls and floors, spinning until its life runs out */
  ACTOR_TYPE_WAR_DRUM = 461, /* levels 18/19: beats on a timer with its own voice; knocked away when hit (WarDrumState) */
  ACTOR_TYPE_ROAMING_RAM = 466, /* wanders its waypoint ring away from Spyro, charges down its path (RoamingRamState) */
  ACTOR_TYPE_BOLT_SENTRY = 467, /* level 19: bolt-throwing sentry with three guards (BoltSentryState) */
  ACTOR_TYPE_BELL_POST = 475, /* level 18: rung by a charge or flame; resets once its animation ends */
  ACTOR_TYPE_GRABBER = 476, /* level 20: carries Spyro off and throws him (GrabberState) */
  ACTOR_TYPE_HAZARD0_ENTER_FX = 477, /* g_anHazardEnterSpawnType[0]; both kind-0 effects snap to the ground under Spyro (or the parent) */
  ACTOR_TYPE_FLIGHT_CHEST_SHARD = 478, /* 478..483: flight chest burst */
  ACTOR_TYPE_BELL_RIPPLE = 493, /* levels 7/8: ripple a charged bell (206/239/242) spawns; removed when its animation ends */
  ACTOR_TYPE_AIMED_BOLT = 494, /* level 19: bolt thrown by 35 and 467 (AimedBoltState) */
  ACTOR_TYPE_BLOWHARD = 495, /* level 16 boss: patrols three paths, drops type 37 riders */
  ACTOR_TYPE_HUMMING_BELL = 497, /* level 16: loops a voice while alive, rings out when struck */
  ACTOR_TYPE_SWAYING_DEBRIS = 502, /* level 3: bouncing debris that sways as it falls (BounceState) */
  ACTOR_TYPE_HAZARD2_ENTER_FX = 507, /* g_anHazardEnterSpawnType[2]; level 32 also spawns it where a knocked actor lands on ground surface 1..2 */
  ACTOR_TYPE_HAZARD2_DEATH_FX = 508, /* g_anHazardTimeoutSpawnType[2] */
};

/* RefreshActorDerivedState's mode bits. */
#define ACTOR_REFRESH_COLLISION 1 /* ->collision from the current animation frame's record */
#define ACTOR_REFRESH_GRID 2      /* relink ->gridCell from the X/Y position */
#define ACTOR_REFRESH_MATRIX 4    /* ->mtx from the rotX/rotY/rotZ euler bytes */

/* PlaySoundEffect's mode: the voice row's flag group. */
#define SFX_MODE_FLAT 4     /* flat volume; no re-trigger while the owner plays it */
#define SFX_MODE_3D 8       /* panned and attenuated from the owner's position; dropped
                             * outside its audible range */
#define SFX_MODE_PANNED 0x10 /* pan supplied by the caller */

/* StopSoundVoicesByOwner's mode: which of the owner's voices to stop. */
#define VOICE_STOP_ALL 1     /* every voice it owns */
#define VOICE_STOP_LOOPING 2 /* only its looping voices (row flag 0x100) */

/* SpawnActorDeathDrop's `kind`: how a thrown drop leaves its source. Rolled
 * extra lives, butterflies and Sparx ignore it. */
enum DropThrow {
  DROP_HOP = 0,             /* lands on the source with a low hop */
  DROP_SCATTER = 1,         /* lands at a random point within 0x200 of the source */
  DROP_AT_POINT = 2,        /* lands on the `alt` position */
  DROP_TUMBLE_IF_NEAR = 3,  /* DROP_TUMBLE when Spyro is charging close by, else DROP_SCATTER */
  DROP_TUMBLE = 4,          /* rides off the source with a random tumble (subMode 3) */
  DROP_IN_PLACE = 5,        /* appears on the source with no velocity */
  DROP_HIGH_HOP = 6,        /* lands within 0x100 of the source with a high hop */
};

/* Waypoint path used by the path-following classes (10, 114, 339) and
 * FollowActorWaypointPath / PivotActorAlongPath: node count, current node,
 * direction, then 0x10-byte nodes that begin with a position. */
typedef struct PathData {
  unsigned char count; /* 0x00 */
  unsigned char cur;   /* 0x01 */
  char pad02[4];
  short reversed;      /* 0x06 -1: walk forward (wrap to 0), else backward */
  struct {
    int x;
    int y;
    int z;
    int speed; /* > 0 overrides the follow speed at this node */
  } nodes[1];          /* 0x08 */
} PathData;

/* UpdateActorWander's behaviour block (the head of the wandering classes'
 * state, e.g. level 32 type 236). Radii are stored >> 10. */
typedef struct WanderBlock {
  int homeX;                     /* 0x00 home point */
  int homeY;                     /* 0x04 */
  int pad08;                     /* 0x08 */
  unsigned char speed;           /* 0x0C move speed (x1.5 / x2 at frame delta 3 / 4) */
  unsigned char turnRate;        /* 0x0D */
  unsigned char contact;         /* 0x0E move contact group, also the probe radius */
  unsigned char repickMin;       /* 0x0F re-pick countdown range */
  unsigned char repickMax;       /* 0x10 */
  unsigned char jitterMin;       /* 0x11 heading jitter range */
  unsigned char jitterMax;       /* 0x12 */
  unsigned char leash;           /* 0x13 walk home once farther than this */
  unsigned char weaveAmp;        /* 0x14 0 = no weave */
  unsigned char weaveRate;       /* 0x15 */
  unsigned char pad16;           /* 0x16 */
  unsigned char repickTimer;     /* 0x17 frames to the next heading jitter */
  unsigned char heading;         /* 0x18 8-bit angle */
  unsigned char weaveDir;        /* 0x19 +1 / -1 */
  unsigned char chaseRadius;     /* 0x1A 0 = only the fixed 0x44C range */
  unsigned char chasing;         /* 0x1B */
  unsigned char chaseCooldown;   /* 0x1C set after a wall bump or a walk home */
  unsigned char contactCooldown; /* 0x1D moves ignore the contact group while set */
  short weave;                   /* 0x1E current weave offset */
} WanderBlock; /* 0x20 */

#endif /* OPEN_SPYRO_ACTOR_TYPE_H */
