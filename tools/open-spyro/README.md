# open-spyro tooling

First-party build tooling for the open-spyro byte-matching decompilation, exposed
through a single `open-spyro` CLI (built with [typer](https://typer.tiangolo.com/)).

```
open-spyro --help
  progress           Regenerate the C-match progress report
  sectionize         Per-function section wrapping of asm/text.s
  gen-overlay-yaml   Emit a splat config per overlay
  gen-syms-ld        PROVIDE() linker script pinning known symbols
  gen-slots-ld       Fixed-VMA text-slot fragment for the main EXE
  fixup-hasm         De-symbolize hand-asm reloc operands
  wad                WAD.WAD repack (unpack | pack | verify)
```

All commands resolve the repo root from the current working directory (the dir
holding `Makefile` + `config/`), so run them from the repo root — exactly how the
Makefile and CI invoke them.

## Development

```
uv sync                      # create the venv + install dev tools (ruff, ty)
uv run ruff check .
uv run ruff format --check .
uv run ty check
```

This project is the single home for the host Python toolchain — there is no bare
root `.venv` anymore. `make` and CI invoke everything via `uv run --project
tools/open-spyro …`, with optional dependency groups:

- **`split`** — `splat64[mips]` + `spimdisasm`, the disassembler behind `make split`.
- **`match`** — the interactive byte-matching tools (below).
- **`dev`** — `ruff`, `ty` (default group; used by the lint/type CI job).

`make setup` runs `uv sync --group split --group match`. The Docker image installs
the **same groups from this same `uv.lock`** (`uv export | uv pip install --system`,
see `tools/Dockerfile`), so host and image versions never drift — `uv.lock` is the
one source of truth. Only `maspsx` (invoked as a script by `compile.sh`) stays a
separate clone.

## Match-loop tools

The interactive byte-matching tools come from the `match` group:

```
uv sync --group match                          # m2c + asm-differ (pinned git deps) + permuter libs
uv run -- m2c <args>                            # asm → first-pass C
uv run -- asm-differ <args>                     # byte-diff at the function VMA
uv run -- python ../decomp-permuter/permuter.py <dir>
```

`m2c` and `asm-differ` are SHA-pinned git deps (locked in `uv.lock`, used by both
the host and the Docker image). `decomp-permuter` ships no installable package, so it runs from its clone
(`tools/decomp-permuter`, provisioned by `tools/setup.sh`); only its required libs
(`toml`, `Levenshtein`) are declared here. The other vendored tools (`maspsx`,
`mkpsxiso`) are not part of this package.
