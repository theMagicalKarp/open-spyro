# Native build (macOS arm64, Linux arm64, Linux x86_64)

An experimental native executable built from the matched C. It runs no
emulation. The feasibility study is in `docs/native-spike.md`. The PS1 build
never reads anything described here, and `make check` stays the gate for every change to shared code.

**Status: in progress.** `make native` builds `build/native/spyro`:
all game C, the translated hand-written asm and PsyZ in one module. Given the
user's disc image it loads the game's executable from it and runs the game's
`main` at the original frame rate: boot splashes, the title menu, the opening
cutscene, and the game itself. All 35 levels load and play (hubs, flight
levels, bosses), with sound (effects, XA music, cutscene narration) and
memory-card saves to a card-image file.
`spyro --stub` runs a disc-free test scene instead, with no disc (the CI smoke
run).

## Setup and use

Supported hosts: macOS on Apple Silicon (Homebrew), and Linux on arm64 or
x86_64. On Linux, `tools/native_setup.sh` uses the system SDL3 when
`pkg-config` finds one and otherwise builds the pinned SDL3 release into
`build/native-tools/sdl3`; it names the Debian/Ubuntu packages it needs
(`LINUX_PACKAGES`) if a tool is missing. Rendering uses SDL_GPU on Vulkan, so
Linux needs a Vulkan driver (Mesa's `mesa-vulkan-drivers` includes the
`lavapipe` software one). With no display, set `SDL_VIDEODRIVER=offscreen`
(the CI smoke runs do).

```sh
tools/native_setup.sh            # wasi-sdk + wasm2c (pinned, into build/native-tools) + SDL3/cmake (Homebrew on macOS)
git submodule update --init external/psyz   # `make native` does this too; PsyZ's own submodules are not needed
make native                      # -> build/native/spyro (+ build/native/gate)
make native-run NATIVE_ARGS="--frames 300 --screenshot out.bmp"   # NATIVE_DISC=path.cue
make native-run NATIVE_ARGS="--frames 1900 --screenshot out.bmp --press 1300:start \
  --press 1400:start --shot 1550"   # scripted pad 1 + a screenshot at VSync 1550
make native-run NATIVE_ARGS="--fast --memcard none --frames 4400 --screenshot out.bmp \
  --press 1300:start --press 1400:start --press 1550:cross --press 1650:cross \
  --press 2300:start --warp 3400:21 --shot 3990"   # Dry Canyon, unthrottled
build/native/spyro --stub --frames 60    # no disc: a spinning-triangle test scene
make native-gate SAVESTATE=~/Library/Application\ Support/DuckStation/savestates/<state>.sav
open-spyro neutral [--ref main] [--host]   # shared-code change neutral for the PS1 cpp?
```

`make clean` empties `build/`, including `build/native-tools`; re-run
`tools/native_setup.sh` after it.

## How it works

The game C needs 32-bit pointers with PS1 RAM at its real addresses
(`docs/native-spike.md`). A 64-bit host process cannot map memory at `0x80000000`, so the
game is compiled for **wasm32**, where an address is an offset into linear
memory, and **wasm2c** turns the module back into C for the host compiler
(Apple clang on macOS, clang or gcc on Linux):

```
game C + src/native/handasm
  + PsyZ core + src/native/wasm        --wasi-sdk clang-->  spyro.wasm (wasm32, PIE)
spyro.wasm                             --wasm2c-->          spyro_w.c
spyro_w.c + src/native/host + PsyZ SDL3 platform layer  --host cc-->     build/native/spyro
```

The result is plain host code (arm64 or x86_64). wasm2c bounds-checks memory with guard pages,
so there are no per-access checks.

### Memory map (`src/native/host/psx_memory.h`)

| Linear address | Contents |
|---|---|
| `0x00000000–0x001FFFFF` | KUSEG mirror of main RAM: the same pages as KSEG0, remapped with `mach_vm_remap` (macOS) or mapped from one `memfd` (Linux) |
| `0x01000000–0x017FFFFF` | wasm-side static data (`__memory_base`) |
| `0x01800000–0x01FFFFFF` | wasm-side C stack |
| `0x1F800000` | scratchpad |
| `0x80000000–0x807FFFFF` | main RAM (2 MiB retail, plus dev-kit headroom for `DEMO_RECORD_BUFFER`) |

Only touched pages are committed.

### Game globals at PS1 addresses: GOT imports

The modules are position-independent (`-fPIC`, `wasm-ld -pie`
`--unresolved-symbols=import-dynamic`). Every game global stays an undefined
`extern` (the `globals.h` declarations), so wasm-ld turns each one into a
`GOT.mem` import. `open-spyro native got-table` writes the host side of those
imports from the same symbol files the PS1 link pins (`config/symbol_addrs.txt`
and the `undefined_*` lists). Each global therefore lives at its PS1 address,
and aliases, nested symbols, `*Block` views and cross-symbol arithmetic
(`textureCountRef - 7`) work unchanged. **No source change is needed.** This
replaces the generated `globals_at.h` header the spike proposed.

### No libc from wasi-sdk

wasi-libc's `libc.a` is not PIC, so it cannot link into the PIE module. The
module carries its own small libc (`src/native/wasm/libc.c`): string functions,
the printf family, the `sscanf` subset PsyZ's cue parser uses, and stdio over
host file handles. The wasi sysroot is used for headers only. The module's
imports are therefore exactly:

- `env.Draw_*`, `Psyz_VideoVSync`, `Psyz_PadsPoll`, `MyPadInit`: PsyZ's
  platform API. `src/native/host/imports.c` forwards these to PsyZ's own
  `sdl3_gpu` backend (Metal on macOS, Vulkan on Linux), built natively.
- `host.*`: libc services (`src/native/wasm/host_imports.h`).
- `GOT.mem.*`: game globals (above).

PsyZ makes `u_long` pointer-sized: 32-bit in the module, 64-bit in the natively
built backend. `imports.c` widens GPU packet words before `Draw_PushPrim` parses
them, and copies `u_long` bitfield structs. Image data is 16-bit pixels on both
sides.

### Threads

A wasm2c instance must only run on one thread. `Psyz_OsThreadStart` fails on
purpose (`src/native/wasm/psyz_bridge.c`), so PsyZ's kernel services its timers
from VSync instead of an IRQ thread. For the same reason sound is pushed, not
pulled (next section).

### Sound

PsyZ's SPU emulation (voices, ADSR, reverb, and the CD / XA audio it decodes)
runs inside the module; its SDL backend would pull samples from an SDL audio
thread, which would re-enter the module. Instead `src/native/host/audio.c`
asks the module for samples (`native_audio_pull`) at every blocking VSync and
tops an SDL audio stream up to ~46 ms ahead of the device. The SPU so
advances at the device's rate, as under PsyZ's pull model.

The SPU therefore runs only between frames, where on a PS1 it runs alongside
the CPU. One place notices: `CdReadSyncSectors` polls the voice tick while it
waits for the disc, and a natively instant read polls twice with no samples
between. A voice keyed on just before then still reads "keyed, envelope 0",
the state the tick takes for a finished sound, and is freed (the opening
cutscene's narration was silent). `GetSpuVoiceKeyStatus` (`sdk_shim.c`)
reports a voice keyed on since the SPU last ran as `SPU_ON`.

PsyZ's first `Psyz_SpuInit` zeroes SPU RAM, and PsyZ calls it lazily, when
libcd first selects CD-DA / XA audio. By then the boot has uploaded the
title's sample bank, so the title menu's sounds played silence (zeroed
blocks with no end flag, so those voices never freed). `native_main` starts
the SPU emulation before the game runs.

### Memory cards

`src/native/wasm/memcard.c` implements the libmcrd calls the game makes
(Exist, Accept, ReadFile, WriteFile, CreateFile, Format) over a raw 128 KiB
card image, the `.mcd` format emulators use, so saves move between this build
and DuckStation. Port A is `card1.mcd` in the user's data directory
(`~/Library/Application Support/open-spyro/spyro/` on macOS,
`~/.local/share/open-spyro/spyro/` on Linux) unless `--memcard PATH`
names another; `--memcard none` leaves it empty, and `--memcard2` fills port
B. A missing file is a freshly formatted card, written on the first change;
every change writes the whole image back. Operations finish at once and
`MemCardSync` reports them on its next call.

## Shared-code portability

The game C compiles natively with `__psyz` defined. The few places where the
PS1 text cannot serve both builds go through `include/port.h` (included from
`types.h`, `actor.h`, `model.h` and the level headers that need it), whose PS1
expansion is the original text:

| Macro | PS1 | native |
|---|---|---|
| `REG_PIN(type, name, "$n")` | `register type name asm("$n")` | `type name` |
| `PORT_VOLATILE` | (empty) | `volatile`: globals declared plain in some headers and volatile in others |

`match_asm.h` has native definitions too (plain C: a fence is nothing,
`OPAQUE_COPY` is an assignment, `REG_RESERVE` zeroes). Other native-only code is
under `#ifdef __psyz` (the variadic `OnActorAnimQueued` prototype, prototypes
for the two `actor/groups.c` functions level 22 calls undeclared) or in
`src/native/game/`, which replaces PS1 files or definitions with no native
meaning: `render/ot.c` (`goto *ra`), `render/static_regs.c` (file-scope
register variables) and `OnActorAnimQueued`'s definition.
Native game C is built with `-Werror=implicit-function-declaration`: on wasm an
undeclared call gets a guessed signature and traps if the definition differs.

`open-spyro neutral` checks every shared-code change: it compares the PS1
preprocessor's output, before and after, as one stream of C tokens (line markers,
whitespace and line breaks ignored). `make check` is still the final word.

## The ILP32 gate

`native/gate/` builds the real `src/main/load/level.c` into its own module and
replays a level transition: `UnpackWorldDataChunks` on the world bundle,
`RelocateActorMeshHeader` on the mesh chunk (KUSEG pointers, 21-bit packed frame
words), then `BuildLoadedLevelRuntime` on the level archive. The input is that
level's original blobs from `WAD.WAD`. `open-spyro native gate` takes the RAM
from a DuckStation savestate taken in the level and checks three things:

- every word the native loader relocated equals the PS1 RAM;
- no on-disc offset that the PS1 rebased was missed;
- every published global (chunk tables, collision header, actor pool, …) is
  produced natively and matches.

Result on level index 10:

```
published globals: 32/32 match
world bundle   0x80087210+0x054014: 307 words relocated, 307 match PS1, 0 differ
mesh chunk     0x800db224+0x08d800: 1266 words relocated, 1266 match PS1, 0 differ
level archive  0x80168a24+0x00b000: 107 words relocated, 107 match PS1, 0 differ
gate: PASS
```

"Runtime-only changes" in the full output are words the game changed after
loading (actor state, animated vertices); the gate does not compare them.

## Starting the game

`spyro --disc game.cue` copies the path into the module, and
`src/native/wasm/game_main.c` does what the BIOS does: opens the image through
PsyZ's CD layer, finds `SCUS_942.28` (`CdSearchFile`), and reads its PS-X EXE
image to the address in its header (`0x80010000`). That puts main `.rodata` and
`.data` at their PS1 addresses, where the game C reaches them through the GOT;
the image's code bytes are inert. Nothing in it needs patching: its 26
code-pointer words all belong to the SDK the module replaces. Then it calls the
game's `main`.

Overlays need nothing extra. The game reads each from `WAD.WAD` into
`0x8007aa38` itself, which lands the overlay's data at its PS1 addresses, and
`ApplyPerLevelGlobalsTable` installs native function pointers (every overlay's
C is in the module; no name collides). Overlay data has no code pointers. The
GOT table includes the overlays' symbol files and fails if a name has two
addresses. Level ids 0–9 and 99 point at overlays that are not on the disc;
their entry points abort (`src/native/game/load/level_id_stubs.c`).

## Input and headless runs

Pad 1 is the keyboard or the first SDL gamepad (PsyZ's backend): arrows =
D-pad, X / Z / S / D = Cross / Square / Triangle / Circle, Q / R / W / E =
L1 / R1 / L2 / R2, Return = Start, Backspace = Select. PsyZ fills the
direct-mode report buffers on every VBlank; the game's own VSync callback
(`PollPadAndDistributeInput`) reads them.

For runs without a person at the keyboard, `--press N:BUTTONS[:HOLD]` holds
buttons (names joined by `+`: `cross+up`) for HOLD VSyncs (default 4) from
VSync N, and `--shot N` writes a VRAM screenshot at VSync N next to
`--screenshot` (`out-N.bmp`). A VSync here is a blocking one: the game runs
one frame per two. The title flyby lasts until VSync ~1300 on a cold boot;
Start skips it, a second Start opens the memory-card check. A card changes
the menus that follow, so scripted runs pass `--memcard none` (or a scratch
card) to stay repeatable.

`--fast` drops the 59.94 Hz frame limiter, and `VSync(-1)`, the counter the
game paces itself by, then counts blocking VSyncs instead of wall time: game
time advances one vblank per VSync, so a run is deterministic and a few
seconds long. `--warp N:LEVEL` starts a load of level id LEVEL (10 =
Artisans, 21 = Dry Canyon, ...) at VSync N, or once gameplay resumes after
it, with the teardown the dev level select runs
(`src/native/game/debug/warp.c`). `--wav out.wav` writes the sound output to
a file as well.

## The SDK

The matched C calls the SDK by project names. `src/native/wasm/sdk_shim.c`
defines each one with the game prototype's signature and forwards to PsyZ:
libspu, libgpu, libcd, libapi, `rand`. Gaps are shimmed:

- libpad direct mode (`PadInitDirect`) is PsyZ's `InitPAD`, which fills the
  raw report buffers on VBlank.
- `SetDrawMode` writes the PS1 packet (tag, E1, E2). PsyZ's own writes its
  native two-word tag, which shifts the commands one word and overwrites the
  tag of the primitive the game packs right after the `DR_MODE`.
- `DrawOTag` converts the game's ordering table, built in the PS1 packet
  format (one tag word: 24-bit next address, length in the top byte), into
  PsyZ's native format (a two-word tag: full pointer, length) before handing
  it to PsyZ. On the way it clears the bits above 8 in each textured
  polygon's texpage halfword: the GPU ignores them, PsyZ uses them as its
  own flags (bit 15 = untextured), and the world renderer's texture records
  set bit 15, which drew all but the nearest terrain untextured.
- `CdRead` completes synchronously, then raises the read callback the game
  registered (every caller raises its in-flight flag first).
- The libmcrd upper layer is `memcard.c` (above).
- `GetSpuVoiceKeyStatus` treats a voice keyed on since the SPU last ran as
  `SPU_ON` (Sound, above).
- `LoadExec`, `_96_init` and the libapi slot 0x10 call log and do nothing
  (`LoadExec` exits).

`src/native/wasm/game_prelude.h` is force-included into game C. It renames
the game names that collide with PsyZ (the game's own `SpuInit` is a
memory-card reset) or that PsyZ declares with other types (`LoadExec`,
`MulMatrix0`, `CdRead`, `CdReadCallback`, `DrawOTag`, `SetDrawMode`).

## Hand-written asm

`open-spyro native handasm` translates the 100 hand-written functions from
`asm/text.s` into `src/native/handasm/ha_*.c` (generated, committed; rerun it
when the asm changes). It decodes each instruction word and emits C that does
exactly what the instruction does (`handasm.h`):

- Every register is a `uint32_t` local; memory accesses are plain pointer
  accesses at the PS1 address the code computes.
- Functions that jump into each other (`X` / `X_Tail`, the portal clip
  helpers) form one *cluster*, a single C function. Four small matched
  functions the asm falls or jumps into (`ReturnT0Fixed12`, ...) are
  translated into their clusters too.
- Clusters call each other with the whole register file (`HaRegs`): the
  "register-convention helpers" pass values in arbitrary registers.
- `jr reg` dispatches through a switch over the cluster's code addresses:
  branch targets, `jal` return addresses, entries, and every instruction of a
  Duff-style computed jump (`la base; add off; jr`). The collision and portal
  clip code also jumps into tables: of `j` instructions (the last entry may be
  `jr $ra`), or of fixed-stride stubs that each end in `j join`; the run then
  goes on through the table. An unknown target aborts with its address.
- Delay slots run before the transfer. R3000 load delays (the next
  instruction sees the old value) are honoured; the translator stops on any
  hazard it cannot express. This code has none.
- GTE moves and commands go to PsyZ's register-level API. `runtime.c` adds
  what PsyZ does not model: the SXYP FIFO push, IRGB/ORGB and LZCS/LZCR.
- Hand-asm frames live on a separate 256 KiB stack (`ha_mips_sp`); calls to
  game C (`GetRandomU32`, `PlaySoundEffect`, ...) use the callee's prototype.
- C callers reach each function through a wrapper with its `funcs.h`
  signature.

## Signatures must match

wasm traps on a call whose signature differs from the callee's, where the PS1
(and a native build) would not care. The link therefore runs with
`--fatal-warnings`, which makes wasm-ld's signature-mismatch warning an error.
The game had three kinds, all fixed without changing PS1 text:

- Overlay headers that declare main or SDK functions differently (`void`
  where the definition returns a value, a K&R `sprintf`, stray arguments the
  PS1 passes in a register the callee ignores): `#ifdef __psyz` declarations,
  with a call macro where the argument count differs.
- `MoveActorByDelta` takes its delta as a struct by value, which the PS1
  passes in `$a1`–`$a3` and callers pass as three ints:
  `src/native/game/actor/move_by_delta.c` is the callers' entry point.
- The titlescreen tests the `$v0` that the void
  `RestartTitleCardCheckOnTriangle` leaves: `src/native/game/gamestate/update.c`
  returns it.

PsyZ's own mismatches (decomp callers vs. PsyZ definitions: `WaitEvent`,
`StartCARD2`, `StopCARD2`, `CD_datasync`, `CD_getsector2`) go through adapters
in `src/native/wasm/psyz_fixups.c`, selected per calling file in
`native/wasm/CMakeLists.txt`.

The wasm link passes `--no-wasm-opt`. Otherwise clang runs any `wasm-opt` on
`PATH`, which makes the output depend on the machine and strips function names.

## Known issues

- The translated hand-asm has no differential tests yet.
- Physics parity with the PS1 has only been checked by eye, not side by side
  with DuckStation.
- The in-game save menu (talking to a save host) has not been driven by a
  scripted run; the title's card check, file creation and slot list have.
