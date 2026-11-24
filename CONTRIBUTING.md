# Contributing to open-spyro

Contributions are welcome! If you are interested, please
[open an issue](https://github.com/theMagicalKarp/open-spyro/issues/new) first
to talk through what you have in mind _(no worries if you don't)_, or browse the
[existing issues](https://github.com/theMagicalKarp/open-spyro/issues) if you
want to help but are unsure where to start. Good places to dig in:

- **New platforms:** Windows _(x86_64, arm64)_ and the web browser are not yet
  supported, and help getting them set up is very welcome.
- **Native parity:** finding and fixing places where the native build behaves
  differently from the PS1.
- **Readability:** naming, typing and documenting the decompiled C so it reads
  as the engine it is.

AI-assisted contributions are welcome too, with the understanding that they may
get additional scrutiny in review. Please make sure you understand and have
tested what you submit.

Build setup for both the native and PS1 builds is in the
[README](README.md#getting-started); native port internals are in
[`docs/native.md`](docs/native.md).

## Core invariant

Every commit builds a **byte-identical** `SCUS_942.28`, all 37 overlays and
`WAD.WAD`. Functions not expressed in C (the PSY-Q SDK libraries and
hand-written asm) link in as the **original assembly** _(`INCLUDE_ASM`)_.
"Progress" is the fraction of game code bytes expressed as verified-matching C.
Native-port work does not relax this: an edit that changes one PS1 output byte
is reverted. Where the PS1 text cannot serve both builds, the difference goes
through `include/port.h` or `#ifdef __psyz`, or into `src/native/game/`.

Target: `SCUS_942.28`, SHA-1 `84e3728ab94720d0873e2514adf4aade4935e0c5`, loads
at `0x80010000` _(file offset `0x800`)_. Main-EXE `.text` spans
`0x80012204`–`0x8006bb40`, 828 functions _(673 game code, 155 stock
PSY-Q/libc)_. 613 of these plus 982 globals were named in the old Ghidra project
_(reused here as a name/type dump only)_.

## Workflow

Every change must keep the PS1 build byte-identical:

1. **Edit the C.** Game code lives in `src/main/<subsystem>/` _(main EXE)_ and
   `src/overlays/<segment>/` _(overlays)_, shared by both builds. Native-only
   code goes in `src/native/`. When shared code has to differ between the two
   builds, guard it with `__psyz`: the native build defines it, the PS1 matching
   build never does, so `#ifdef __psyz` code is native-only and the `#else`
   branch is what gets byte-matched:

   ```c
   #ifdef __psyz
       /* native only: never seen by the PS1 compiler */
   #else
       /* PS1: must still match the original bytes */
   #endif
   ```

   Every PS1 build reruns `open-spyro gen-slots-ld`, which routes each
   function's compiled object into its fixed-VMA slot. **Never hand-edit
   `config/*.slots.ld`**.
2. **Check shared code stays neutral.** For a change to shared C or headers,
   `open-spyro neutral` confirms the PS1 preprocessor output is unchanged _(see
   [`docs/native.md`](docs/native.md))_.
3. **Build and run native.** `make native` must link _(signature mismatches are
   fatal on wasm)_; check the change in `make native-run`. For level-loader
   changes, `make native-gate SAVESTATE=...` compares the native load against
   PS1 RAM from a DuckStation savestate.
4. **Format & lint.** `make fmt` then `make lint` _(ruff + clang-format, same as
   CI)_.
5. **Prove the whole build still matches.** `make check` _(build + verify)_ must
   keep every artifact byte-identical. If it doesn't,
   [`open-spyro diff`](#comparing-a-function-against-the-original) shows the
   offending function.

## Layout

```
open-spyro/
├── src/                       # all C source
│   ├── main/<subsystem>/      #   matched C for the main EXE (SCUS_942.28), see table below
│   ├── overlays/<segment>/    #   matched C for the 37 overlays (title, credits, 35 levels)
│   └── native/                #   native-only code, never seen by the PS1 build
│       ├── host/              #     SDL3 host executable: window, memory map, audio, input
│       ├── wasm/              #     in-module SDK shim, libc, memory card
│       ├── game/              #     native replacements for game code that can't be shared
│       └── handasm/           #     hand-written PS1 asm routines translated to C
├── include/                   # shared headers: types.h, funcs.h, globals.h, system structs,
│                              #   port.h (PS1/native differences), include_asm.h, sdata.h
├── asm/                       # splat disassembly (`make split`); unmatched code is INCLUDE_ASM
│   ├── text.s/, header.s/     #   main EXE code + header
│   ├── data/                  #   main EXE .data / .rodata
│   └── overlays/<segment>/    #   per-overlay asm
├── config/                    # splat + linker config for the PS1 build
│   ├── compile.sh             #   the locked compile flags (gcc 2.7.2 + maspsx)
│   ├── spyro.main.yaml        #   splat config for the main EXE
│   ├── symbol_addrs.txt       #   function/global names -> addresses
│   ├── overlays.json          #   overlay table (generates the per-overlay splat configs)
│   ├── overlays/              #   per-overlay link scripts + symbols (lets CI link disc-free)
│   ├── wad.json, disc.xml     #   WAD.WAD and ISO repack layouts
│   └── sha1sums.txt           #   SHA-1 of every original file the build must reproduce
├── native/                    # CMake for the native build
│   ├── wasm/                  #   game C -> wasm32 module (wasm2c'd back to C)
│   ├── gate/                  #   `make native-gate`: native level load vs PS1 savestate
│   └── tests/                 #   native unit tests
├── external/                  # git submodules: psyz (PSY-Q SDK reimplementation), tomlc17
│                              #   (config parser), imgui + implot (diagnostics overlay)
├── tools/                     # build scripts (build_main.sh, extract.sh, make_iso.sh, …)
│   ├── Dockerfile             #   the matching toolchain image
│   └── open-spyro/            #   `open-spyro` Python CLI (uv): split, diff, neutral, progress
├── docs/                      # native port notes (native.md), progress map, images
├── .github/workflows/         # CI: disc-free PS1 build + verify, native build + smoke, lint
├── Makefile                   # entry point for every workflow (`make` lists targets)
├── spyro.example.toml         # native-run config template (copy to spyro.toml)
├── PROGRESS.md                # C-match progress report (`make progress`)
│
│  gitignored, local only:
├── iso/                       # your disc image goes here (.cue + .bin)
├── disc/orig/                 # files extracted from your disc (`make extract`)
└── build/                     # all build output: rebuilt SCUS_942.28 + overlays,
                               #   SpyrotheDragon.{bin,cue}, native/spyro, native-tools/
```

The main EXE's C is grouped by subsystem under `src/main/`, one file per role:

| `src/main/`  | Files                                                                                                 |
| ------------ | ----------------------------------------------------------------------------------------------------- |
| `boot/`      | `main.c` _(frame loop)_, `init.c` _(hardware/system init, new-game state)_                            |
| `load/`      | `level.c` _(level load + relocation, per-level tables)_, `cd_stream.c`, `anim_stream.c`               |
| `math/`      | `math.c` _(vector / fixed-point helpers; the rest is hand-written asm)_                               |
| `render/`    | `sprites.c` _(text, 2D overlays)_, `frame.c`, `portal_sky.c`, `ot.c`, `static_regs.c`                 |
| `gamestate/` | `draw.c` / `update.c` _(per-state handlers + dispatchers)_, `transitions.c`, `cheats.c`               |
| `camera/`    | `camera.c`                                                                                            |
| `actor/`     | `helpers.c` _(random, angles)_, `queries.c`, `movement.c`, `move_by_delta.c`, `effects.c`, `groups.c` |
| `spyro/`     | `anim.c`, `movement.c`, `collision.c`, `state.c`, `attack.c`, `triggers.c`                            |
| `pad/`       | `pad.c`                                                                                               |
| `hud/`       | `hud.c`                                                                                               |
| `sound/`     | `sound.c`, `play_sfx.c`, `voice_pan.c`                                                                |
| `particles/` | `particles.c`                                                                                         |
| `save/`      | `save.c`                                                                                              |

Overlays are still one function per file, under `src/overlays/<segment>/`.

Directories and files are grouping only. A file may define any number of
functions: `config/compile.sh` gives each one its own `.text.<fn>` /
`.rodata.<fn>` section, and every function still links into its own
fixed-address slot, so moving functions between files never changes the bytes as
long as each function still sees the same declarations. Function names must be
unique per segment (the build enforces it); objects mirror the source tree under
`build/<segment>/c/`. A function needs its own file (or an alias symbol, see
`config/symbol_addrs.txt` `_blk` entries) when it:

- declares a global or prototype with a different type than another function in
  the file sees (an array vs scalar view changes the addressing);
- declares a file-scope `register` variable (`ot.c`, `static_regs.c`);
- is `SDATA()`-declared small data that another function addresses absolutely.

## Matching toolchain

- **Compiler:** gcc 2.7.2 _(the pinned decompals/old-gcc prebuilt `cc1`, in the
  Docker image at `/opt/gcc2.7.2/cc1`)_
- **Flags:**
  `-O2 -G8 -g3 -fverbose-asm -mips1 -mcpu=3000 -fgnu-linker -mno-abicalls -mgpopt -msoft-float`
  _(overlays override to `-G0`)_
- **Assembler:** `maspsx --aspsx-version 2.56 -G4 --expand-div` → GNU `as`/`ld`
- **Disasm/config:** splat / spimdisasm _(tools/open-spyro `split` group;
  `make split` runs them via `uv run`)_

## Comparing a function against the original

When a C edit breaks `make verify`, the per-function byte-diff shows where:

```sh
uv run --project tools/open-spyro -- open-spyro diff <FunctionName>
# e.g. open-spyro diff ComputeInventoryProgressPercent
```

It rebuilds the containing artifact _(the main EXE, or the function's overlay)_,
slices the function's bytes at its VMA out of both the rebuilt file and
`disc/orig`, and prints an instruction-level side-by-side plus a match %. Exit
codes: `0` = byte-identical _(`MATCH`)_, `1` = differs, `2` = cannot run. Useful
flags: `--no-build` reuses the current build output as-is; `--full` prints every
instruction rather than just the diff hunks.

## Building the ISO

`make iso` writes the repacked disc to the gitignored **`build/`** dir:

- `build/SpyrotheDragon.bin` + `build/SpyrotheDragon.cue`: the repacked disc
- `build/main/SCUS_942.28`: the rebuilt executable staged into it

## Editor IntelliSense

`make compdb` (after `tools/native_setup.sh`; configures, does not build) writes
a gitignored `compile_commands.json` covering every C file: game and overlay C
with the flags the native build compiles them with, plus the host runtime and
unit tests. Any clangd-based editor picks it up; in VS Code, install the
recommended clangd extension (`.vscode/` turns off the Microsoft C/C++
IntelliSense engine so the two do not conflict). Rerun it after adding a file.
