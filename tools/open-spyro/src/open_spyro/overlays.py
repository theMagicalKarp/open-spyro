"""Queries over the committed overlay table (``config/overlays.json``).

Three build/verify steps used to parse this JSON inline (as shell heredocs); they
live here now as one shared reader plus thin commands:

* ``list_names``  — overlay names in config order (optional substring filter), used
  by ``tools/build_overlays.sh`` to drive its build loop.
* ``size_of``     — one overlay's frozen file length, fed to ``normalize-binary``.
* ``verify``      — assert every built ``build/overlays/<n>.ovl`` matches its frozen
  SHA-1 (and, with ``--compare-bytes``, the original disc bytes). Used by
  ``tools/verify_main.sh``; self-contained — prints its own summary and exits
  non-zero on any mismatch.
"""

from __future__ import annotations

import hashlib
import json
import sys

from open_spyro.paths import repo_root


def load() -> list[dict]:
    """The ``overlays`` array from config/overlays.json, in file order."""
    data = json.loads((repo_root() / "config/overlays.json").read_text())
    return data["overlays"]


def list_names(flt: str = "") -> None:
    for r in load():
        if not flt or flt in r["name"]:
            print(r["name"])


def size_of(name: str) -> None:
    print(next(r["size"] for r in load() if r["name"] == name))


def verify(compare_bytes: bool) -> None:
    repo = repo_root()
    ok = bad = 0
    for r in load():
        n, want = r["name"], r["sha1"]
        built = repo / f"build/overlays/{n}.ovl"
        if not built.exists():
            print(f"  MISSING build/overlays/{n}.ovl — run 'make build'", file=sys.stderr)
            bad += 1
            continue
        b = built.read_bytes()
        got = hashlib.sha1(b).hexdigest()
        byte_ok = True
        if compare_bytes:
            byte_ok = b == (repo / f"disc/orig/overlays/{n}.ovl").read_bytes()
        if got == want and byte_ok:
            ok += 1
        else:
            print(f"  DIFFER {n}.ovl — built {got}, want {want}", file=sys.stderr)
            bad += 1

    if bad == 0:
        print(f"verify: MATCH  all {ok} overlays byte-identical")
    else:
        print(f"verify: DIFFER {bad} overlay(s) mismatched ({ok} matched)", file=sys.stderr)
        raise SystemExit(1)
