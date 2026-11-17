"""Prove a shared-code change is neutral for the PS1 build.

Runs the PS1 preprocessor (``$CPP $CPP_FLAGS`` from config/compile.sh, in the
matching Docker image) over every game C file twice: once on the tree at a git
ref (default ``HEAD``) and once on the working tree. Line markers and blank
lines are dropped, so moving code or adding comments and ``#ifdef __psyz``
blocks stays neutral. The rest is compared as one stream of C tokens, so
whitespace and line breaks (``char * p`` from a macro vs ``char *p``, a
reflowed statement) do not count. The same tokens means cc1 sees the same
input, so the bytes cannot change. Exits 1 on any difference.

Without Docker, ``--host`` uses the host's clang preprocessor with the same
flags. Both trees go through the same preprocessor, so the comparison still
holds, but only the Docker run is the PS1 preprocessor itself.
"""

from __future__ import annotations

import difflib
import re
import shutil
import subprocess
from pathlib import Path

from open_spyro.paths import repo_root

WORK = "build/neutral"
GAME_C_ROOTS = ("src/main", "src/overlays")
TREE_PATHS = ("src", "include")  # all a game .c can #include (CPP_FLAGS -Iinclude)


def _game_c(tree: Path) -> set[str]:
    out: set[str] = set()
    for r in GAME_C_ROOTS:
        out.update(str(p.relative_to(tree)) for p in (tree / r).rglob("*.c"))
    return out


# One preprocessing token: a string / char literal, a pp-number, an
# identifier, a multi-char punctuator (longest first), or any other character.
_TOKEN = re.compile(
    r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    r"|\.?[0-9](?:[eEpP][+-]|[.\w])*|[A-Za-z_]\w*"
    r"|\.\.\.|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|##|\S"
)


def _normalize(text: str) -> list[str]:
    """Non-empty lines without line markers, each re-spelled as its tokens
    joined by one space (kept per line for the diff report)."""
    lines = []
    for line in text.splitlines():
        if line.startswith("# ") or line.startswith("#line"):
            continue  # line markers
        tokens = _TOKEN.findall(line)
        if tokens:
            lines.append(" ".join(tokens))
    return lines


def _preprocess(root: Path, trees: dict[str, Path], files: dict[str, set[str]], host: bool) -> None:
    """Write build/neutral/out/<tree>/<file>.i for every file of every tree.

    One script, one container start: a list file per tree, looped over in bash.
    Paths are repo-relative so they mean the same inside the container."""
    work = root / WORK
    script = ["set -u", "source config/compile.sh"]
    if host:
        script.append('CPP="clang -E -x c"')
    for name, tree in trees.items():
        (work / f"{name}.list").write_text("\n".join(sorted(files[name])) + "\n")
        rel_tree = tree.relative_to(root)
        out = f"$OLDPWD/{WORK}/out/{name}"
        # Run from the tree root: CPP_FLAGS uses relative -Iinclude / -Iasm.
        script.append(
            f'(cd "{rel_tree}" && while read -r f; do mkdir -p "{out}/$(dirname "$f")"; '
            f'$CPP $CPP_FLAGS "$f" -o "{out}/$f.i" '
            f'|| echo "neutral: cpp failed: {name}/$f" >&2; done) < "{WORK}/{name}.list"'
        )
    (work / "run.sh").write_text("\n".join(script) + "\n")
    cmd = ["bash", f"{WORK}/run.sh"]
    if not host:
        cmd = ["bash", "tools/docker_env.sh", *cmd]
    subprocess.run(cmd, cwd=root, check=True)


def run(ref: str, host: bool) -> None:
    root = repo_root()
    work = root / WORK
    shutil.rmtree(work, ignore_errors=True)
    base = work / "base"
    base.mkdir(parents=True)
    archive = subprocess.run(
        ["git", "archive", ref, *TREE_PATHS], cwd=root, check=True, capture_output=True
    ).stdout
    subprocess.run(["tar", "-x", "-C", str(base)], input=archive, check=True)

    trees = {"base": base, "head": root}
    files = {name: _game_c(tree) for name, tree in trees.items()}
    _preprocess(root, trees, files, host)

    added = sorted(files["head"] - files["base"])
    removed = sorted(files["base"] - files["head"])
    changed: list[str] = []
    for f in sorted(files["head"] & files["base"]):
        a_path, b_path = work / "out/base" / f"{f}.i", work / "out/head" / f"{f}.i"
        if not (a_path.exists() and b_path.exists()):
            changed.append(f)  # cpp failed (reported above)
            continue
        a = _normalize(a_path.read_text(errors="replace"))
        b = _normalize(b_path.read_text(errors="replace"))
        if " ".join(a).split() != " ".join(b).split():
            changed.append(f)
            diff = difflib.unified_diff(a, b, f"{ref}:{f}", f"working:{f}", n=1, lineterm="")
            print("\n".join(list(diff)[:24]))

    common = len(files["head"] & files["base"])
    print(
        f"neutral: {common - len(changed)}/{common} files preprocess identically vs {ref}"
        + (" (host cpp)" if host else "")
    )
    for f in added:
        print(f"  new file (not comparable): {f}")
    for f in removed:
        print(f"  removed file: {f}")
    if changed or added or removed:
        raise SystemExit(
            f"neutral: NOT neutral ({len(changed)} changed, {len(added)} added, "
            f"{len(removed)} removed); confirm with `make check`"
        )
