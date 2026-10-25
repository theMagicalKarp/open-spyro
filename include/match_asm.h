/* match_asm.h — the zero-code idioms (empty asm statements and empty loops) the matched C uses to steer
 * gcc 2.7.2's optimizers.
 *
 * Each macro expands to an empty asm statement: no instruction is emitted,
 * only what cse / sched / the register allocators may assume changes. They
 * are load-bearing; removing one changes the output (cookbook id per macro). */

#ifndef OPEN_SPYRO_MATCH_ASM_H
#define OPEN_SPYRO_MATCH_ASM_H

/* A382: a scheduling barrier; no insn is moved across it in either direction. */
#define SCHED_FENCE() __asm__ volatile("")

/* A383: dst = src through an opaque copy. The tied constraint keeps both in
 * one register, but cse / combine / reload can no longer see through it. */
#define OPAQUE_COPY(dst, src) __asm__("" : "=r"(dst) : "0"(src))

/* A383 in place: v keeps its value but becomes opaque from here on. */
#define OPAQUE(v) OPAQUE_COPY(v, v)

/* A382 / A433: v must be computed, in a register, at this point. */
#define KEEP_IN_REG(v) __asm__ volatile("" : : "r"(v))

/* A384: v is born here with no defined value (a register reservation). */
#define REG_RESERVE(v) __asm__("" : "=r"(v))

/* A200 / A232: an empty loop. Its loop notes end cse's extended basic block
 * (so a held base is not folded back into absolute addresses) and split the
 * scheduler's basic block (nothing moves across it). */
#define EBB_BREAK() do { } while (0)

#endif /* OPEN_SPYRO_MATCH_ASM_H */
