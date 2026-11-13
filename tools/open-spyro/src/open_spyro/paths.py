"""Repo-root resolution shared by every command.

Each tool was historically anchored with ``Path(__file__).resolve().parent.parent``
(it lived at ``tools/<x>.py``). Now that the code lives inside the ``open_spyro``
package, the repo root is found by walking up from the current working directory
to the directory holding the repo sentinels (``Makefile`` + ``config/``). Every
invocation — the Makefile and CI (``docker run -w /work``) alike — runs from the
repo root, so this is stable.
"""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path


@lru_cache(maxsize=1)
def repo_root() -> Path:
    start = Path.cwd().resolve()
    for d in (start, *start.parents):
        if (d / "Makefile").is_file() and (d / "config").is_dir():
            return d
    raise SystemExit(
        "open-spyro: could not locate the repo root "
        "(no Makefile + config/ at or above the current directory)"
    )


# C sources live under one root per segment: ``src/main/<subsystem>/`` for the main
# EXE and ``src/overlays/<ovl>/`` for each overlay. Subdirectories and file names
# are grouping only; which file defines which function is ``open_spyro.c_layout``.
MAIN_SRC = "src/main"


def segment_src_root(repo: Path, segment: str) -> Path:
    """The C source root for a segment (``src/main`` or ``src/overlays/<seg>``)."""
    return repo / MAIN_SRC if segment == "main" else repo / "src/overlays" / segment


def c_sources(root: Path, suffix: str = ".c") -> list[Path]:
    """Every ``*<suffix>`` under ``root`` (recursive, sorted); [] if it's absent."""
    return sorted(root.rglob(f"*{suffix}")) if root.is_dir() else []
