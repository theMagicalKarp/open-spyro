/* Native definitions for the LevelId<id>_* hook entry points
 * (include/level_entry_points.h).
 *
 * Level ids 0-9 and 99 install entry points inside overlays that are not in
 * WAD.WAD (ApplyPerLevelGlobalsTable), so they exist only as PS1 addresses.
 * The retail game never loads those ids; natively each entry point logs and
 * aborts if it is ever called. */

/* Game C is built -nostdinc; the module libc (src/native/wasm/libc.c). */
int printf(const char *fmt, ...);
void abort(void);

struct Actor;

#define UNMAPPED(name)                                                         \
  do {                                                                         \
    printf("native: %s: the overlay for this level id is not on the disc\n",   \
           name);                                                              \
    abort();                                                                   \
  } while (0)

void LevelId1_UpdateActors(void) { UNMAPPED("LevelId1_UpdateActors"); }
void LevelId4_UpdateActors(void) { UNMAPPED("LevelId4_UpdateActors"); }
void LevelId9_UpdateActors(void) { UNMAPPED("LevelId9_UpdateActors"); }
void LevelId3_UpdateActors(void) { UNMAPPED("LevelId3_UpdateActors"); }
void LevelId99_UpdateActors(void) { UNMAPPED("LevelId99_UpdateActors"); }
void LevelId2_UpdateActors(void) { UNMAPPED("LevelId2_UpdateActors"); }
void LevelId7_UpdateActors(void) { UNMAPPED("LevelId7_UpdateActors"); }
void LevelId0_BeginActorCutscene(struct Actor *actor) {
  UNMAPPED("LevelId0_BeginActorCutscene");
}
void LevelId0_OnSaveMenuExit(void) { UNMAPPED("LevelId0_OnSaveMenuExit"); }
void LevelId0_BalloonistCutscene_Update(void) {
  UNMAPPED("LevelId0_BalloonistCutscene_Update");
}
struct Actor *LevelId1_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId1_SpawnActor");
}
struct Actor *LevelId4_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId4_SpawnActor");
}
struct Actor *LevelId9_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId9_SpawnActor");
}
struct Actor *LevelId3_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId3_SpawnActor");
}
struct Actor *LevelId99_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId99_SpawnActor");
}
void LevelId1_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId1_AdvanceEmitList");
}
void LevelId4_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId4_AdvanceEmitList");
}
void LevelId9_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId9_AdvanceEmitList");
}
void LevelId3_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId3_AdvanceEmitList");
}
void LevelId99_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId99_AdvanceEmitList");
}
void LevelId1_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId1_EmitParticles");
}
void LevelId4_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId4_EmitParticles");
}
void LevelId9_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId9_EmitParticles");
}
void LevelId99_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId99_EmitParticles");
}
void LevelId3_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId3_EmitParticles");
}
void LevelId0_BalloonistCutscene_Draw(void) {
  UNMAPPED("LevelId0_BalloonistCutscene_Draw");
}
void LevelId0_UpdateActors(void) { UNMAPPED("LevelId0_UpdateActors"); }
struct Actor *LevelId2_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId2_SpawnActor");
}
struct Actor *LevelId7_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId7_SpawnActor");
}
void LevelId2_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId2_AdvanceEmitList");
}
void LevelId7_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId7_AdvanceEmitList");
}
void LevelId7_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId7_EmitParticles");
}
void LevelId2_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId2_EmitParticles");
}
struct Actor *LevelId0_SpawnActor(int type, struct Actor *parent) {
  UNMAPPED("LevelId0_SpawnActor");
}
void LevelId0_AdvanceEmitList(int frameStep) {
  UNMAPPED("LevelId0_AdvanceEmitList");
}
void LevelId0_EmitParticles(int count, int type, void *pos, int effectArg) {
  UNMAPPED("LevelId0_EmitParticles");
}
