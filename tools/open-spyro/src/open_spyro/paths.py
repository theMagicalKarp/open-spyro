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
