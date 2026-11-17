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
    c_layout,
    fixup_hasm,
    funcdiff,
    gen_overlay_yaml,
    gen_slots_ld,
    gen_syms_ld,
    handasm,
    native,
    neutral,
    normalize_binary,
    overlays,
    progress,
    progress_treemap,
    sectionize,
    wad,
)
from open_spyro.paths import repo_root

app = typer.Typer(
    help="Build tooling for the open-spyro byte-matching decompilation.",
    no_args_is_help=True,
    add_completion=False,
)


def _progress(
    require_full: Annotated[
        bool,
        typer.Option(
            "--require-full",
            help="exit non-zero unless every game function is matched from C (100.00%)",
        ),
    ] = False,
) -> None:
    """Regenerate the C-match progress report (PROGRESS.md + badge)."""
    if not progress.run() and require_full:
        raise typer.Exit(1)


def _progress_treemap(
    progress_json: Annotated[
        Path | None, typer.Option("--progress-json", help="progress.json to render")
    ] = None,
    output: Annotated[Path | None, typer.Option("-o", "--output", help="write SVG here")] = None,
) -> None:
    """Regenerate the progress svg treemap (docs/progress_map.svg)."""
    progress_treemap.run(progress_json, output)


def _sectionize() -> None:
    """Per-function `.section`/`.ifndef HAVE_C_*` wrapping of asm/text.s."""
    sectionize.run()


def _sectionize_overlays() -> None:
    """Same wrapping for every function/data-split overlay (has a .symbols.txt seed)."""
    sectionize.run_overlays()


def _gen_overlay_yaml() -> None:
    """Emit a splat config per overlay from config/overlays.json."""
    gen_overlay_yaml.run()


def _gen_slots_ld(
    overlay: Annotated[
        str,
        typer.Option("--overlay", help="emit slots for this overlay instead of the main EXE"),
    ] = "",
    check_objects: Annotated[
        bool,
        typer.Option(
            "--check-objects",
            help="first verify each compiled C object defines exactly its scanned functions",
        ),
    ] = False,
) -> None:
    """Emit a fixed-VMA text-slot fragment (main EXE, or one overlay with --overlay)."""
    if overlay:
        gen_slots_ld.run_overlay(overlay, check_objects)
    else:
        gen_slots_ld.run(check_objects)


def _c_functions(
    segment: Annotated[str, typer.Argument(help="'main' or an overlay name")] = "main",
) -> None:
    """Print the HAVE_C assembler flags for the functions a segment's C defines."""
    print(c_layout.havec_defsyms(repo_root(), segment))


def _diff(
    name: Annotated[str, typer.Argument(help="function name (main EXE or any split overlay)")],
    no_build: Annotated[
        bool, typer.Option("--no-build", help="reuse the existing build output as-is")
    ] = False,
    full: Annotated[
        bool, typer.Option("--full", help="print every instruction, not just diff hunks")
    ] = False,
    color: Annotated[
        str, typer.Option("--color", help="colorize output: auto (TTY only), always, or never")
    ] = "auto",
) -> None:
    """Rebuild, then byte-diff one function at its VMA against disc/orig (exit 1 on mismatch)."""
    funcdiff.run(name, build=not no_build, full=full, color=color)


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


def _neutral(
    ref: Annotated[str, typer.Option("--ref", help="git ref to compare against")] = "HEAD",
    host: Annotated[
        bool, typer.Option("--host", help="use the host clang cpp instead of Docker's")
    ] = False,
) -> None:
    """Check the PS1 preprocessor output of every game .c is unchanged vs a ref."""
    neutral.run(ref, host)


app.command("progress")(_progress)
app.command("progress-treemap")(_progress_treemap)
app.command("sectionize")(_sectionize)
app.command("sectionize-overlays")(_sectionize_overlays)
app.command("gen-overlay-yaml")(_gen_overlay_yaml)
app.command("gen-slots-ld")(_gen_slots_ld)
app.command("c-functions")(_c_functions)
app.command("gen-syms-ld")(_gen_syms_ld)
app.command("diff")(_diff)
app.command("fixup-hasm")(_fixup_hasm)
app.command("normalize-binary")(_normalize_binary)
app.command("list-overlays")(_list_overlays)
app.command("overlay-size")(_overlay_size)
app.command("verify-overlays")(_verify_overlays)
app.command("neutral")(_neutral)


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


# ---- native: nested group ---------------------------------------------------
native_app = typer.Typer(
    help="Native (wasm32 -> wasm2c -> arm64) port helpers.", no_args_is_help=True
)
app.add_typer(native_app, name="native")


@native_app.command("got-table")
def native_got_table(
    header: Annotated[Path, typer.Argument(help="wasm2c module header (<name>_w.h)")],
    module: Annotated[str, typer.Option("--module", help="wasm2c module name")],
    output: Annotated[Path, typer.Option("-o", "--output", help="C file to write")],
    syms: Annotated[
        list[Path] | None,
        typer.Option("--syms", help="extra `name = 0xADDR;` symbol files (overlays)"),
    ] = None,
) -> None:
    """Resolve a module's GOT.mem imports to game globals' PS1 addresses."""
    native.got_table(header, module, output, syms or [])


@native_app.command("handasm")
def native_handasm(
    out: Annotated[
        Path | None, typer.Option("-o", "--out", help="output directory (src/native/handasm)")
    ] = None,
) -> None:
    """Translate the hand-written asm to C for the native build."""
    handasm.run(out)


@native_app.command("gate")
def native_gate(
    savestate: Annotated[Path, typer.Argument(help="DuckStation savestate taken in a level")],
    binary: Annotated[Path, typer.Option("--binary", help="gate executable")] = Path(
        "build/native/gate"
    ),
) -> None:
    """ILP32 gate: native level load vs PS1 RAM from a savestate."""
    native.gate(savestate, binary)


if __name__ == "__main__":
    app()
