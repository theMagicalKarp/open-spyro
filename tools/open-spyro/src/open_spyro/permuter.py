"""Stage + launch decomp-permuter against a parked ``.c.wip`` attempt.

``open-spyro permuter <Func>`` automates the register-allocation brute-force loop for
a not-yet-matching function. It:

  1. locates the function (``funcdiff._locate``) and its parked ``src/c/<Func>.c.wip``
     (or ``src/overlays/<seg>/<Func>.c.wip``) source,
  2. slices the function's original asm out of ``asm/text.s`` (``partial._extract_block``)
     and trims it to the bare ``glabel <Func> … endlabel <Func>`` block import.py wants,
  3. runs ``tools/decomp-permuter/import.py`` (with the checked-in
     ``config/permuter_settings.toml`` overriding the tool's N64 defaults for this PS1
     r3000/gcc-2.7.2 toolchain) to stage a ``nonmatchings/<Func>/`` scratch dir under the
     gitignored ``build/permuter/``, then
  4. launches ``tools/decomp-permuter/permuter.py`` on it, then
  5. if the permuter finds a byte-perfect (score-0) result, writes it to ``src/c/<Func>.c``
     (or the overlay path), verifies with ``open-spyro diff``, and on MATCH drops the
     ``.c.wip`` — turning a parked attempt into a real match (see ``_apply_match``).

Note the written C is decomp-permuter's *pruned* form (stub externs, no ``globals.h``, no
doc comments); it byte-matches and builds, but you'll usually want to re-clothe it in the
repo's ``#include "globals.h"`` convention afterward.

Why wrap import.py rather than hand-roll ``base.c``: the permuter mutates ``base.c`` as a
pycparser AST, so it must be a fully preprocessed + pruned translation unit — exactly what
import.py's ``import_c_file``/``prune_source`` produce.

Everything toolchain-touching happens inside ONE container invocation (cc1/maspsx/cpp live
only in the matching image), shelled in via ``tools/docker_env.sh`` from the host or run
directly when already in-container — the same host/container split as ``partial`` and
``funcdiff``. Stdio is inherited (not captured) so the permuter's live TUI works.

Known limits (see the module's plan notes): the permuter's own scorer aborts on the
intra-``.text`` R_MIPS_26 relocs that local ``j .Llabel`` jumps produce here, so functions
with switch/jump-tables may not permute cleanly; regalloc-tie (branch-only) candidates are
the sweet spot. If import.py's pycparser parse of the expanded source fails, retry with
``--no-prune``.
"""

from __future__ import annotations

import shlex
import subprocess
import sys
from pathlib import Path

from open_spyro import funcdiff
from open_spyro.funcdiff import _locate
from open_spyro.partial import _asm_path, _extract_block, _in_container
from open_spyro.paths import repo_root

# decomp-permuter clone (gitignored; provisioned by tools/setup.sh) and its two entry points.
_IMPORT_PY = "tools/decomp-permuter/import.py"
_PERMUTER_PY = "tools/decomp-permuter/permuter.py"
_SETTINGS = "config/permuter_settings.toml"


def _src_dir(repo: Path, segment: str) -> Path:
    """The C source directory for a segment (``src/c`` or ``src/overlays/<seg>``)."""
    return repo / "src/c" if segment == "main" else repo / "src/overlays" / segment


def _wip_source(repo: Path, name: str, segment: str) -> Path | None:
    """The parked ``.c.wip`` for a function, or None if it isn't parked."""
    p = _src_dir(repo, segment) / f"{name}.c.wip"
    return p if p.is_file() else None


def _winning_source(repo: Path, name: str) -> Path | None:
    """The permuter's byte-perfect result, if it produced one.

    decomp-permuter writes each accepted candidate to ``output-<score>-<n>/source.c``;
    a score of 0 is a byte-perfect match. Returns the first such file, or None.
    """
    base = repo / "build/permuter/nonmatchings" / name
    outs = sorted(base.glob("output-0-*/source.c"))
    return outs[0] if outs else None


def _trim_to_glabel(block: str, name: str) -> str | None:
    """Reduce an ``_extract_block`` slice to the bare ``glabel … endlabel`` body.

    import.py's ``prune_asm`` only picks up the function name from a ``glabel`` line while
    the section is exactly ``.text``; the extracted block opens with ``.section
    .text.<name>``, which changes the tracked section and hides the ``glabel``. Stripping
    the wrapping directives leaves the block in the default ``.text`` the prelude sets up.
    Returns None if the ``glabel``/``endlabel`` markers aren't both present.
    """
    lines = block.splitlines()
    start = end = None
    for i, ln in enumerate(lines):
        s = ln.strip()
        if start is None and s.startswith(f"glabel {name}"):
            start = i
        elif s.startswith(f"endlabel {name}"):
            end = i
            break
    if start is None or end is None:
        return None
    return "\n".join(lines[start : end + 1]) + "\n"


_ASM_HEREDOC = "__SPYRO_TARGET_ASM_EOF__"


def _container_script(
    *,
    root_rel_wip: str,
    name: str,
    mode: str,
    trimmed_asm: str,
    stage_only: bool,
    threads: int,
    keep: bool,
    no_prune: bool,
    stop_on_zero: bool,
) -> str:
    """One shell script: write the target asm, import.py stages the dir, permuter.py runs.

    All filesystem writes happen HERE (in the container, as root) rather than host-side:
    ``build/`` is created and owned by root by prior Docker builds, so the host user can't
    write into it. The trimmed target asm is streamed in via a quoted heredoc.

    Anchors ``$ROOT`` at the repo root (both entry paths start there — docker ``-w /work``,
    or ``cwd=repo`` in-container) then ``cd``s into ``build/permuter`` so import.py's
    hardcoded ``nonmatchings/`` lands inside the gitignored ``build/`` tree.
    """
    work = f"build/permuter/{name}"
    import_cmd = [
        "python3",
        f'"$ROOT/{_IMPORT_PY}"',
        f'"$ROOT/{root_rel_wip}"',
        f'"$ROOT/{work}/target_input.s"',
        "--settings",
        f'"$ROOT/{_SETTINGS}"',
        "--preserve-macros",
        "''",
        "--keep",  # we manage the scratch dir ourselves; keep it on import error to inspect
    ]
    if no_prune:
        import_cmd.append("--no-prune")

    q_work = shlex.quote(work)
    q_nonmatch = shlex.quote(f"build/permuter/nonmatchings/{name}")
    lines = [
        "set -euo pipefail",
        f"export SPYRO_PERM_MODE={shlex.quote(mode)}",
        'ROOT="$PWD"',
        f"mkdir -p {q_work}",
        # stream the trimmed glabel..endlabel asm into the staging input (quoted heredoc
        # => no shell expansion of the asm body)
        f"cat > {q_work}/target_input.s <<'{_ASM_HEREDOC}'",
        trimmed_asm.rstrip("\n"),
        _ASM_HEREDOC,
    ]
    if not keep:
        lines.append(f"rm -rf {q_nonmatch}")
    lines.append("cd build/permuter")
    lines.append(" ".join(import_cmd))

    if not stage_only:
        run_cmd = ["python3", f'"$ROOT/{_PERMUTER_PY}"', "-j", str(threads)]
        if stop_on_zero:
            run_cmd.append("--stop-on-zero")
        run_cmd.append(f"nonmatchings/{shlex.quote(name)}")
        lines.append(" ".join(run_cmd))
    else:
        d = f"build/permuter/nonmatchings/{name}"
        lines += [
            f'echo "permuter: staged {d}"',
            f'ls -1 "$ROOT/{d}"',
            'echo "permuter: launch with ->"',
            f'echo "  (cd build/permuter && python3 \\"$ROOT/{_PERMUTER_PY}\\" '
            f'-j {threads} --stop-on-zero nonmatchings/{name})"',
        ]
    return "\n".join(lines)


def run(
    name: str,
    *,
    stage_only: bool = False,
    threads: int = 4,
    keep: bool = False,
    no_prune: bool = False,
    stop_on_zero: bool = True,
) -> None:
    repo = repo_root()

    t = _locate(repo, name)
    if t is None:
        import difflib

        from open_spyro.funcdiff import _all_names

        close = difflib.get_close_matches(name, _all_names(repo), n=5, cutoff=0.5)
        hint = f" (close: {', '.join(close)})" if close else ""
        print(f"permuter: unknown function '{name}'{hint}", file=sys.stderr)
        raise SystemExit(2)

    mode = "main" if t.segment == "main" else "ovl"

    wip = _wip_source(repo, name, t.segment)
    if wip is None:
        seg = "src/c" if t.segment == "main" else f"src/overlays/{t.segment}"
        print(
            f"permuter: no parked attempt at {seg}/{name}.c.wip — this command targets "
            "parked `.c.wip` files (start one with `open-spyro diff`/m2c first).",
            file=sys.stderr,
        )
        raise SystemExit(2)

    ap = _asm_path(repo, t.segment)
    if not ap.is_file():
        print(f"permuter: missing {ap.relative_to(repo)}", file=sys.stderr)
        raise SystemExit(2)
    block = _extract_block(ap.read_text(), name)
    if block is None:
        print(f"permuter: no asm block for {name} in {ap.relative_to(repo)}", file=sys.stderr)
        raise SystemExit(2)
    trimmed = _trim_to_glabel(block, name)
    if trimmed is None:
        print(
            f"permuter: could not find glabel/endlabel markers for {name} in "
            f"{ap.relative_to(repo)}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    script = _container_script(
        root_rel_wip=str(wip.relative_to(repo)),
        name=name,
        mode=mode,
        trimmed_asm=trimmed,
        stage_only=stage_only,
        threads=threads,
        keep=keep,
        no_prune=no_prune,
        stop_on_zero=stop_on_zero,
    )
    cmd = (
        ["bash", "-c", script]
        if _in_container()
        else ["bash", "tools/docker_env.sh", "bash", "-c", script]
    )
    proc = subprocess.run(cmd, cwd=repo)

    if stage_only:
        raise SystemExit(proc.returncode)

    _apply_match(repo, name, t.segment, wip)
    raise SystemExit(proc.returncode)


def _apply_match(repo: Path, name: str, segment: str, wip: Path) -> None:
    """On a byte-perfect permuter result, write the C file and verify it, else no-op.

    Writes host-side (the winning ``source.c`` under root-owned ``build/`` is world-
    readable) so the new ``src/`` file is owned by the user. Verifies with ``open-spyro
    diff`` — the permuter scores an isolated object, so this proves the win still holds
    once linked at the function's real VMA:
      * MATCH  -> drop the ``.c.wip`` (the attempt is now a real match).
      * DIFFER -> revert the written ``.c`` and keep the ``.c.wip`` (isolated match didn't
        reproduce in the full build — rare).
      * can't verify (e.g. no ``disc/orig``) -> leave the ``.c`` written AND the ``.c.wip``,
        and tell the user to verify by hand.
    """
    win = _winning_source(repo, name)
    if win is None:
        print("permuter: no byte-perfect match produced; src/ left unchanged.")
        return

    dest = _src_dir(repo, segment) / f"{name}.c"
    print(f"permuter: match found -> writing {dest.relative_to(repo)} and verifying...")
    dest.write_text(win.read_text())

    try:
        funcdiff.run(name, build=True, color="auto")  # prints MATCH; raises on differ/error
    except SystemExit as exc:
        if exc.code == 1:  # DIFFER: roll back, keep the parked attempt
            dest.unlink()
            print(
                f"permuter: verification DIFFERs — reverted {dest.relative_to(repo)}, kept "
                f"{wip.relative_to(repo)}. The isolated permuter match didn't reproduce in "
                "the linked build.",
                file=sys.stderr,
            )
            raise SystemExit(1) from None
        # exit 2: couldn't run the diff at all — leave both files, don't guess.
        print(
            f"permuter: wrote {dest.relative_to(repo)} but could not verify it "
            f"(diff exit {exc.code}); left {wip.relative_to(repo)} in place. Verify with "
            f"`open-spyro diff {name}`.",
            file=sys.stderr,
        )
        raise SystemExit(2) from None

    wip.unlink()
    print(f"permuter: verified MATCH — removed {wip.relative_to(repo)}.")
