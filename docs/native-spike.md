# Native port feasibility spike

Date: 2026-11-13. Scope: answer the two unknowns that size a native macOS arm64 build of the
matched C on PsyZ: what loaded level data holds, and how much shared code a
64-bit build would touch. Nothing in the PS1 build changed for this spike.

## Verdict

**GO, but not with a 64-bit (LP64) data model.** Build the game C with **32-bit pointers
(ILP32)**, with PS1 RAM mapped at its real addresses (`0x80000000`). On macOS arm64 the practical
ILP32 route is **`wasm32` → `wasm2c` → clang arm64**: the output is a native executable (no
interpreter, no MIPS emulation), PsyZ already builds for wasm32, and the game C compiles under
ILP32 with no pointer-width diagnostics.

| Measure (261 game `.c` files, clang syntax-only) | arm64 (LP64) | wasm32 (ILP32) |
|---|--:|--:|
| Files that fail to compile | 222 | 11 |
| Errors | 270 (220 are the `Actor` size assert) | 23 |
| Unique pointer↔int cast / truncation diagnostics | 1,571 | **0** |
| `ActorSizeCheck` (0x58-byte `Actor`) | fails | passes |

An LP64 build would need a 32-bit handle type at every *stored* pointer: about
6,600 use sites of 149 pointer-typed globals, plus the pointer members of 111 structs. Those
structs are read straight from disc or sit at fixed RAM offsets. That is a fork of the shared
code, not a macro sweep. A C++ `ps1ptr<T>` wrapper avoids the use-site rewrites, but the game C
fails as C++ with 3,883 errors before any pointer work. ILP32 avoids both.

The one thing ILP32 needs that this machine does not have is a wasm toolchain. Apple clang 21
has no WebAssembly backend; it parses `-target wasm32` but cannot emit code. The port therefore
starts with a gate: a toolchain smoke test (see Strategy below).

---

## WAD level-data pointer audit

**Answer: (b) — offsets relocated in place at load time.** But the data also depends on things
that (b) alone does not cover: fixed 32-bit layouts, KSEG0/KUSEG address bits, and addresses
packed into 21-bit fields. In practice that puts it in **(c)**: the 64-bit options all need a
2 MiB arena with 32-bit addresses. Identity-mapped ILP32 makes all of it work as written.

### Relocation sites

All of them are in `src/main/load/`. Each adds a section base to offsets stored on disc.

| Function | What it rebases |
|---|---|
| `UnpackWorldDataChunks` (`level.c:66`) | world-chunk table, vis-zone lists + sky-zone lists, trigger regions, the 5 `WorldCollisionHeader` offsets, level-sky chunks, each `Portal`'s `pSky` + sky chunks; emit-texture table entries are published as pointers into the blob |
| `RelocateLevelSampleBank` (`level.c:25`) | sample-bank entries: adds the SPU RAM base (`SPU_SAMPLE_BASE`). This is an SPU address, not a CPU pointer |
| `BuildLoadedLevelRuntime` (`level.c:415`) | 7 world-anim lists, pickup table, **generic relocation list** (below), actor pool base (`g_pActorListBase` points *into the blob*) |
| `RelocateActorMeshHeader` (`level.c:332`) | `Model`: 8 `m_CollisionModels` (rebased as **KUSEG**, `table & 0x7FFFFFFF`), `m_Data`, every `AnimationHeader`'s face/colour/vertex pointers, and per-frame words (see packing below) |
| `InstallSpyroAnimStream` (`anim_stream.c:90`) | Spyro anim frames, same 21-bit packing |
| `TickCutsceneBundleLoadStream` `CSTREAM_RELOCATE` / `CSTREAM_REBASE_PATHS` (`cd_stream.c:167–212`) | actor mesh headers of a streamed bundle, path table |
| `TickLevelTransitionStream` (`cd_stream.c:488`) | re-relocates level actor meshes on transitions (`& 0xFFFFFF` range check at `:693`; the same check is in the rescue stream at `:436`) |
| `TickDragonRescueAssetStream` (`cd_stream.c:297–379`) | statue and mirror models |

### The generic relocation list

A section after the actor spatial grid lists byte offsets from the archive root. Each listed word
gets the root address added. Measured on all 35 levels in `WAD.WAD`:

- **6,005 entries in total.** 4,877 are `Actor.state` (+0x00), one per placed actor. The other
  1,128 are pointers *inside* actor state blocks, and every one of them points into the
  state-block area.
- Those 1,128 fall on **142 distinct (actor type, state offset) fields**: path pointers, partner
  links and so on. The most common are `AMBIENT_SOUND`+0x08 (73), `PORTAL_GUIDE`+0x00 (30) and
  `POST_GRABBER`+0x10 (30).
- The list carries no type information. The loader cannot know which struct a word belongs to,
  so a 64-bit loader cannot widen these fields at load time. A translation pass would have to
  rebuild the 142-field type map from the overlays.

### Disc-resident structs with pointer fields

These are read from disc or carved from the level blob, so their layout is fixed:

| Struct | Pointer fields | Notes |
|---|---|---|
| `Actor` (0x58, `actor_type.h`) | `state`, `gridNext`, `collision` (int holding a pointer) | lives in the loaded blob; `collision < 0` tests "is a KSEG0 pointer"; `ActorSizeCheck` asserts 0x58 |
| Actor state blocks (25+ typed views) | 142 (type, offset) fields from the reloc list | pointers into the state area |
| `Model` (`model.h`) | `m_CollisionModels[8]` (KUSEG), `m_Data`, `m_Animations[]` | |
| `AnimationHeader` | `m_AnimationVertices`, `m_Faces`, `m_Colors`, `m_LpFaces`, `m_LpColors` | frame words hold packed addresses |
| `Portal` | `pSky` | |
| `SkyHeader` | `chunks`, `zoneLists` | |
| `WorldCollisionHeader` | `xIndex`, `yzIndex`, `tris`, `visZone`, `groundCode` (ints holding pointers) | |
| world-chunk / vis-zone / trigger / anim lists, pickup table, emit-texture table | pointer arrays | |

### Address tricks that rule out a plain 64-bit loader

- **Packed 21-bit addresses.** Spyro anim frame words store `(address >> 1) & 0x1FFFFF` in their
  low 21 bits (`level.c:376`, `anim_stream.c:102`). Actor frame words add `m_Data & 0x1FFFFF`.
  Both need the data to sit in a ≤4 MiB space addressed by PS1-physical offsets.
- **16-bit word-delta links** in actor anim frames (`((DATA - w + 4) >> 2) & 0xFFFF`). These are
  relative, so they survive any arena.
- **KSEG0 sign tests and KUSEG mirrors.** `Actor.collision < 0` means "hittable". Collision
  models are stored as KUSEG (`& 0x7FFFFFFF`) addresses and dereferenced through the mirror.
  `cd_stream.c:436` compares `ptr & 0xFFFFFF`.
- **Fixed placement relative to the top of RAM.** The mesh directory is placed below `RAM_END`
  (`0x80200000`, `level.c:444`), `init.c:346` uses `0x801C0000`, and `DEMO_RECORD_BUFFER` is
  `0x80600000`, the 8 MiB dev-kit RAM (record mode only).

### macOS arm64 rules out a low identity mapping

Tested: `mmap(0x80000000, MAP_FIXED)` fails under the default 4 GiB `__PAGEZERO`. A binary linked
with `-pagezero_size 0x4000` is SIGKILLed at exec. A 64-bit native build therefore cannot simply
place PS1 RAM at its PS1 addresses. Under wasm32 the address is just a linear-memory offset, so
`0x80000000` works there. That needs 2 GiB + 8 MiB of reserved (not committed) memory; wasm32
allows up to 4 GiB.

---

## Clang compile-only pass

Command, per file, with a throwaway `-include` stub that defines `M2CTX` (blanks `INCLUDE_ASM`)
and `#define asm(x)` (blanks the 300 `register … asm("$n")` pins):

```
clang -target <arm64-apple-macos|wasm32-unknown-unknown> -fsyntax-only -std=gnu89 \
  -nostdinc -Iinclude -Iasm -funsigned-char -include port_stub.h \
  -Wpointer-to-int-cast -Wint-to-pointer-cast -Wshorten-64-to-32
```

The `match_asm.h` idioms (empty `__asm__` statements with `"r"` / `"=r"` / `"g"` constraints)
compile unchanged on both targets.

### Buckets (arm64 / LP64; diagnostics de-duplicated by source location)

| Bucket | Sites | Main / overlays | Note |
|---|--:|--:|---|
| Offsetof idiom `(int)&((S *)0)->f` via `ACTOR_OFFSET` / `ACTOR_RAW` / `MODEL_*` / `ANIM_HEADER_*` / `SAVE_OFFSET` / `PAD_*` / `FIELD_RAW` … | 571 | 253 / 318 | `offsetof` form on native fixes it; it vanishes under ILP32 |
| Pointer stored in / rebuilt from an int | ~907 | 215 / 692 | ~239 pointers passed in `int` params (e.g. `g_pfnLevelEmitParticles(…, (int)vel)`), ~95 stored into int vars/fields, ~94 address arithmetic through `int`, ~91 int slots reread as pointers, the rest mixed |
| 64→32 truncation | 72 | 5 / 67 | almost all `actor - g_pActorListBase` (`ptrdiff_t` → `int`); harmless |
| 24/21-bit address masks, KSEG0 sign tests | 21 | 20 / 1 | `cd_stream.c` mesh-table range checks, `state.c:3438` `ptr < 0` |
| GPU packet tag / OT link | small in C | — | 25 `PRIM_TAG` stores in 13 files; 16 `AddPrimToOT` calls in 3 files. The OT/packet heavy lifting is all hand-written asm (see the hand-written asm inventory). PsyZ's `O_TAG` is `u_long` and its 64-bit build widens it, so on LP64 every packet struct in `gpu_prim.h` changes size; under ILP32 none do |
| **Total** | **1,571** | | **0 under ILP32** |

### Hard-coded addresses

- Scratchpad (`0x1F800000`): game C touches it in **one** place (`math.c:250`). Everything else
  is in hand-written asm.
- HW registers: 234 `0x1F801xxx` externs in `globals.h`. **None** is referenced by game C any
  more; they were SDK-only.
- KSEG0 literals: `RAM_END`, `0x801C0000` (`init.c:346`), `DEMO_RECORD_BUFFER`.

### Overlapping / layout-dependent globals

Game C references 1,084 RAM symbols. It assumes the PS1 data layout in several ways:

- **121 same-address alias groups** (251 symbols), e.g. `g_pLevelSampleBankEntries` /
  `g_pActiveCutsceneSoundDef` / `g_apSampleBankEntriesBlock`.
- **46 symbols inside another symbol's extent**, e.g. `g_abGameOverRespawnYawVirtualBase` in
  `g_anGameOverRespawnPosTable`, and `g_pCdReadDstBuffer` in `g_abCdlLocScratchBlock`.
- **128 `*Block` / `*VirtualBase` views**, plus cross-symbol pointer arithmetic, e.g.
  `textureCountRef - 7` is `g_nWorldChunkCount` and `skyBg - 0x10` is a count
  (`UnpackWorldDataChunks`).
- Initialized data is still splat asm (`asm/data/`). `.data` holds 343 RAM-pointer words, 26 of
  them code addresses (3 tables).

The fix is the same for both data models. Natively, every game global lives at its PS1 address.
Generate a header from `config/spyro.main.syms.ld` (`#define g_nFoo (*(int *)0x800757d8)`), and
load the original `.rodata` / `.data` images from the user's `SCUS_942.28` (and each overlay's
image from `WAD.WAD`) into place. Under ILP32 the pointer words in that data are already correct.
Only code addresses need patching, through a generated PS1-address → native-function table.
There are 64 local `extern` lines in 29 `.c` files; the macro approach needs them hoisted or
guarded.

### Other errors (both targets unless noted)

| Error | Files | Fix |
|---|---|---|
| file-scope `register … asm("$n")` (13) and `goto *ra` | `render/static_regs.c`, `render/ot.c` | native replacements in `src/native/` (these are the asm-adjacent register tricks) |
| `OnActorAnimQueued` declared `()` in `funcs.h` and `(Actor *, ...)` in `actor.h` | 5 TUs | one prototype; a native-only guard if the PS1 call sites depend on the K&R form |
| `volatile` redeclarations (`g_bSpyroPersistentEulerZ`, `g_nLightSweepAngle`) | 4 TUs | `#ifdef __psyz` or unify qualifiers |
| implicit declaration `OrPickupGroupActorFlags`, `DespawnPickupGroupActors` (arm64 only: Apple makes it an error) | `level_22…/update_actors.c` | add prototypes |
| `ActorSizeCheck` negative array (arm64 only) | 220 TUs | goes away under ILP32 |

### SDK names with no PsyZ equivalent

See the SDK surface inventory: 22 of the 75 SDK functions called from game C.

---

## SDK surface inventory

Game C calls **75** functions in the SDK tail (`0x8005b8e0+`). The other ~364 of the 439 library
functions are only called from inside the SDK. 34 of the 75 carry project names instead of
PSY-Q names (e.g. `GetRandomU32` = `rand`, with 5,446 call sites; `CdControlBlocking` =
`CdControlB`).

Checked against what PsyZ actually compiles natively: `psyz/src/psyz`, `psyz/src/platform`, and
the `decomp/src` files listed in `psyz/CMakeLists.txt`. Its PS1-only decomp tree was not counted.
PsyZ was at `76ddc61` (2026-11-07).

| Status | Count | Functions |
|---|--:|---|
| In the PsyZ native build | 53 | libgpu, libspu, libcd core (`CdInit`, `CdRead`, `CdControlF/B`, `CdSync`…), libetc (`VSyncCallback`, `PadInit`/`PadRead`), libapi events/callbacks + `_96_remove`, libcard `_card_load/_read/_write`, libgte helpers, `rand` (PS1-compatible, which demo playback needs: `srand(1234)`) |
| Host libc | 5 | `memcpy`, `strlen`, `sprintf` (157), `printf`, `exit` |
| libmcrd upper layer (in PsyZ's PS1 decomp, not its native build) | 7 | `MemCardSync` (16), `MemCardStart`, `MemCardStop`, `MemCardCreateFile`, `MemCardFormat`, the reset (`InitCARD2`/`StartCARD2`/`_bu_init`) and the info op. Shim them over PsyZ's native `_card_*` layer, or a host file |
| libpad (missing) | 4 | `PadInitDirect`, `PadSetMainMode`, `PadInfoMode`, `PadStopCom` → shim over libetc pad |
| Boot / kernel | 3 | `LoadExec`, `_96_init`, libapi vtable slot 0x10 → stubs |
| Misc | 3 | `MulMatrix0` (PS1 decomp only; trivial C), `CdReadCallback`, `CdLastInt` (`GetCdStatusByte`) → small shims |

Full table in the appendix.

---

## Hand-written asm inventory

100 functions, 97,104 bytes, all in the main EXE. 61 use the GTE.

| Subsystem | Funcs | Bytes | GTE | S / M / L |
|---|--:|--:|--:|---|
| math / vector / GTE lib | 42 | 5,420 | 23 | 36 / 6 / 0 |
| OT / packet helpers | 2 | 344 | 0 | 1 / 1 / 0 |
| actor / emit-list runtime | 13 | 5,032 | 5 | 4 / 9 / 0 |
| render loops | 34 | 70,584 | 25 | 4 / 19 / 11 |
| collision loops | 9 | 15,724 | 8 | 1 / 4 / 4 |
| **Total** | **100** | **97,104** | **61** | **46 / 39 / 15** |

Size classes: S ≤ 200 B, M ≤ 1,500 B, L > 1,500 B. The biggest are `RenderWorldChunks` (19,976 B),
`EmitSecondaryActorPrimitives` (6,836 B), `RasterizeSpyroModel_Tail` (5,688 B) and
`EmitActorDrawList` (4,452 B). The seven `*_Tail` functions are jump-entered continuations of
their parents and have to be rewritten with them. Several hand-asm functions use `$gp` as a work
register and spill to `g_anStaticRegSaveArea`. That matters only to a literal translation.

### Effort

PsyZ exposes register-level GTE endpoints (`Psyz_GteDataWrite`, `Psyz_GteCtrlWrite`,
`Psyz_GteCommand`, …) meant for exactly this use. Under identity-mapped ILP32, a literal
asm → C translation is therefore *exact*: same addresses, same 32-bit arithmetic, GTE through
PsyZ. Readable rewrites can follow later, one function at a time, checked against the literal
version.

| Approach | Estimate |
|---|---|
| Hand rewrite of all 100 to readable C | ~130 person-days (S 1–2 h, M ~1.25 d, L ~5 d) |
| **Literal translator** (R3000 subset + GTE, in `tools/open-spyro`) + review and fixtures | ~10 d for the tool; S nearly automatic (~5 d total); M ~0.5 d each (~20 d); L ~2 d each (~30 d) → **~65 person-days** |

Priority order: the math/vector/GTE lib (1,437 `ArcTan2` and 1,203
`AddVector` call sites), then OT helpers, actor runtime, render, collision. Per-function list in
the appendix.

---

## Strategy

### 64-bit strategy: identity-mapped ILP32 (wasm32 → wasm2c → native arm64)

- The game C, PsyZ core and the translated hand-asm build for `wasm32`, with linear memory
  covering `0x80000000 + 8 MiB` (and the `0x1F800000` scratchpad).
- Every game global is placed at its PS1 address through a generated header. `.rodata`, `.data`
  and the overlay images are loaded from the user's disc. Code pointers in that data are patched
  through a generated address → function table.
- The KUSEG mirror (`0x00000000–0x001FFFFF` aliasing KSEG0) is provided by remapping the same
  pages into wasm2c's reserved linear memory (`mach_vm_remap`). The fallback is a native-only fix
  at the two `& 0x7FFFFFFF` sites.
- `wasm2c` turns the module into C, which Apple clang compiles for arm64. PsyZ's ~53-function
  platform API (audio / cd / gpu / input / video / system) runs on the host: SDL3 presents
  PsyZ's software-rasterized framebuffer and handles audio and input. Calls between the wasm
  side and the host go through wasm imports.
- All 37 overlays are compiled in; no function names collide across overlays. Loading an
  overlay means copying its data image to `0x8007aa38` and pointing the hook table at its
  functions.

### What this changes for the port

- **A toolchain gate comes first.** wasi-sdk or Homebrew LLVM (wasm backend) + wabt `wasm2c`.
  Build `level.c` + `actor` with a stub `main`, place memory at `0x80000000`, run
  `UnpackWorldDataChunks` on a real level blob, compare results with DuckStation RAM. If this
  fails, fall back to LP64 with arena handles (below).
- The native target is the wasm32 → wasm2c → arm64 pipeline, not a direct arm64 build of the
  game C.
- Most shared-code portability work is **unnecessary**. `REG_PIN` and native `match_asm.h` are
  still useful (a `port.h` that blanks `asm("$n")` is what the spike used). `FIELD_RAW`,
  `PRIM_HDR`, `uptr` and `SCRATCHPAD` are not needed under ILP32.
- Overlapping globals are handled by the generated address-placed globals header (fixes all 121
  alias groups, 46 nested symbols and 128 block views at once).
- The remaining compile errors are 11 files / 23 errors (table in the clang pass above).
- Hand-written asm: literal translator first; readable C later.
- Memory model = identity map; no level-data translation layer.

### Fallback if the ILP32 gate fails

LP64 with a 2 MiB arena, 32-bit PS1-address handles, and globals placed in the arena by the same
generated header. Cost: ~1,571 cast sites, plus a handle type at every stored-pointer use (~6,600
pointer-global uses plus the pointer members of 111 structs), plus the packet structs. That is
too large for neutral macro sweeps. It would effectively be a native-only fork of the game C,
so treat it as **NO-GO** unless the gate result forces it.

---

## Method notes

- Level data: `WAD.WAD` from `disc/orig`, actor sections found with `tools/_levelscan/lvl.py`.
  The reloc list is the section after the actor pool, aux pool, pickup table and spatial grid
  (`BuildLoadedLevelRuntime`).
- Counts of symbols and aliases come from `config/spyro.main.syms.ld` and the `include/*.h`
  extern declarations. "Use sites" are word-boundary matches in comment-stripped `src/**/*.c`.
- Hand-asm sizes are from `progress.json` (`handwritten: true`); GTE counts are from
  `asm/text.s` (`mtc2/mfc2/ctc2/cfc2/lwc2/swc2` and COP2 commands).

## Appendix A — SDK functions called from game C

| Game name | PSY-Q / PsyZ name | Call sites | PsyZ native build |
|---|---|--:|---|
| `InitSpu` | `SpuInit` | 1 | yes — decomp/src/libspu/s_i.c |
| `ApplySpuVoiceAttr` | `SpuSetVoiceAttr` | 3 | yes — decomp/src/libspu/s_sva.c |
| `SetSpuKeyMask` | `SpuSetKey` | 3 | yes — decomp/src/libspu/s_sk.c |
| `GetSpuVoiceKeyStatus` | `SpuGetKeyStatus` | 1 | yes — decomp/src/libspu/s_gks.c |
| `ReadSpuRam` | `SpuRead` | 1 | yes — decomp/src/libspu/s_r.c |
| `WriteSpuRam` | `SpuWrite` | 7 | yes — decomp/src/libspu/s_w.c |
| `SetSpuTransferStartAddr` | `SpuSetTransferStartAddr` | 8 | yes — decomp/src/libspu/s_stsa.c |
| `SetSpuTransferMode` | `SpuSetTransferMode` | 1 | yes — decomp/src/libspu/s_stm.c |
| `GetSpuTransferStatus` | `SpuIsTransferCompleted` | 8 | yes — decomp/src/libspu/s_itc.c |
| `SetSpuCommonAttr` | `SpuSetCommonAttr` | 5 | yes — decomp/src/libspu/s_sca.c |
| `GetAllSpuVoiceKeyStatus` | `SpuGetAllKeysStatus` | 1 | yes — decomp/src/libspu/sr_gaks.c |
| `SetSpuVoiceVolumeLR` | `SpuSetVoiceVolume` | 1 | yes — decomp/src/libspu/s_svv.c |
| `SpuGetVoiceVolume` | = | 1 | yes — decomp/src/libspu/s_gvv.c |
| `LoadExec` | = | 1 | **missing** |
| `_96_init` | = | 1 | **missing** |
| `Bios96Remove` | `_96_remove` | 1 | yes — psyz/src/psyz/libapi.c |
| `VSync` | = | 116 | yes — psyz/src/psyz/libapi.c |
| `InstallVSyncCallback` | `VSyncCallback` | 2 | yes — psyz/src/psyz/kernel.c |
| `CallLibapiSysSlot10` | `(libapi vtable slot 0x10)` | 1 | **missing** |
| `InitDefaultDrawEnv` | `SetDefDrawEnv` | 2 | yes — decomp/src/libgpu/ext.c |
| `InitDefaultDispEnv` | `SetDefDispEnv` | 2 | yes — decomp/src/libgpu/ext.c |
| `ResetGraph` | = | 2 | yes — decomp/src/libgpu/sys.c |
| `SetGraphDebug` | = | 1 | yes — decomp/src/libgpu/sys.c |
| `SetDispMask` | = | 7 | yes — decomp/src/libgpu/sys.c |
| `DrawSync` | = | 79 | yes — decomp/src/libgpu/sys.c |
| `ClearImage` | = | 12 | yes — decomp/src/libgpu/sys.c |
| `LoadImage` | = | 31 | yes — decomp/src/libgpu/sys.c |
| `StoreImage` | = | 11 | yes — decomp/src/libgpu/sys.c |
| `MoveImage` | = | 4 | yes — decomp/src/libgpu/sys.c |
| `DrawOTag` | = | 27 | yes — decomp/src/libgpu/sys.c |
| `PutDrawEnv` | = | 28 | yes — decomp/src/libgpu/sys.c |
| `PutDispEnv` | = | 38 | yes — decomp/src/libgpu/sys.c |
| `SetDrawMode` | = | 6 | yes — decomp/src/libgpu/sys.c |
| `InitGeom` | = | 1 | yes — psyz/src/psyz/libgte.c |
| `MulMatrix0` | = | 3 | **missing** |
| `MulMatrix` | = | 6 | yes — psyz/src/psyz/libgte.c |
| `SetTransMatrix` | = | 3 | yes — psyz/src/psyz/libgte.c |
| `SetGeomOffset` | = | 1 | yes — psyz/src/psyz/libgte.c |
| `SetGeomScreen` | = | 1 | yes — psyz/src/psyz/libgte.c |
| `memcpy` | = | 30 | **missing** |
| `GetRandomU32` | `rand` | 5446 | yes — decomp/src/libc2/rand.c |
| `srand` | = | 2 | yes — decomp/src/libc2/rand.c |
| `strlen` | = | 60 | **missing** |
| `WritePrintf` | `printf` | 2 | **missing** |
| `sprintf` | = | 157 | **missing** |
| `exit` | = | 2 | **missing** |
| `CdInit` | = | 3 | yes — psyz/src/psyz/libcd.c |
| `GetCdStatusByte` | `CdLastInt` | 1 | **missing** |
| `CallCdSync` | `CdSync` | 10 | yes — decomp/src/libcd/sys.c |
| `CdControl` | = | 9 | yes — decomp/src/libcd/sys.c |
| `CdControlForce` | `CdControlF` | 1 | yes — decomp/src/libcd/sys.c |
| `CdControlBlocking` | `CdControlB` | 7 | yes — decomp/src/libcd/sys.c |
| `CdMix` | = | 2 | yes — decomp/src/libcd/sys.c |
| `CdIntToPos` | = | 6 | yes — decomp/src/libcd/sys.c |
| `CdPosToInt` | = | 1 | yes — decomp/src/libcd/sys.c |
| `CdRead` | = | 4 | yes — psyz/src/psyz/libcd.c |
| `CdReadCallback` | = | 3 | **missing** |
| `MemCardReset` | `(libmcrd internal: InitCARD2/StartCARD2/_bu_init)` | 1 | **missing** |
| `MemCardOpenSession` | `MemCardStart` | 2 | **missing** |
| `MemCardCloseSession` | `MemCardStop` | 3 | **missing** |
| `RunMemCardInfoOp` | `(libmcrd internal: _card_info op)` | 1 | **missing** |
| `MemCardLoad` | `_card_load` | 3 | yes — psyz/src/psyz/libcard.c |
| `MemCardRead` | `_card_read` | 4 | yes — psyz/src/psyz/libcard.c |
| `MemCardWrite` | `_card_write` | 2 | yes — psyz/src/psyz/libcard.c |
| `MemCardSync` | = | 16 | **missing** |
| `MemCardCreateFile` | = | 1 | **missing** |
| `MemCardFormat` | = | 1 | **missing** |
| `PadStartCom` | = | 1 | yes — psyz/src/psyz/libetc.c |
| `PadStopCom` | = | 1 | **missing** |
| `PadGetState` | = | 1 | yes — psyz/src/psyz/libetc.c |
| `PadInfoMode` | = | 1 | **missing** |
| `PadSetActAlign` | = | 1 | yes — psyz/src/psyz/libetc.c |
| `PadSetMainMode` | = | 1 | **missing** |
| `PadSetAct` | = | 1 | yes — psyz/src/psyz/libetc.c |
| `PadInitDirect` | = | 1 | **missing** |

## Appendix B — hand-written asm functions

GTE column: COP2 commands / register moves. C call sites: word-boundary call matches in `src/`. `*_Tail` functions are entered by jumps from their parent.

#### math / vector / GTE lib

| Function | Addr | Bytes | GTE ops / moves | C call sites | Size |
|---|---|--:|--:|--:|:-:|
| `FillWord` | `0x80016914` | 28 | 0 / 0 | 73 | S |
| `FillBlock16` | `0x80016930` | 40 | 0 / 0 | 2 | S |
| `CopyWords` | `0x80016958` | 84 | 0 / 0 | 65 | S |
| `ArcTan2_8bit` | `0x800169ac` | 264 | 0 / 0 | 299 | M |
| `ArcTan2` | `0x80016ab4` | 420 | 0 / 2 | 1437 | M |
| `LookupSine` | `0x80016c58` | 88 | 0 / 0 | 676 | S |
| `LookupCosine` | `0x80016cb0` | 88 | 0 / 0 | 788 | S |
| `CountLeadingZeros` | `0x80016d08` | 36 | 0 / 2 | 2 | S |
| `ApplyEulerRotation` | `0x80016d2c` | 676 | 6 / 53 | 49 | M |
| `RotateVectorByMatrix` | `0x80017048` | 120 | 1 / 11 | 507 | S |
| `ApplyActiveGteRotation` | `0x800170c0` | 80 | 1 / 6 | 44 | S |
| `WorldToCameraRotate` | `0x80017110` | 120 | 1 / 11 | 38 | S |
| `FitVector3ToInt16` | `0x80017188` | 116 | 0 / 2 | 0 | S |
| `VectorLength` | `0x800171fc` | 152 | 1 / 9 | 635 | S |
| `RefineSqrtEstimate` | `0x8001729c` | 148 | 1 / 9 | 3 | S |
| `SetVector3Magnitude` | `0x80017330` | 240 | 1 / 8 | 120 | M |
| `ReflectVectorOffNormal` | `0x80017428` | 288 | 3 / 22 | 37 | M |
| `CrossProductOfEdges` | `0x80017548` | 112 | 1 / 9 | 0 | S |
| `ScaleVector3ByRatio` | `0x800175b8` | 92 | 1 / 10 | 396 | S |
| `ScaleVector3Shifted` | `0x80017614` | 140 | 1 / 10 | 1 | S |
| `VectorsEqual` | `0x8001771c` | 60 | 0 / 0 | 0 | S |
| `AddVector` | `0x80017758` | 52 | 0 / 0 | 1203 | S |
| `SubtractVector` | `0x8001778c` | 52 | 0 / 0 | 759 | S |
| `ScaleVector3Sat` | `0x800177c0` | 56 | 1 / 10 | 216 | S |
| `DivideVector3` | `0x800177f8` | 92 | 1 / 10 | 18 | S |
| `CrossProduct` | `0x80017854` | 64 | 1 / 9 | 0 | S |
| `LerpVectorGTE` | `0x80017894` | 116 | 2 / 11 | 1 | S |
| `AbsAngleDelta8` | `0x80017908` | 32 | 0 / 0 | 289 | S |
| `AbsAngleDelta12` | `0x80017928` | 32 | 0 / 0 | 135 | S |
| `SignedAngleDelta8` | `0x80017948` | 36 | 0 / 0 | 29 | S |
| `SignedAngleDelta12` | `0x8001796c` | 36 | 0 / 0 | 61 | S |
| `ApproxDist2D` | `0x80017990` | 96 | 0 / 0 | 1511 | S |
| `ApproachAngle8` | `0x800179f0` | 72 | 0 / 0 | 7 | S |
| `IntegerSqrt` | `0x80017a38` | 100 | 0 / 2 | 122 | S |
| `ProjectWorldPointGTEUnscaled` | `0x80017aa4` | 164 | 1 / 12 | 9 | S |
| `ProjectWorldPointGTE` | `0x80017b48` | 180 | 1 / 12 | 2 | S |
| `AddSVector` | `0x80017c84` | 52 | 0 / 0 | 284 | S |
| `UnpackWorldCollisionTri` | `0x80017cb8` | 196 | 0 / 0 | 45 | S |
| `ComputeStepTowardPoint` | `0x80017d7c` | 216 | 1 / 9 | 20 | M |
| `ComputeDepthCuedColor` | `0x80017e54` | 68 | 1 / 6 | 7 | S |
| `ConvertRGB15ToLuma8` | `0x80017e98` | 140 | 0 / 0 | 6 | S |
| `AddBiasToColorRuns` | `0x80017f24` | 176 | 0 / 0 | 4 | S |

#### OT / packet helpers

| Function | Addr | Bytes | GTE ops / moves | C call sites | Size |
|---|---|--:|--:|--:|:-:|
| `LinkOTPrimitives` | `0x80016784` | 284 | 0 / 0 | 27 | M |
| `AddPrimToOTSlot` | `0x800168a0` | 60 | 0 / 0 | 4 | S |

#### actor / emit-list runtime

| Function | Addr | Bytes | GTE ops / moves | C call sites | Size |
|---|---|--:|--:|--:|:-:|
| `BuildActiveActorList` | `0x80051fec` | 468 | 1 / 5 | 35 | M |
| `BuildRenderEntityLists` | `0x800521c0` | 256 | 0 / 0 | 33 | M |
| `TickActorAnims` | `0x800522c0` | 516 | 0 / 0 | 81 | M |
| `AllocActorRecordSlot` | `0x800524c4` | 164 | 0 / 0 | 37 | S |
| `DespawnActorRecord` | `0x80052568` | 320 | 0 / 0 | 1528 | M |
| `InsertActorIntoSpatialGrid` | `0x800526a8` | 804 | 6 / 48 | 338 | M |
| `HideActorRenderRecord` | `0x800529cc` | 24 | 0 / 0 | 212 | S |
| `RefreshActorDerivedState` | `0x800529e4` | 896 | 6 / 48 | 624 | M |
| `GetActorAttachPoint` | `0x80052d64` | 468 | 3 / 24 | 93 | M |
| `AccumulateAnimRootMotionDelta` | `0x80052f38` | 392 | 0 / 0 | 2 | M |
| `EncodeHitNormalToActorGroundCode` | `0x800533d0` | 416 | 1 / 6 | 287 | M |
| `AllocEmitListRecord` | `0x80053570` | 152 | 0 / 0 | 697 | S |
| `FreeEmitListRecord` | `0x80053608` | 156 | 0 / 0 | 732 | S |

#### render loops

| Function | Addr | Bytes | GTE ops / moves | C call sites | Size |
|---|---|--:|--:|--:|:-:|
| `BuildActorDrawList` | `0x8001f158` | 1600 | 7 / 51 | 8 | L |
| `EmitActorDrawList` | `0x8001f798` | 4452 | 15 / 110 | 8 | L |
| `BuildSecondaryActorDrawList` | `0x800208fc` | 1592 | 7 / 51 | 2 | L |
| `EmitSecondaryActorPrimitives` | `0x80020f34` | 6836 | 22 / 196 | 2 | L |
| `RasterizeSpritePrimQueue` | `0x80022a2c` | 4248 | 22 / 143 | 30 | L |
| `RasterizeSpyroModel` | `0x80023ac4` | 2036 | 7 / 80 | 10 | L |
| `RasterizeSpyroModel_Tail` | `0x800242b8` | 5688 | 28 / 221 | 0 | L |
| `RenderWorldChunks` | `0x800258f0` | 19976 | 38 / 238 | 5 | L |
| `TickWorldChunkAnimations` | `0x8002a6fc` | 3220 | 6 / 33 | 1 | L |
| `ModifyWorldChunkAnimFlags` | `0x8002b390` | 100 | 0 / 0 | 99 | S |
| `GetWorldChunkAnimState` | `0x8002b3f4` | 80 | 0 / 0 | 37 | S |
| `SetWorldChunkAnimStep` | `0x8002b444` | 104 | 0 / 0 | 7 | S |
| `ResetWorldChunkAnimations` | `0x8002b4ac` | 1312 | 0 / 0 | 1 | M |
| `EmitSkyMesh` | `0x8004eba8` | 1112 | 2 / 22 | 7 | M |
| `EmitSkyMeshFogged` | `0x8004f000` | 1212 | 3 / 29 | 2 | M |
| `EmitPortalSky` | `0x8004f4bc` | 812 | 2 / 22 | 1 | M |
| `ClipAndEmitPortalSkyTriangle` | `0x8004f7e8` | 260 | 3 / 8 | 0 | M |
| `ClipAndEmitPortalSkyTriangle_Tail` | `0x8004f8ec` | 1216 | 0 / 0 | 0 | M |
| `BlendDepthCuedEdgeColor` | `0x8004fdac` | 244 | 2 / 12 | 0 | M |
| `EmitPortalColorFill` | `0x8004fea0` | 240 | 0 / 0 | 3 | M |
| `ClipAndEmitPortalFillTriangle` | `0x8004ff90` | 212 | 3 / 8 | 0 | M |
| `ClipAndEmitPortalFillTriangle_Tail` | `0x80050068` | 472 | 0 / 0 | 0 | M |
| `EmitPortalSkyFogShaded` | `0x80050240` | 972 | 3 / 29 | 1 | M |
| `ClipAndEmitPortalSkyFogShadedTriangle` | `0x8005060c` | 260 | 3 / 8 | 0 | M |
| `ClipAndEmitPortalSkyFogShadedTriangle_Tail` | `0x80050710` | 1216 | 0 / 0 | 0 | M |
| `BurstActorIntoSparks` | `0x800530c0` | 784 | 1 / 19 | 1 | M |
| `RasterizeEmitList` | `0x800573c8` | 3372 | 17 / 149 | 25 | L |
| `DrawWorldSparkleParticles` | `0x800580f4` | 976 | 3 / 21 | 1 | M |
| `DrawWorldRotatingBillboards` | `0x800584c4` | 872 | 4 / 60 | 1 | M |
| `FindFreeRotBillboardSlot` | `0x8005882c` | 56 | 0 / 0 | 1 | S |
| `DrawDragonRescueAuraStar` | `0x80058864` | 644 | 4 / 28 | 1 | M |
| `DrawSpyroFlameTrails` | `0x80058d64` | 1808 | 8 / 59 | 1 | L |
| `DrawSpyroDropShadow` | `0x80059a48` | 1348 | 6 / 48 | 6 | M |
| `DrawActorDropShadows` | `0x80059f8c` | 1252 | 8 / 67 | 4 | M |

#### collision loops

| Function | Addr | Bytes | GTE ops / moves | C call sites | Size |
|---|---|--:|--:|--:|:-:|
| `CastRayWorldAndActors` | `0x8004ae38` | 4020 | 18 / 163 | 33 | L |
| `CastRayWorldAndActors_Tail` | `0x8004bdec` | 96 | 0 / 0 | 0 | S |
| `CollideSphereWithWorldAndActors` | `0x8004be4c` | 4240 | 19 / 161 | 223 | L |
| `CollideSphereWithWorldAndActors_Tail` | `0x8004cedc` | 1808 | 9 / 78 | 0 | L |
| `FindGroundHeightBelow` | `0x8004d5ec` | 2360 | 10 / 66 | 390 | L |
| `FindWorldFloorBelowPoint` | `0x8004df24` | 964 | 4 / 24 | 3 | M |
| `RaiseSpyroTriggerInRadius` | `0x8004e2e8` | 224 | 1 / 6 | 47 | M |
| `DispatchActorContactAtSphere` | `0x8004e3c8` | 576 | 1 / 6 | 70 | M |
| `DispatchActorContactAtSphere_Tail` | `0x8004e60c` | 1436 | 4 / 36 | 0 | M |
