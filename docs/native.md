# The native build

The native build turns the decompiled game into a regular desktop program. It
does not emulate a PlayStation: the matched C is compiled for your machine and
runs directly. It reads the game data from your own disc image.

It runs on:

- macOS on Apple Silicon
- Linux on arm64 or x86_64

**Status: experimental, but playable.** The boot splashes, title menu, opening
cutscene and all 35 levels (hubs, flight levels and bosses) work, along with
sound effects, music, cutscene narration and memory-card saves.

The native build is separate from the byte-matching PS1 build. Nothing here
affects the PS1 output, and `make check` is still the test every change to
shared code has to pass. The original feasibility study is in
[`native-spike.md`](native-spike.md).

- [Quick start](#quick-start)
- [Playing](#playing)
- [Configuration](#configuration)
- [Display options](#display-options)
- [Scripted and headless runs](#scripted-and-headless-runs)
- [Development](#development)
- [How it works](#how-it-works)
- [Known issues](#known-issues)

## Quick start

```sh
tools/native_setup.sh              # one-time: download and build the pinned toolchain and SDL3
make native                        # build build/native/spyro
cp spyro.example.toml spyro.toml   # then set `disc` in spyro.toml to your .cue file
make native-run                    # play
```

`tools/native_setup.sh` installs everything into `build/native-tools/`: a pinned
wasi-sdk, wasm2c and a static build of SDL3. On macOS it also installs cmake
through Homebrew. On Linux it prints the Debian/Ubuntu packages to install if
anything is missing. `make native` fetches the git submodules it needs by
itself.

`make clean` deletes all of `build/`, including the toolchain, so run
`tools/native_setup.sh` again afterwards.

**Linux notes.** Rendering uses Vulkan, so you need a Vulkan driver. Mesa's
`mesa-vulkan-drivers` package works, and includes the `lavapipe` software
renderer for machines without a GPU. On a machine with no display at all, set
`SDL_VIDEODRIVER=offscreen`.

The finished executable has no unusual runtime dependencies. SDL3 is linked in
statically. On macOS it needs only the system libraries. On Linux it needs glibc
and the usual X11/Wayland, Vulkan and audio libraries.

**No disc?** `build/native/spyro --stub` shows a spinning-triangle test scene
instead. CI uses it as a smoke test.

## Playing

### Controls

Pad 1 is the keyboard, or the first gamepad SDL finds. The keyboard layout
follows DuckStation's, except that the camera buttons sit next to the face
buttons.

| Keyboard        | PS1 pad                            |
| --------------- | ---------------------------------- |
| W A S D, arrows | D-pad                              |
| K, Space        | Cross (Space is there for jumping) |
| J               | Square                             |
| I               | Triangle                           |
| L               | Circle                             |
| Q / E           | L1 / R1                            |
| U / O           | L2 / R2 (turn the camera)          |
| Return          | Start                              |
| Backspace       | Select                             |
| (unbound)       | L3 / R3                            |

Every button can be rebound in the config file, under `[input.keyboard]`. Each
button takes a key name or a list of up to four. An empty list unbinds it:

```toml
[input.keyboard]
cross = ["k", "space"]
l2 = "f1"
select = []
```

Key names include letters and digits, `space`, `return`, `up`, `lshift`, `f1`,
`kp_4` and so on (see `src/native/host/keymap.c`).

These keys always belong to the program and can't be rebound:

| Key     | Action                                                      |
| ------- | ----------------------------------------------------------- |
| `` ` `` | Show or hide the diagnostics overlay (L3 + R3 on a gamepad) |
| `=`     | Cycle the render scale: 1x, 2x, 4x, 8x                      |
| F4      | Toggle fullscreen                                           |
| F6      | Show the VRAM viewer                                        |
| Escape  | Quit                                                        |

The diagnostics overlay shows the frame rate, a graph of recent frame times, the
current game state and the current level. It is not part of the original game.

### Gamepads and analog sticks

The game sees pad 1 as a DualShock in analog mode. The left stick moves Spyro
through the game's own analog code. Set `input.pad = "digital"` to present a
plain digital pad instead.

The original game never reads the right stick. The native build can optionally
use it to turn the camera (see [Camera](#camera) below).

### Memory cards

Saves go to a `.mcd` card image, the same 128 KiB format emulators use, so you
can move saves between this build and DuckStation.

By default, port 1 is `card1.mcd` in your data directory:

- macOS: `~/Library/Application Support/open-spyro/spyro/`
- Linux: `~/.local/share/open-spyro/spyro/`

The example config sets it to `card1.mcd` in the repo root instead. If the file
doesn't exist, the game sees a freshly formatted card, and the file is created
the first time the game saves. Use `--memcard none` to leave port 1 empty, and
`--memcard2 PATH` to put a card in port 2.

## Configuration

### Where settings come from

Settings are read in this order, with later sources winning:

1. `config.toml` in your data directory (the folder listed under
   [Memory cards](#memory-cards)), if it exists.
2. Files given with `--config PATH`. These replace step 1. You can pass
   `--config` up to eight times, and each file overrides the ones before it, so
   a small file can tweak a larger base config.
3. Command-line flags.

`--no-config` skips the config files entirely, which is handy for repeatable
scripted runs.

`make native-run` passes `--config spyro.toml`, the gitignored copy of
[`spyro.example.toml`](../spyro.example.toml) in the repo root. Use
`NATIVE_CONFIG=other.toml` to point it at a different file, or `NATIVE_CONFIG=`
to fall back to the one in your data directory. Extra flags go in
`NATIVE_ARGS="..."`.

Every key is optional. A bad file, an unknown key or an out-of-range value stops
the program, and every problem is reported with its file and line.

### Setting any value from the command line

Every setting has a dotted name made of its table and key, such as `debug.fast`
or `memcard.port1`. `--set NAME=VALUE` sets any of them:

```sh
build/native/spyro --set debug.log_level=debug --set input.keyboard.cross=k,space
```

Relative paths given through `--set` are relative to the current directory. In a
config file, they are relative to that file.

A few common settings also have short flags:

| Flag                   | Setting                  |
| ---------------------- | ------------------------ |
| `--disc PATH`          | `disc`                   |
| `--memcard PATH`       | `memcard.port1`          |
| `--memcard2 PATH`      | `memcard.port2`          |
| `--fast`               | `debug.fast`             |
| `--log-level LEVEL`    | `debug.log_level`        |
| `--resolution-scale N` | `video.resolution_scale` |
| `--aspect W:H`         | `video.aspect`           |

### Settings reference

[`spyro.example.toml`](../spyro.example.toml) lists every setting with a short
comment. In brief:

| Setting                          | Default      | Meaning                                                      |
| -------------------------------- | ------------ | ------------------------------------------------------------ |
| `disc`                           |              | Your `.cue` file                                             |
| `memcard.port1`, `memcard.port2` | see above    | Card image paths, or `"none"`                                |
| `input.pad`                      | `"analog"`   | `"digital"` to present a digital pad                         |
| `input.analog.left.*`            | original     | Left stick shaping (see [Stick tuning](#stick-tuning))       |
| `input.analog.right.*`           | radial       | Right stick shaping                                          |
| `input.camera.*`                 | original     | Right-stick camera (see [Camera](#camera))                   |
| `input.keyboard.*`               | see Controls | Key bindings                                                 |
| `video.resolution_scale`         | `1`          | 1 to 8, or `"auto"` to match the window                      |
| `video.aspect`                   | `"4:3"`      | `"16:9"`, `"21:9"`, any `"W:H"` up to 8:3, or `"auto"`       |
| `video.hud`                      | `"center"`   | `"edges"` moves the HUD counters to the sides of a wide view |
| `debug.fast`                     | `false`      | Run as fast as possible, without the 59.94 Hz frame limiter  |
| `debug.log_level`                | `"info"`     | `"debug"`, `"info"`, `"warn"` or `"error"`                   |
| `debug.overlay`                  | `false`      | Start with the diagnostics overlay showing                   |
| `debug.cull_margin`              | `0`          | Draw this many extra pixels past each edge of a wide view    |
| `debug.start.level`              | `0`          | Boot straight into this level id (`0` is the title screen)   |

### Stick tuning

Each stick has a `mode`:

- **`"original"`** (left stick default) passes the stick through untouched, so
  the game's own dead zone applies.
- **`"radial"`** (right stick default) shapes the stick before the game sees it,
  using two extra settings:
  - `range = [inner, outer]`: below `inner` the stick reads as centred, past
    `outer` it reads as fully pushed, and it is linear in between. Values are
    fractions of full deflection.
  - `curve`: a response exponent. `1` is linear; higher values give finer
    control near the centre.

The left stick's default radial range, `[0.375, 1.0]`, matches the size of the
original dead zone. Gamepads often feel better with about `[0.1, 0.95]`.

### Camera

In the original game, L2 and R2 turn the camera. Setting
`input.camera.mode = "analog"` also lets the right stick turn it, at a speed
that follows how far the stick is pushed. L2 and R2 keep working and take
priority.

| Setting                   | Meaning                                                          |
| ------------------------- | ---------------------------------------------------------------- |
| `input.camera.invert`     | Reverse the stick direction                                      |
| `input.camera.speed`      | Turn speed at full deflection; `1` matches L2/R2                 |
| `input.camera.boost`      | When held fully, speed up to this many times `speed` (`1` = off) |
| `input.camera.boost_time` | Seconds to reach full boost                                      |

Moving the stick counts as camera input, just like L2/R2, so the camera waits
the usual 1.5 seconds before swinging back behind Spyro.

## Display options

### Resolution scale

`video.resolution_scale` renders the 3D scene at a multiple of the PS1's
512x240. The default, `1`, uses a display path that closely follows the PS1's
own. Any other value, including `"auto"`, switches to a scaled renderer. The `=`
key cycles through 1x, 2x, 4x and 8x while the game runs.

The image always fills the window at the chosen aspect ratio. `"auto"` picks the
smallest scale that is at least as tall as the window.

Some things stay at the original resolution:

- **Geometry still snaps to the 512x240 pixel grid.** The PS1's geometry
  hardware produces whole-pixel coordinates. Removing that jitter would take
  PGXP-style subpixel support in PsyZ, which hasn't been done.
- **Pause and menu backdrops** look lower resolution than the HUD and text on
  top of them. The game copies the frame into RAM and redraws it as a texture,
  so it only ever has the 512x240 copy.

The PS1 only shows lines 8 to 232 of its 240-line image, because TVs hid the
rest. The scaled renderer crops to those lines; the 1x path shows all 240.

### Widescreen

`video.aspect` widens the view to `"16:9"`, `"21:9"`, any `"W:H"` between 4:3
and 8:3, or `"auto"` to follow the window as you resize it. With a fixed aspect,
the window opens at that shape.

The game culls anything outside its original 4:3 view, so the native build
widens those checks to fit the new view. Things that were checked at 21:9
against 4:3, with nothing found missing at the edges:

- the hubs, a normal level, a flight level and a boss
- the title screen and menus, the intro cutscene and a dragon rescue
- pause, inventory, the attract demo, dying and respawning, and game over
- the first level of every world

The 2D screens (title, menus, pause, inventory, cutscene text) stay in a centred
4:3 area. So does the HUD, unless you set `video.hud = "edges"`. That moves the
gem counter and eggs to the left edge and the lives and key to the right edge,
at the same distance from the edge as in the original. The dragon counter stays
centred, and flight levels keep their own HUD.

Things to know:

- **Cutscenes were framed for 4:3**, so characters can stand half in the extra
  space at the sides. The intro's dragons do.
- **Runs drift over time.** The game only animates actors it thinks are on
  screen, and more of them are on screen in a wide view. So a scripted run at
  21:9 slowly diverges from the same run at 4:3.
- **Pause backdrops lose some horizontal detail** (about 0.74x at 16:9), because
  the wide view is squeezed into the 512-pixel-wide copy and stretched back out.

`debug.cull_margin = N` makes the game draw `N` extra pixels past each edge.
Comparing runs with and without it is a quick way to spot anything that is still
culled too early.

## Scripted and headless runs

The native build can play itself with pre-scripted input and take screenshots,
which is how most testing is done.

| Flag                       | Effect                                                                                                 |
| -------------------------- | ------------------------------------------------------------------------------------------------------ |
| `--frames N`               | Quit after N VSyncs                                                                                    |
| `--press N:BUTTONS[:HOLD]` | Hold buttons on pad 1 from VSync N for HOLD VSyncs (default 4). Join buttons with `+`, e.g. `cross+up` |
| `--screenshot out.bmp`     | Save a screenshot of VRAM when the run ends                                                            |
| `--shot N`                 | Also save one at VSync N, as `out-N.bmp`                                                               |
| `--capture out.bmp`        | Save the displayed frame at its scaled size, instead of or as well as VRAM                             |
| `--warp N:LEVEL`           | Load level id LEVEL at VSync N, as the developer level select did                                      |
| `--fast`                   | No frame limiter; time advances one frame per VSync, so runs are repeatable                            |
| `--wav out.wav`            | Also record the audio to a file                                                                        |

These flags apply to one run only and have no config-file equivalent.

Some level ids: 10 is Artisans, 21 is Dry Canyon. Setting
`debug.start.level = LEVEL` boots straight into a level with a fresh game, no
button presses needed. It skips the boot screens quickly with nothing shown or
heard, then switches back to normal speed once the level is playing.

Examples:

```sh
# Run 300 VSyncs and take a screenshot
make native-run NATIVE_ARGS="--frames 300 --screenshot out.bmp"

# Press Start twice to skip the title flyby, screenshot at VSync 1550
make native-run NATIVE_ARGS="--frames 1900 --screenshot out.bmp \
  --press 1300:start --press 1400:start --shot 1550"

# Same, but capture the displayed frame at 4x (2048x960)
make native-run NATIVE_ARGS="--frames 1900 --capture out.bmp --resolution-scale 4 \
  --press 1300:start --press 1400:start --shot 1550"

# Warp to Dry Canyon, as fast as possible
make native-run NATIVE_ARGS="--fast --memcard none --frames 4400 --screenshot out.bmp \
  --press 1300:start --press 1400:start --press 1550:cross --press 1650:cross \
  --press 2300:start --warp 3400:21 --shot 3990"
```

Tips for writing scripts:

- The game runs at one frame per two VSyncs.
- On a cold boot the title flyby lasts until about VSync 1300. Start skips it,
  and a second Start opens the memory-card check.
- A memory card changes which menus appear, so pass `--memcard none` (or a
  scratch card) to keep runs repeatable.

## Development

### Make targets

| Command                                    | What it does                                                                             |
| ------------------------------------------ | ---------------------------------------------------------------------------------------- |
| `make native`                              | Build `build/native/spyro` and the gate tool                                             |
| `make native-run`                          | Run with `spyro.toml`; extra flags in `NATIVE_ARGS`                                      |
| `make native-test`                         | Run the host unit tests (needs only a C compiler and cmake)                              |
| `make native-gate SAVESTATE=...`           | Check the level loader against a DuckStation savestate ([below](#the-level-loader-gate)) |
| `open-spyro neutral [--ref main] [--host]` | Check that a shared-code change leaves the PS1 build unchanged                           |

### Unit tests

`make native-test` builds `native/tests` as its own CMake project in
`build/native-test`, with AddressSanitizer and UndefinedBehaviorSanitizer turned
on. It tests the native code that doesn't depend on SDL or wasm, including the
config parser, the libc, memory cards, input scripting, stick shaping, the
camera, level warps, audio, the diagnostics overlay and the memory layout.

Tests live next to the code they test, as `<unit>_test.c` (for example
`src/native/host/config_test.c`), and are picked up automatically. A test is a
`TEST(name)` block using `CHECK`, `CHECK_INT` and `CHECK_STR` from
`native/tests/test.h`. To run only some tests:

```sh
build/native-test/native_tests FILTER   # tests whose name contains FILTER
```

When code is tied up with SDL, PsyZ or wasm2c, the testable part is split into
its own file (`host_io.c`, `script.c`, `stick.c`, `audio_format.c`,
`diag_stats.c`, `shim_helpers.c`) so it can be tested alone. Tests for code that
runs inside wasm `#include` the file under test rather than linking it, and
provide the `host_*` functions it calls. Tests for native game code also define
the game globals the code uses.

### Changing shared code

The game C is shared with the PS1 build, and the PS1 build must stay
byte-identical. Before committing a change to shared code, run
`open-spyro neutral`. It compares the PS1 preprocessor output before and after
your change, ignoring whitespace and line breaks. `make check` is the final
word.

Where the PS1 source can't work as-is natively, `include/port.h` provides macros
that expand to the original text on PS1:

| Macro                       | PS1                            | Native                                                                 |
| --------------------------- | ------------------------------ | ---------------------------------------------------------------------- |
| `REG_PIN(type, name, "$n")` | `register type name asm("$n")` | `type name`                                                            |
| `PORT_VOLATILE`             | (nothing)                      | `volatile`, for globals some headers declare volatile and others don't |

`match_asm.h` also has plain-C native versions of its matching helpers. Other
native-only code is either inside `#ifdef __psyz` or in `src/native/game/`,
which replaces PS1 code that has no native meaning (such as computed `goto *ra`
jumps and file-scope register variables).

Native game C is built with `-Werror=implicit-function-declaration`. On wasm,
calling an undeclared function guesses its signature, and the call crashes if
the guess is wrong.

### The level loader gate

`native/gate/` checks that the level loader produces exactly the same memory as
the PS1. It builds the real `src/main/load/level.c` on its own, loads a level
from the original `WAD.WAD`, and compares the result against RAM from a
DuckStation savestate taken in that level. It checks that:

- every pointer the native loader fixed up matches the PS1 RAM;
- no pointer the PS1 fixed up was missed;
- every global the loader sets up (chunk tables, collision header, actor pool,
  and so on) matches.

Result on level index 10:

```
published globals: 32/32 match
world bundle   0x80087210+0x054014: 307 words relocated, 307 match PS1, 0 differ
mesh chunk     0x800db224+0x08d800: 1266 words relocated, 1266 match PS1, 0 differ
level archive  0x80168a24+0x00b000: 107 words relocated, 107 match PS1, 0 differ
gate: PASS
```

The full output also lists "runtime-only changes". Those are values the game
changed after loading, such as actor state and animated vertices, and the gate
ignores them.

## How it works

### The big picture

The game's C code assumes 32-bit pointers and that PS1 RAM sits at its real
addresses, such as `0x80000000`. A normal 64-bit program can't put memory there.
So the game is compiled to **wasm32**, where every address is just an offset
into one block of memory, and **wasm2c** then turns that WebAssembly back into C
for the normal host compiler:

```
game C + translated hand-asm + PsyZ core + src/native/wasm
    --(wasi-sdk clang)-->  spyro.wasm
spyro.wasm
    --(wasm2c)-->          spyro_w.c
spyro_w.c + src/native/host + PsyZ's SDL3 backend
    --(clang or gcc)-->    build/native/spyro
```

The result is ordinary native code. wasm2c protects memory with guard pages, so
there is no bounds check on each memory access.

[PsyZ](https://github.com/Xeeynamo/psyz) provides PC versions of the PS1 SDK
libraries (graphics, sound, CD, controllers) and an SDL3 backend. The native
build uses it for everything the PS1 hardware used to do.

### Memory layout

Defined in `src/native/host/psx_memory.h`:

| Address                 | Contents                                                   |
| ----------------------- | ---------------------------------------------------------- |
| `0x00000000–0x001FFFFF` | A mirror of main RAM (the same memory, mapped twice)       |
| `0x01000000–0x017FFFFF` | Static data for the native-only code                       |
| `0x01800000–0x01FFFFFF` | The C stack                                                |
| `0x1F800000`            | Scratchpad RAM                                             |
| `0x80000000–0x807FFFFF` | Main RAM: the PS1's 2 MiB, plus extra room the dev kit had |

Only pages that are actually used take up memory. The RAM mirror uses
`mach_vm_remap` on macOS and a shared `memfd` on Linux.

### Game globals at their PS1 addresses

Every game global stays an `extern` declaration with no definition. The wasm
module is built position-independent, so the linker turns each one into an
import, and `open-spyro native got-table` fills those imports in with the
addresses from the same symbol files the PS1 build uses
(`config/symbol_addrs.txt` and the `undefined_*` lists).

So every global sits at exactly its PS1 address, and tricks the game relies on,
such as two names for one address or arithmetic across symbols, keep working.
**No source changes were needed for this.**

### Booting

`spyro --disc game.cue` does what the PS1 BIOS would
(`src/native/wasm/game_main.c`): it opens the disc image, finds the game
executable `SCUS_942.28`, and loads it at `0x80010000`. That puts the game's
data at its original addresses. The machine code in the file is ignored, since
the compiled C replaces it. Then it calls the game's `main`.

Levels need nothing special. The game loads each level's overlay from `WAD.WAD`
into memory itself, which puts its data in the right place, and since every
overlay's C is already compiled in, the game's per-level function table just
points at native functions. Level ids 0–9 and 99 refer to overlays that aren't
on the disc; they stop the program if reached
(`src/native/game/load/level_id_stubs.c`).

### The PS1 SDK

The game calls the PS1 SDK libraries through the names this project gave them.
`src/native/wasm/sdk_shim.c` defines each one and forwards it to PsyZ. Where
PsyZ behaves differently from the real SDK, the shim fills the gap. For example:

- `SetDrawMode` writes the real PS1 packet format, since PsyZ's version is laid
  out differently and corrupts the next packet.
- `DrawOTag` converts the game's PS1-format draw list into PsyZ's format. It
  also clears some texture bits that the PS1 GPU ignored but PsyZ treats as
  "untextured", which otherwise left most of the terrain untextured.
- `CdRead` finishes immediately, then calls the game's callback.
- `LoadExec` and a couple of BIOS calls just log a message.

`src/native/wasm/game_prelude.h` is included into all game C. It renames game
functions whose names clash with PsyZ's, such as the game's `SpuInit`, which
actually resets the memory card.

The native code carries its own small libc (`src/native/wasm/libc.c`), because
wasi-sdk's libc can't be linked into a position-independent module. File access,
logging and other services go through `host_*` functions on the native side
(`src/native/wasm/host_imports.h`).

PsyZ makes `u_long` pointer-sized, so it is 32 bits inside the wasm module and
64 bits in the native graphics backend. `src/native/host/imports.c` converts
graphics packets between the two.

### One thread

A wasm2c module can only be used from one thread. PsyZ normally runs timers on a
separate thread, so the native build stops that thread from starting
(`src/native/wasm/psyz_bridge.c`) and PsyZ runs its timers once per frame
instead.

### Sound

PsyZ emulates the PS1 sound chip inside the module, including voices, envelopes,
reverb and CD/XA audio. Normally SDL's audio thread would pull samples from it,
but that would mean a second thread entering the module. Instead,
`src/native/host/audio.c` asks the module for samples once per frame and keeps
SDL's audio buffer about 46 ms ahead.

This means sound only advances between frames, whereas on a PS1 it runs
alongside the game. Two bugs came from that, both fixed:

- **Silent cutscene narration.** While waiting for the disc, the game checks
  whether each sound has finished. Disc reads are instant here, so the sound
  chip hadn't run yet and a sound that had just started looked finished, and was
  stopped. `GetSpuVoiceKeyStatus` now reports such sounds as still playing.
- **Silent title menu.** PsyZ clears sound memory the first time it starts the
  sound chip, which used to happen after the title screen's sounds were loaded.
  `native_main` now starts it before the game runs.

### Memory cards

`src/native/wasm/memcard.c` implements the memory-card calls the game makes
(check, read, write, create and format) on top of a `.mcd` file. Every change
writes the whole file. Operations finish instantly, and the game sees the result
the next time it checks.

### Hand-written assembly

About 100 of the game's functions were written by hand in assembly and can't be
matched as C. `open-spyro native handasm` translates them from `asm/text.s` into
C, one instruction at a time, as `src/native/handasm/ha_*.c`. These files are
generated but committed; run it again if the assembly changes.

Each MIPS register becomes a local variable, and each memory access becomes a
pointer access at the same PS1 address. Details:

- Functions that jump into each other are merged into one C function.
- Indirect jumps (`jr`) become a `switch` over every possible target, including
  jump tables and computed jumps into the middle of unrolled loops. An
  unexpected target stops the program with its address.
- Branch delay slots and the R3000's load delay are reproduced exactly.
- Geometry coprocessor (GTE) instructions go to PsyZ's GTE, with a few missing
  pieces added in `src/native/handasm/runtime.c`.
- The translated code has its own 256 KiB stack.
- Game C calls each function through a wrapper with its normal C signature.

Widescreen needs some hand-written culling code to see a wider view. Those
changes are applied as patches during translation
(`tools/open-spyro/src/open_spyro/handasm_patches.py`, `include/native_view.h`).

### Widescreen internals

The wider view is a wider canvas at the same projection, centred on the
original: 640 pixels wide at 16:9 (x from -64 to 576), 840 at 21:9. Its width is
rounded to a multiple of 8 game pixels.

- The culling checks for world geometry, actors, sprites, sky, particles,
  billboards, portal skies and Spyro's own model are widened to the view. The
  per-zone visibility lists don't depend on view width, so nothing is missing
  from them.
- The sky skips segments very close to the camera. That margin shrinks as the
  view widens, so the corners don't show the background colour.
- A wide view draws more than the original per-frame draw buffers can hold (the
  intro cutscene at 21:9 overflowed and crashed). The native build gives each
  frame a 512 KiB buffer in the extra RAM above 2 MiB instead
  (`NATIVE_PRIM_BUFFER_BASE` in `include/frame_db.h`).
- The HUD is laid out at its 4:3 positions, then moved to the view's edges when
  `video.hud = "edges"` is set, and again whenever the view changes
  (`NativeHudAnchor` in `src/native/game/render/view.c`).
- Pause backdrops are captured squeezed into 512 pixels and stretched back over
  the view (`NativeMenuBackdrop` and `NativeStretchBackdrop`).

### Function signatures must match exactly

On the PS1, calling a function with the wrong number or type of arguments
usually works. In WebAssembly it crashes. The wasm link therefore treats
signature mismatches as errors (`--fatal-warnings`). The game had three kinds of
mismatch, all fixed without changing the PS1 output:

- Overlay headers that declare a function differently from its definition. These
  get corrected declarations under `#ifdef __psyz`.
- `MoveActorByDelta`, which takes a struct by value but is called with three
  separate ints. `src/native/game/actor/move_by_delta.c` provides the three-int
  version.
- The title screen uses a return value from `RestartTitleCardCheckOnTriangle`,
  which is declared `void`. `src/native/game/gamestate/update.c` returns it.

PsyZ has a few mismatches of its own, smoothed over by adapters in
`src/native/wasm/psyz_fixups.c`.

The wasm link also passes `--no-wasm-opt`. Otherwise clang runs whatever
`wasm-opt` is on your `PATH`, which makes the output differ between machines and
strips function names.

## Known issues

- The translated hand-written assembly isn't tested against the original yet.
- Physics have only been compared with the PS1 by eye, not frame by frame
  against DuckStation.
- No scripted run has tested saving from inside a level yet. The title screen's
  memory-card check, file creation and save list have been tested.
- Cutscenes can show characters cut off in the extra widescreen area.
- Geometry still snaps to the PS1 pixel grid at higher resolutions.
