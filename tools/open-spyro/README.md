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
  diff               Byte-diff one function at its VMA against disc/orig
  neutral            Check shared-code edits leave the PS1 preprocessor output unchanged
  wad                WAD.WAD repack (unpack | pack | verify)
  native             Native-port helpers (got-table | handasm | gate)
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
- **`dev`** — `ruff`, `ty` (default group; used by the lint/type CI job).

`make setup` runs `uv sync --group split`. The Docker image installs
the **same group from this same `uv.lock`** (`uv export | uv pip install --system`,
see `tools/Dockerfile`), so host and image versions never drift — `uv.lock` is the
one source of truth. Only `maspsx` (invoked as a script by `compile.sh`) stays a
separate clone.
