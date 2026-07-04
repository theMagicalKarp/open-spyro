# Matching cookbook — proven idioms, known walls, process gotchas

Distilled from every match session so far (~215 matched functions). Read this **before**
attempting any function: most "stubborn diffs" are one of the idioms below, and most
time-sinks are functions on the skip list. The toolchain is gcc 2.7.2 → maspsx → GNU as
(locked, `config/compile.sh`); everything here is specific to that chain.

## A. Proven source idioms (diff symptom → fix)

| Symptom in the byte diff | Fix in the C source |
|---|---|
| Store slides into a `jal`/`jr`/branch delay slot (original has `sw; jal; nop`) | Make the store volatile: `*(volatile int *)g_pReg = x;`, or declare the pointed-to type volatile (`extern volatile T *p;` — any local copy must stay `volatile T *` too). |
| Original materializes a global block's address **once** (`lui;addiu` then `lw/sw off(rX)`), gcc emits one `lui` per access | Declare the base as an **incomplete array**: `extern int g_block[];`, access `g_block[0]`, `&g_block[N]`. Works reliably when at least one address is passed to a call. Add the alias symbol to `config/symbol_addrs.txt` (two symbols at one address is fine). |
| Store-then-reload of one scalar global on a single base reg | Alias-array trick: add `g_anXxxBlock` at the same address, then `extern volatile int g_anXxxBlock[]; volatile int *p = g_anXxxBlock; *p = k; f(*p, ...)`. Direct `&scalar` always folds back to `$at` form. |
| Missing load-delay `nop` after `lw/lhu` (function 4 bytes short) | **Retired 2026-07-03**: the historical -g3 `.L_LC` line-label nop drop was a from-source-cc1 artifact; the pinned prebuilt cc1 emits `.loc` (which maspsx ignores) and the nops survive. Multi-line load/use is fine now (proven: `GetSoundVoiceStatusByOwner` un-collapsed, `MemCardLoad` matched with a `lw; nop; bnez` head). If you ever see a genuinely dropped nop, capture the cc1 `.s` before assuming this class is back. |
| `sltiu v0,v0,1` wanted for a zero test | `return f() == 0;` |
| `slt` (reg bound) where you emit `slti` (immediate) | Put the loop bound in a local: `int n = 24; while (i < n)` — gcc 2.7.2 does not const-propagate it. |
| `sltiu` (unsigned cmp) on a signed-divide result | `unsigned int half = (w*h + 1) / 2;` — RHS stays signed (keeps the `sra` divide), unsigned lvalue makes the compare `sltiu`. |
| Commutative `addu`/`or` operand order swapped | For pointer+scaled-index: write the offset as an explicit shift — `*(unsigned int *)(p + (len << 2))` flips gcc to base-first. For `or` result order: **reuse one variable across roles** (entry-test temp → result half), see `BuildDrawModeWord.c`. The permuter finds these (`perm_temp_for_expr`). |
| Loop pre-test present where original has none | Use `do { } while`, not `while`. |
| Call-arg loads in wrong order | Explicit temps: `int arg0 = V1; int arg1 = V2; fn(arg0, arg1);`. |
| Redundant `move`/`andi 0xFF` masks around a char param | `unsigned char c` param **plus** a separate `unsigned char r` local — an `int` param coalesces the copy away. |
| Only some arg regs set per call site | Unprototyped extern: `extern unsigned int f();`, call with exactly the args the asm loads. |
| Switch bodies laid out in the "wrong" order | gcc emits compares in ascending case value but **bodies in source order** — reorder the `case` blocks in the source to match the original layout. |
| Vararg printf-style wrapper: arg homing scheduled into the body | Do **not** use real `...`. Named int params `(char *fmt, int arg1, int arg2, int arg3)`, pass `&arg1`, force addressability with `(void)&fmt;` etc. |
| Unused 8-byte stack frame on a small leaf | Usually a `while (n--) *p++ = 0;`-shaped loop (also with a pre-loop store). |
| Global-array loop, original uses absolute `$at` addressing per iteration | `*(T *)((char *)g_arr + off + k)` with `off` a plain variable. With TWO strides, compute each byte offset into its own local first (`int o1 = i*12; int o2 = i*88;`). `.o` objdump shows `%hi(arr)`/`k($at)` vs original `%hi(arr+k)` — these link identical; trust `make check`, not the `.o` dump. |
| Memcard/libcd pending-op globals reordered by the scheduler | Declare them `volatile` (whole op-block: `D_80075B50..98`, `D_8007515C..78`). Volatile-array `[0]` form ⇒ base-reg reuse; volatile scalar ⇒ split `%lo` form — pick per the asm. Chained `a=b=c=d=0` reproduces the store→reload cascade. |
| Global RELOADED right after a branch where your build reuses the register (extra lui/lw in the original, e.g. `D_80075B4C` in `func_8006635C`) | Make that global `volatile` — a non-volatile global CSEs across the extended basic block (fallthrough into an if-body); volatile forces the original's reload. Check the counter-signal first: a value reused in-register after a store (e.g. `AF8+1; sw; slti` with no reload) means that one is NOT volatile. |
| Early `return 0;` block emitted at the END of the function (yours: `beqz → ret0-at-end`; original: `bnez → main` with `j exit; delay v0=0` inline after the test) | gcc relocates early-`return` blocks next to the epilogue. Write the if/else **result-variable** form instead: `if (p == 0) code = 0; else { ... code = expr; } return code;` — keeps the zero arm inline (proven: `BuildTextureWindow`). |
| `return 0` inside a switch case copies `move v0,zero` into a load-delay slot; original jumps to a shared zero+epilogue tail | Use `break` (falling to the single function-end `return 0;`) instead of `return 0;` in the case — with `break` there is no per-path v0=0 copy for the scheduler to hoist (proven: `func_8006635C` case 10). |
| Global pointer (dispatch table etc.) loaded BEFORE a small if/branch in the original, after it in yours (yours gains a load-delay `nop`) | Read the pointer into a local **above** the conditional: `tbl = (char *)g_pGpuDispatchTable; cmd = A; if (m) cmd = B; (*(fn **)(tbl + OFF))(cmd);` (proven: `SetDispMask`). |
| `A \| B \| CONST \| ...` OR-chain associativity wrong (constant attaches to the wrong operand) | Parens do NOT survive — gcc reassociates and floats the constant. Split the grouping into temps, one statement per grouping, ordered as the original computes them: `y = t1 << 15; r = (t0 << 10) \| 0xE2000000; code = y \| r \| ...` (proven: `BuildTextureWindow`). |
| Unused 0x10 stack frame + dead `sw`s of loop-free locals | Locals declared as an ARRAY (`int t[4];`) — assignments stay as stores (aggregates aren't dead-store-eliminated), reads CSE back to registers (proven: `BuildTextureWindow`'s get_tw). |

### Reliable veins (deterministic, match first try)

- **Dispatch wrappers**: `jal Helper` + indirect call through `g_pGpuDispatchTable` /
  `g_pLibapiSysVtable` (`(*(T(**)(...))((char*)tbl + OFF))(...)`). The ABI pins registers →
  no regalloc ambiguity. Grep the asm for this shape and do these first.
- **Debug-trace wrappers**: `if (g_bGpuDebugLevel >= 2) ((void(*)(...))g_pfnGpuDebugPrintf)(D_str, ...); <work>`.
  Format strings are `extern char D_xxxxxxxx[];`.
- **Straight `jal` chains** (call A; call B; call C) — transcribe calls + delay-slot arg setup in order.
- **RECT-build + libgpu call**: local `RECT rect;` (types.h), assign fields, call — gcc places
  it at the right frame offset on its own.
- **libc BIOS-style leaves**: `while (*s++) n++;` etc.

## B. Skip list — do NOT attempt (toolchain cannot emit these)

1. **`%gp_rel` global access** in the original (`sw $r,%gp_rel(sym)($gp)`). Our externs always
   expand to `lui $at,%hi; sw %lo($at)`. Cluster ~0x80075600–0x80075950; grep `%gp_rel` in the asm.
2. **`$at` literal-address HW load** (`lui $at,hi; lhu $v0,lo($at)`, no reloc — GNU as macro for
   a literal MMIO address). No C form produces it.
3. **RETIRED — unaligned `lwl/lwr` struct copy.** The pinned prebuilt cc1 emits the LE-adjusted
   offsets (`lwl 3 / lwr 0`) directly; the old "BE convention" observation was a from-source-cc1
   artifact. Proven: `FormatMemCardPath` (0x80067d74) is now **matched** with a plain 6-byte
   struct assignment.
4. **RETIRED — load→branch at a statement head under -g3** (`lw; nop; bnez`, the `.L_LC`
   line-label nop drop). Same root cause and resolution as above; the pinned cc1 emits `.loc`
   directives, not `.L_LC` labels, and maspsx ignores `.loc`. Proven: `MemCardLoad`
   (0x800665b8) is now **matched**. Former blocked list (`FUN_80065270`, `func_8006635C`,
   `SetDrawMove`) — all worth re-attempting.
5. **libgpu AddPrim mask family** (`*p=(*p&0xff000000)|(*q&0xffffff)`): mask constants land in
   swapped arg regs and no source reordering flips it — these were built with ccpsx, not gcc.
6. **Split-label functions** (one function wrapped into multiple `.text.<name>` sections by
   seeded mid-function symbols, e.g. `SpuGetVoiceVolume`, `memchr`): a C override overflows its
   anchor slot. Needs a layout merge in `config/text_layout.json` first.
7. **Handwritten asm** (`handwritten` tag in progress) and the 35 level **moby megafunctions**
   (`blob` tag) — out of scope for a match session.
8. **Shared-base pure load/store with no calls** (e.g. `AdvanceSpyroSpeedTowardTarget`,
   `SetGraphDebug`): the array trick needs a call to force the base register; without one gcc
   folds per-access. Permuter-or-never.
9. **`.rodata` switch jump tables** (`jr` through `switchdataD_*`): a C override may emit no
   loadable `.rodata`, and gcc regenerates its own table for a dense switch. Blocks the memcard
   `jr`-table handler family: `func_80066F34`, `func_80066634`, `func_800671F0`, `func_80067718`
   (same class as the parked `TitlescreenUpdate`). Needs a jtbl-placement story first.
10. **get_cs/get_ce clamp family** (`BuildDrawAreaTopLeft` 0x80060bc8, `BuildDrawAreaBottomRight`
   0x80060c94): original keeps w/h base copies, recomputes `w-1` instead of CSE, evicts the
   param from a0 and joins through v0 — five structural attempts (ternary/statement/comma
   forms, short params, swapped arms) all get canonicalized back by our gcc. Suspect ccpsx
   like the AddPrim family (§B5); the simpler neighbors (`BuildDrawModeWord`,
   `BuildDrawOffsetCommand`) DO match, so the wall is shape-specific. Permuter-or-never.
11. **`SetDrawMove`** (0x8005efe0): pure regalloc tie — original homes the packet pointer to t0
   (`move t0,a0` at entry) and gives a0 to `len`; our gcc always coalesces the higher-ref
   pointer into a0 (tried: local copy, `register`, s8 len, param role-reuse — the last inverts
   both regs). 1-decision residue; permuter candidate.

## C. Process gotchas (each of these has burned a session)

- **Never trust a host-side `xxd`/`cmp` right after a docker build** — the volume mount can
  serve the *previous* EXE (false match; caused two broken commits). The gate is `make check`.
- **`make check 2>&1 | tail && git commit` commits on failure** — a pipeline's status is the
  last stage. Use `make check > /tmp/check.log 2>&1 && git commit ...`, or `set -o pipefail`.
- **File offset formula** for the main EXE: `0x800 + (VMA - 0x80010000)`. Cross-check with
  `cmp -l built orig | head` (offsets are 1-based). An all-zero region at the VMA means the
  override was **dropped**, not matched.
- **A `.text.<name>` section can hold more than one function** (later ones had no Ghidra
  symbol). Compare the `INCLUDE_ASM` block size against your function's instruction count; if
  bigger, define every function in the block in the one `.c`, or the tail zeroes out.
- **No loadable `.rodata`/`.data` in overrides** (`build_main.sh` rejects it): no string
  literals, no initialized arrays, no jump tables that spill (`switch` is fine when gcc keeps
  it as compares). Reference ROM data via `extern` symbols.
- **`globals.h` does not include `funcs.h`** — every called function gets an explicit `extern`
  decl in the `.c` (house style; see any recent `src/c/*.c`).
- Overlay C compiles **without -g3** (`compile.sh ovl`); main keeps -g3. Don't fight overlay
  line-label issues that don't exist.
- The old reference checkout at `~/code/moonshots/open-spyro` (~419 C files) is **hints only**
  — its sources predate the locked toolchain and several do not match here. Never copy
  wholesale; always re-derive and re-verify. (Clean-room rule: `~/code/spyro-1` /
  MobyCollective sources are off-limits entirely.)

## D. Per-function fast loop (commands)

```bash
# 0. Once per session — regenerate the m2c context (real types/globals/func sigs):
make ctx

# 1. First pass (works directly on the monolithic text.s; ~2 s, context is cached):
uv run --project tools/open-spyro -- m2c --target mipsel-gcc-c \
    --context build/ctx.c -f <Name> asm/text.s 2>/dev/null | grep -v '^Note: assuming HAVE_C'

# 2. THE inner loop — one command: incremental rebuild + linked-byte diff at the VMA
#    (side-by-side instructions + match %; exit 0 = identical, 1 = differs).
#    Runs entirely inside one docker call, so it is immune to the stale-mount trap (§C).
uv run --project tools/open-spyro --group split -- open-spyro diff <Name>   # ~9 s
#    --full: every instruction; --no-build: reuse the current build output.

# 3. Final gate before commit (full verify across main EXE + 37 overlays + WAD):
make check          # 0 diffs, or it is not a match

# (Manual fallback: compile one override + objdump it without a link —
#  tools/docker_env.sh bash -c 'bash config/compile.sh main src/c/<Name>.c /tmp/f.o \
#    && mips-linux-gnu-objdump -dr /tmp/f.o'
#  — but remember reloc'd operands (%hi/%lo pairing) can differ in the .o yet link
#  identical; `open-spyro diff` compares post-link bytes and has no such caveat.)
```

## E. Permuter harness (proven recipe)

Use only when logic is right and the diff is a pure regalloc/schedule tie (1–3 insns).
Host runs the permuter, docker compiles:

1. `build/perm/<fn>/target.o` — extract original bytes at `0x800+(VMA-0x80010000)`, emit a
   `.word` `.s` under `.globl <fn>`, assemble with docker `mips-linux-gnu-as -EL -march=r3000`.
   (Target has resolved relocs ⇒ every candidate carries a constant reloc penalty; a candidate
   at exactly that floor **is** the match — `--stop-on-zero` never fires, watch `output-*`.)
2. `base.c` — self-contained C89 (plain externs, no `globals.h`); pycparser must parse it.
3. `compile.sh` wrapper — copy `$1` into `mktemp -d` under `build/perm/tmp/` (docker only
   mounts the repo), run `tools/docker_env.sh bash config/compile.sh main <rel>/in.c <rel>/out.o`.
4. `settings.toml` — `func_name = "<fn>"`, `compiler_type = "gcc"`.
5. Host objdump shim at `build/perm/bin/mips-linux-gnu-objdump` proxying into docker; run
   `PATH="$PWD/build/perm/bin:$PATH" uv run --project tools/open-spyro -- python tools/decomp-permuter/permuter.py --best-only build/perm/<fn>`.

Treat permuter output as a **hint about which transform class closes the gap** (temp reuse,
statement order), then write clean source that matches — don't commit permuter spew, and don't
iterate your own C against permuter noise (that's a known doom-loop).
