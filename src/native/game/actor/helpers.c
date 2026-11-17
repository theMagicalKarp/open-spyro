/* Native replacements for src/main/actor/helpers.c definitions. */

struct Actor;

/* 0x80037e98: empty stub. Natively one variadic signature (funcs.h, actor.h)
 * serves every caller; wasm traps on a call whose arguments differ from the
 * definition. */
void OnActorAnimQueued(struct Actor *actor, ...) {}
