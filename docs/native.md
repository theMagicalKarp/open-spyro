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
git submodule update --init external/psyz external/tomlc17 external/imgui external/implot   # `make native` does this too; PsyZ's own submodules are not needed
make native                      # -> build/native/spyro (+ build/native/gate)
cp spyro.example.toml spyro.toml # then set its `disc` to your .cue
make native-run NATIVE_ARGS="--frames 300 --screenshot out.bmp"   # with spyro.toml (its `disc` key names the .cue)
make native-run NATIVE_ARGS="--frames 1900 --screenshot out.bmp --press 1300:start \
  --press 1400:start --shot 1550"   # scripted pad 1 + a screenshot at VSync 1550
make native-run NATIVE_ARGS="--fast --memcard none --frames 4400 --screenshot out.bmp \
  --press 1300:start --press 1400:start --press 1550:cross --press 1650:cross \
  --press 2300:start --warp 3400:21 --shot 3990"   # Dry Canyon, unthrottled
make native-run NATIVE_ARGS="--frames 1900 --capture out.bmp --internal-res 4 \
  --press 1300:start --press 1400:start --shot 1550"   # the displayed frame at 4x (2048x960)
build/native/spyro --stub --frames 60    # no disc: a spinning-triangle test scene
make native-gate SAVESTATE=~/Library/Application\ Support/DuckStation/savestates/<state>.sav
make native-test                 # host unit tests (native/tests): C compiler + cmake only
open-spyro neutral [--ref main] [--host]   # shared-code change neutral for the PS1 cpp?
```

`make clean` empties `build/`, including `build/native-tools`; re-run
`tools/native_setup.sh` after it.

### Config file

Settings that stay the same from run to run can live in a TOML file instead
of on the command line. `spyro` reads `config.toml` from the user's data
directory (`~/Library/Application Support/open-spyro/spyro/` on macOS,
`~/.local/share/open-spyro/spyro/` on Linux) when it exists;
`--config PATH` reads that file instead (and it must exist), and
`--no-config` reads none, for repeatable scripted runs. `--config` can be
given more than once (up to 8): the files are read in order and a later one
overrides an earlier one, so a small file with only a few settings can be
layered on a base config. Flags override the files. `make native-run` passes
`--config spyro.toml` instead: the repo-root config, a gitignored copy of
[`spyro.example.toml`](../spyro.example.toml) (`NATIVE_CONFIG=other.toml` for
another, `NATIVE_CONFIG=` for the user's own); set `disc` there to point it at
your dump.

```toml
disc = "/path/to/Spyro.cue"   # --disc; a relative path is relative to this file

[memcard]
port1 = "card1.mcd"            # --memcard; "none" leaves port A empty
port2 = "card2.mcd"            # --memcard2

[input]
pad = "analog"                 # "digital": pad 1 reports a digital pad

[input.analog.left]            # and [input.analog.right] (radial, [0.15, 1.0])
mode = "original"              # or "radial": the two below
range = [0.375, 1.0]           # [inner, outer], fractions of full deflection
curve = 1.0

[input.camera]
mode = "original"              # "analog": the right stick turns the camera
invert = false
speed = 1.0
boost = 1.0
boost_time = 0.5

[video]
internal_resolution = 1        # --internal-res: render scale 1..8, 0 = auto
aspect = "4:3"                 # --aspect: "16:9", "21:9", "W:H" up to 8:3, "auto"

[debug]
fast = false                   # --fast
log_level = "info"             # --log-level: "debug", "info", "warn", "error" (or d/i/w/e)
overlay = false                # start with the diagnostics overlay shown
cull_margin = 0                # widened views: cull this many pixels past each side

[debug.start]                  # the state a run starts in
level = 0                      # a level id: boot straight into it, 0 = title screen
```

Every key is optional. Each setting has a dotted name, its table and key
(`memcard.port1`, `debug.fast`), and `--set KEY=VALUE` sets any of them from
the command line (`--set debug.log_level=debug`; a relative path there is
relative to the working directory). The older flags are short forms of
`--set`: `--disc`, `--memcard`, `--memcard2`, `--fast`, `--log-level`,
`--internal-res` and `--aspect`.
The per-run flags (`--frames`, `--press`, `--shot`, `--warp`,
`--screenshot`, `--capture`, `--wav`) have no config key. The settings are one table in
`src/native/host/config.c` (`settings[]`), which the config files, `--set` and
the short flags all go through; a new setting is one row there. A malformed
file, an unknown key or table, or a value of the wrong type or out of range
stops the run, with every bad key reported by file and line. The parser is
[tomlc17](https://github.com/cktan/tomlc17) (MIT), the `external/tomlc17`
submodule, compiled into the host executable only (`native/CMakeLists.txt`).

### Unit tests

`make native-test` builds and runs `native/tests` (a CMake project of its own
in `build/native-test`, with ASan and UBSan), which needs only a C compiler,
cmake and the tomlc17 submodule. It covers the native code that has no SDL or
wasm dependency:

- `src/native/host/config.c`: the config files, `--set` and the `--press` /
  `--warp` / `--log-level` value formats that `main.c` uses.
- `src/native/wasm/libc.c`: the printf family, sscanf, string functions and
  stdio. The test includes `libc.c` with each libc name renamed to `w_<name>`,
  so the module's implementation runs beside the host's.
- `src/native/wasm/memcard.c`: card presence, formatting, the directory and
  block chains, and reads/writes round-tripped through the `.mcd` image.
- `src/native/wasm/shim_helpers.c`: the SDK shim's texpage masking of GPU
  packets, the GTE MulMatrix0 and the CdlGetlocP result `sdk_shim.c` fakes.
- `src/native/handasm/runtime.c`: the GTE side-effect layer (SXYP, IRGB/ORGB,
  LZCS/LZCR) over a fake GTE, the R3000 mult/div macros and the hand-asm
  leaves that touch no memory (hand-asm loads and stores need wasm32
  addresses).
- `src/native/host/host_io.c`: the module's file-handle table, the memory-card
  path copy and the log filter (`imports.c` forwards to it).
- `src/native/host/script.c`: `--press` hold windows, the scripted buttons on
  pad 1's report and the `--shot` file names (`main.c`).
- `src/native/host/stick.c`: the stick dead zones and curve, checked through
  a model of the game's stick calibration (`main.c`).
- `src/native/game/camera/manual_yaw.c`: L2/R2 and the right-stick camera
  turn, its speed, invert and boost ramp, over the game globals it reads
  (defined by the test).
- `src/native/game/debug/warp.c`: when `--warp` and `debug.start.level` may
  leave the current state and what they hand the game, the story-intro
  fly-over cut, over stubs for the game calls (`funcs.h` is kept out, since
  its struct size checks are 32-bit only).
- `src/native/host/audio_format.c`: the sample frames per vblank and the
  `--wav` header (`audio.c`).
- `src/native/host/diag_stats.c`: the diagnostics overlay's frame rate and
  frame-time averages, frame-time history and state names (`diag.cpp`).
- `src/native/host/psx_memory.c`: the address-space layout and the KUSEG
  mirror (`mach_vm_remap` on macOS, a shared memfd on Linux), built only when
  the wasm2c runtime from `tools/native_setup.sh` is in `build/native-tools`
  (CI sets it up before the tests).

Where a unit sits next to SDL, PsyZ or wasm2c code, its testable logic lives
in its own file (`host_io.c`, `script.c`, `stick.c`, `audio_format.c`, `diag_stats.c`, `shim_helpers.c`)
and the original file calls it.

A unit's tests live next to it as `<unit>_test.c` (`src/native/host/config_test.c`),
which `native/tests/CMakeLists.txt` picks up by glob; `native/tests` holds only
the harness. The wasm-side tests include their unit rather than link it, and
implement the `host_*` imports it calls (`host_imports.h` drops the wasm import
attributes off wasm). The native game replacements' tests
(`src/native/game/<subsystem>/<unit>_test.c`) include their unit too, with the
game headers in `include/`, and define the game globals it uses. A test is a `TEST(name)` block with `CHECK`, `CHECK_INT` and
`CHECK_STR` (`native/tests/test.h`); `build/native-test/native_tests FILTER`
runs only the tests whose name contains FILTER.

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
(or `memcard` in the config file) names another; `--memcard none` leaves it empty, and `--memcard2` fills port
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
L1 / R1 / L2 / R2, Return = Start, Backspace = Select. Pad 1 reports a
DualShock in analog mode (id 0x73; `input.pad = "digital"` makes it a digital
pad, 0x41): the left stick steers Spyro (and the free-look camera) through
the game's own analog path, which calibrates the stick centre from the first
analog report and never reads the right stick. The keyboard leaves both
sticks centred. PsyZ fills the direct-mode report buffers on every VBlank;
the game's own VSync callback (`PollPadAndDistributeInput`) reads them.

Backtick (`` ` ``), or L3 + R3 on a gamepad (click one stick while holding the other in),
shows and hides the diagnostics overlay (`src/native/host/diag.cpp`, not in
the original): presented FPS, a graph of the last 240 frames' rates, the mean
and longest time between presents, the time the game ran between blocking
VSyncs, the VSync count, `g_nGamestate` and `g_nActiveLevelId`; the averages
refresh every half second. `debug.overlay = true` starts it shown. It is a
[Dear ImGui](https://github.com/ocornut/imgui) panel with an
[ImPlot](https://github.com/epezent/implot) graph (both MIT, the
`external/imgui` and `external/implot` submodules), drawn through PsyZ's
SDL_GPU overlay hooks with ImGui's SDL_GPU backend (Metal or Vulkan), in the
system's monospace font (SF Mono, else DejaVu Sans Mono, else ImGui's own).
If ImGui can't start, the frame rate goes in the window title instead. PsyZ's own keys stay: F4
fullscreen, F6 the VRAM view, Escape quits.

Each stick has a `mode` (`input.analog.left.mode` / `input.analog.right.mode`).
`"original"`, the left stick's default, passes the report through untouched,
so the game's own calibration applies: a 0x30-per-axis dead zone around the
centre it snapshots, linear past that. `"radial"`, the right stick's default,
shapes the stick on the host before the game sees it
(`src/native/host/stick.c`): a radial `range = [inner, outer]` (below inner
the stick reads centred, from outer on it reads full, linear between) and a
response `curve` (exponent; above 1 gives finer control near the centre).
Those two apply only in radial mode.
The shaped value is encoded just past the game's dead zone, so the shaped
deflection reaches the game unchanged and the host's dead zone replaces the
game's. The left stick's radial inner default, 0.375, is the original's size. The
game never reads the right stick, so in `"original"` mode the right-stick
camera turn gets the raw stick with no dead zone.

`input.camera.mode = "analog"` adds what the original lacks: the right
stick's X axis turns the camera the way L2 / R2 do, in proportion to the
deflection, with the buttons taking priority; stick right turns the way R2 does, and
`invert = true` flips that. `speed` scales the rate (1 = the
L2/R2 rate); held at full deflection it ramps to `boost` times that over
`boost_time` seconds (boost 1 = no ramp). Moving it counts as input the way L2 / R2 do: it
restarts the standing camera's 1.5 s countdown before it swings back behind
Spyro. The camera spring eases every turn
in and out on its own. It is the native `UpdateCameraManualYawInput`
(`src/native/game/camera/manual_yaw.c`); L2 / R2 keep the original rate.

For runs without a person at the keyboard, `--press N:BUTTONS[:HOLD]` holds
buttons (names joined by `+`: `cross+up`) for HOLD VSyncs (default 4) from
VSync N, and `--shot N` writes a VRAM screenshot at VSync N next to
`--screenshot` (`out-N.bmp`). `--capture out.bmp` writes the displayed
frame instead of VRAM, at the internal resolution (and with `--shot`, one
next to it per shot too); the two can be given together.

`video.internal_resolution` (`--internal-res N`) renders at N times
512x240, 1 to 8, or 0 for the window size over the game's size. Anything
but 1 puts PsyZ in its native display mode (a 512x240 canvas of its own,
centred), which is where it scales; 1, the default, keeps the PS1 display
path. Either way the frame fills the window at 4:3 (PsyZ's native mode
alone would only scale by whole multiples; the host reports a display
larger than any window so it fits it instead), and auto picks the
smallest scale at least as tall as that fit. At 1x the two modes capture the same frame, up to a few pixels one
5-bit step apart in 3D scenes (gouraud interpolation rounding).
The pause and menu backdrops stay at 1x under a scaled HUD and menu: the
game reads the frame back into RAM (`StoreImage`, `MoveImage`) and draws it
again as a texture, so it only ever sees the 512x240 copy.
`video.aspect` (`--aspect`) widens the view: `"16:9"`, `"21:9"`, any
`"W:H"` from 4:3 to 8:3 (PsyZ's widest canvas, 1024 game pixels), or
`"auto"` for the window's, followed as it is resized. It asks PsyZ's native
mode for a canvas wider than the 512x240 framebuffer, at the same height and
the same projection, centred on it (688 wide at 16:9, x -88..600; 896 at
21:9), and the run logs the view's game-coordinate bounds. The game's
side culls, hand-written asm tuned to the 4:3 edges, are widened to the view
by native patches to the hand-asm translation
(`tools/open-spyro/src/open_spyro/handasm_patches.py`, `include/native_view.h`):
the world chunks, actors, sprites, sky, particles, billboards and the portal
skies' clip rect (`portal_sky.c`). The per-zone visibility lists don't depend
on the view's width, so nothing is missing at the new edges. Actors in the
wider strips count as drawn, which the game uses to decide what to animate,
so a run at 21:9 drifts from the same run at 4:3 after a while.
`debug.cull_margin = N` makes the culls reach N game pixels past each side
of a widened view. Comparing a run with it against one without shows what
a cull still drops at the edges. PsyZ draws primitives of any width, so
polygons spanning the whole canvas are not dropped.
The sky's segment walks skip segments close to the camera plane, which the
4:3 view never showed; that margin shrinks as the view widens, or the
strips' top corners would show the clear colour. Known issue: wider views
also emit more primitives than the original 4:3 ones,
into the same per-frame primitive buffer (0x1C000 bytes, carved from the top
of the PS1's RAM): when it fills, the rest of that frame's polygons are
dropped. A sweep of every level, spinning the camera, found this at 21:9 in
level 53 and at 8:3 in levels 50 and 53, and nowhere at 4:3.
Geometry still snaps to the 512x240 grid at any scale: the GTE returns
whole-pixel screen coordinates, and removing the jitter needs subpixel
vertices through PsyZ's GTE and packet path (a PGXP-style change in the
`external/psyz` submodule, not done). A VSync here is a blocking one: the game runs
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
(`src/native/game/debug/warp.c`). `debug.start.level = LEVEL` (or
`--set debug.start.level=LEVEL`) instead boots into a level: once the title
screen's scene has loaded, it takes the title's load-game exit into LEVEL
with a fresh game (no memory-card save loaded), with no pad input needed.
Until the level is in play nothing is shown or heard and the run is
`--fast`; the boot screens' VSyncs are presented with the display held off
(black, but the GPU work is still submitted), and the story intro's
fly-over is cut short (`native_skip_intro_fly`), so the game opens on the
level a fraction of a second after launch. It then drops back to real time,
with `VSync(-1)` carrying on from the VSync count.
`--wav out.wav` writes the sound output to
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
