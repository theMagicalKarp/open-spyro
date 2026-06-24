"""The ``open-spyro`` umbrella CLI.

Every first-party build tool is a subcommand here. Commands resolve the repo root
from the current working directory (see ``open_spyro.paths``), so run them from the
repo root — exactly how the Makefile and CI invoke them. Commands are named
explicitly (``progress``, ``gen-syms-ld``, ...) to match the Makefile / shell call
sites rather than typer's function-derived names.
"""

from __future__ import annotations

from pathlib import Path
from typing import Annotated

import typer

from open_spyro import (
    fixup_hasm,
    gen_overlay_yaml,
    gen_slots_ld,
    gen_syms_ld,
    normalize_binary,
    overlays,
    progress,
    progress_delta,
    sectionize,
    wad,
)

app = typer.Typer(
    help="Build tooling for the open-spyro byte-matching decompilation.",
    no_args_is_help=True,
    add_completion=False,
)


def _progress() -> None:
    """Regenerate the C-match progress report (PROGRESS.md + badge)."""
    progress.run()


def _progress_delta(
    base_ref: Annotated[
        str, typer.Option("--base-ref", help="base branch name (shown in the comment)")
    ],
    head_json: Annotated[
        Path, typer.Option("--head-json", help="freshly generated head progress.json")
    ],
    base_md: Annotated[
        Path, typer.Option("--base-md", help="base branch PROGRESS.md (may be empty/missing)")
    ],
) -> None:
    """Print the CI PR progress-delta comment body (head progress.json vs base PROGRESS.md)."""
    progress_delta.run(base_ref, head_json, base_md)


def _sectionize() -> None:
    """Per-function `.section`/`.ifndef HAVE_C_*` wrapping of asm/text.s."""
    sectionize.run()


def _gen_overlay_yaml() -> None:
    """Emit a splat config per overlay from config/overlays.json."""
    gen_overlay_yaml.run()


def _gen_slots_ld() -> None:
    """Emit the fixed-VMA text-slot fragment (config/spyro.main.slots.ld)."""
    gen_slots_ld.run()


def _fixup_hasm() -> None:
    """De-symbolize hand-asm reloc operands to raw immediates."""
    fixup_hasm.run()


def _gen_syms_ld(
    out: Annotated[Path, typer.Argument(help="linker script to write")],
    infiles: Annotated[list[Path], typer.Argument(help="symbol files (`name = 0xADDR;` lines)")],
) -> None:
    """PROVIDE() every known symbol at its original address (byte-stable link)."""
    gen_syms_ld.run(out, infiles)


def _normalize_binary(
    path: Annotated[Path, typer.Argument(help="binary file to normalize in place")],
    size: Annotated[int, typer.Argument(help="exact target length in bytes")],
) -> None:
    """Truncate / zero-pad a binary file to an exact length."""
    normalize_binary.run(path, size)


def _list_overlays(
    flt: Annotated[
        str, typer.Option("-f", "--filter", help="only names containing this substring")
    ] = "",
) -> None:
    """Print overlay names from config/overlays.json (config order)."""
    overlays.list_names(flt)


def _overlay_size(
    name: Annotated[str, typer.Argument(help="overlay name")],
) -> None:
    """Print one overlay's frozen file length from config/overlays.json."""
    overlays.size_of(name)


def _verify_overlays(
    compare_bytes: Annotated[
        bool, typer.Option("--compare-bytes", help="also byte-compare against disc/orig")
    ] = False,
) -> None:
    """Assert every built overlay matches its frozen SHA-1 (exit non-zero on mismatch)."""
    overlays.verify(compare_bytes)


app.command("progress")(_progress)
app.command("progress-delta")(_progress_delta)
app.command("sectionize")(_sectionize)
app.command("gen-overlay-yaml")(_gen_overlay_yaml)
app.command("gen-slots-ld")(_gen_slots_ld)
app.command("gen-syms-ld")(_gen_syms_ld)
app.command("fixup-hasm")(_fixup_hasm)
app.command("normalize-binary")(_normalize_binary)
app.command("list-overlays")(_list_overlays)
app.command("overlay-size")(_overlay_size)
app.command("verify-overlays")(_verify_overlays)


# ---- wad: nested group ------------------------------------------------------
wad_app = typer.Typer(help="Byte-identical WAD.WAD repack.", no_args_is_help=True)
app.add_typer(wad_app, name="wad")


@wad_app.command("unpack")
def wad_unpack() -> None:
    """WAD.WAD -> config/wad.json manifest + data parts."""
    wad.unpack()


@wad_app.command("pack")
def wad_pack(
    output: Annotated[
        Path | None, typer.Option("-o", "--output", help="write packed WAD here")
    ] = None,
    check: Annotated[
        bool, typer.Option("--check", help="assert byte-identical to disc/orig/WAD.WAD")
    ] = False,
    overlay_dir: Annotated[
        Path | None,
        typer.Option("--overlay-dir", help="dir to pull overlay parts from"),
    ] = None,
) -> None:
    """manifest + parts -> WAD.WAD."""
    wad.pack(output=output, check=check, overlay_dir=overlay_dir)


@wad_app.command("verify")
def wad_verify() -> None:
    """Round-trip proof: repack from current parts, assert SHA-1 == original."""
    wad.verify()


if __name__ == "__main__":
    app()
