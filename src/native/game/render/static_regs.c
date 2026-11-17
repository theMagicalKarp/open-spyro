/* Native replacement for src/main/render/static_regs.c. */

/* 0x800229e8: the PS1 version stores $v0 through $at, then reloads ra, fp,
 * sp, gp and s0-s7 from g_anStaticRegSaveArea and returns through the
 * restored ra: a longjmp back into hand-written asm. Nothing calls it (xref);
 * there is no native equivalent, so reaching it is a bug. */
void StoreAndRestoreStaticRegs(void) { __builtin_trap(); }
