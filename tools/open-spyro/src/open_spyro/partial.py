"""Partial-match scoring for parked ``.c.wip`` attempts.

The headline progress metric is all-or-nothing per function: a function counts
its full size only once its rebuilt bytes are byte-identical to the original
(``progress.py``). A parked ``src/c/<Name>.c.wip`` that already reproduces most of
a function contributes nothing. This module gives that in-flight work partial
credit, at instruction granularity, so ``make progress`` reflects it.

Why it can't just diff the rebuilt EXE
--------------------------------------
``.c.wip`` files are never compiled into the build (``build_main.sh`` globs only
``src/c/*.c``); the EXE links the original asm at that VMA. So there are no
candidate bytes to slice — a ``funcdiff``-style compare would falsely report a
100% match. Partial scoring therefore compiles the wip C *standalone* and diffs
the resulting object against a target object assembled from the function's
original asm block (extracted from ``asm/text.s`` / the overlay ``text.s``).

Because both sides are relocatable objects (calls/global refs unresolved), the
compare must be relocation-aware — a raw byte compare would miscount every
``jal``/``%gp_rel``. We ``objdump -drz`` both objects and count instructions whose
raw word AND relocation (type + symbol) are identical: those are exactly the ones
that link to identical bytes.

Split of responsibilities (mirrors ``funcdiff``'s single-docker-invocation rule):
  * ``run``                 enumerates wip functions, extracts each target asm
                            block, then does ALL the toolchain work in ONE shell
                            invocation (assemble + compile + score) and writes
                            ``build/partial.json``. That shell runs inside the
                            matching image — shelled in via ``tools/docker_env.sh``
                            from the host, or directly when already in-container
                            (CI, where a nested ``docker run`` would fail). Nothing
                            binary crosses back to the host — only the scores JSON,
                            since reading build output through the mount right after
                            a docker build can serve stale bytes.
  * ``score_from_manifest`` (in-container) objdump-diffs each target/candidate pair.

``build/partial.json`` is a gitignored build artifact consumed by ``progress.py``;
when it is absent (a host-only stdlib ``progress`` run) the metric degrades to
exact-only.
"""

from __future__ import annotations

import hashlib
import json
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from open_spyro.funcdiff import _locate
from open_spyro.paths import repo_root

# Preamble every extracted target .s needs to assemble on its own — the same
# directives asm/text.s opens with (macros + noat/noreorder), minus the per-
# function `.ifndef HAVE_C_*` guard (we WANT the asm included).
_TARGET_PREAMBLE = '.include "macro.inc"\n.set noat\n.set noreorder\n.section .text, "ax"\n'

# The matching toolchain lives here in the Docker image; its presence is a
# reliable "am I already inside the matching container?" signal. When true the
# assemble/compile/score step runs directly (CI is already in-container — a
# nested `docker run` would be docker-in-docker); otherwise we shell into it.
_CC1 = Path("/opt/gcc2.7.2/cc1")


def _in_container() -> bool:
    return _CC1.exists()


def _asm_path(repo: Path, segment: str) -> Path:
    """The text.s holding a function's original asm block for a segment."""
    if segment == "main":
        return repo / "asm/text.s"
    return repo / "asm/overlays" / segment / "text.s"


def _extract_block(asm_text: str, name: str) -> str | None:
    """Slice a function's `.section .text.<name>, "ax"` .. `.endif` block.

    Returns the block verbatim (still wrapped in its `.ifndef HAVE_C_<name>`
    guard — harmless, since we never `--defsym` it when assembling standalone),
    or None if the section is not present.
    """
    lines = asm_text.splitlines()
    start = None
    header = f".section .text.{name},"
    for i, line in enumerate(lines):
        if line.startswith(header):
            start = i
            break
    if start is None:
        return None
    for j in range(start + 1, len(lines)):
        if lines[j].strip() == ".endif":
            return "\n".join(lines[start : j + 1]) + "\n"
    # Not sectionized (no guard) — take through the endlabel instead.
    for j in range(start + 1, len(lines)):
        if lines[j].startswith(f"endlabel {name}"):
            return "\n".join(lines[start : j + 1]) + "\n"
    return None


def _wip_sources(repo: Path) -> list[tuple[str, Path]]:
    """(name, src_path) for every parked ``.c.wip`` (main + overlays)."""
    out: list[tuple[str, Path]] = []
    for p in sorted((repo / "src/c").glob("*.c.wip")):
        out.append((p.name.removesuffix(".c.wip"), p))
    ovl_root = repo / "src/overlays"
    if ovl_root.is_dir():
        for p in sorted(ovl_root.glob("*/*.c.wip")):
            out.append((p.name.removesuffix(".c.wip"), p))
    return out


def _digest(src: Path, block: str) -> str:
    """Cache key: wip source contents + the target asm block it is scored against."""
    h = hashlib.sha256()
    h.update(src.read_bytes())
    h.update(block.encode())
    return h.hexdigest()


# --- container side ----------------------------------------------------------

# objdump -drz output: instruction rows are `   <off>:\t<word> \t<mnem> <ops>`;
# relocations print on their own row `\t\t\t<off>: R_MIPS_xxx\t<symbol>` right
# after the instruction they patch.
_INSN = re.compile(r"^\s*[0-9a-f]+:\s+([0-9a-f]{8})\s+(\S+)")
_RELOC = re.compile(r"^\s*[0-9a-f]+:\s+(R_MIPS_\S+)\s+(\S+)")


@dataclass
class _Insn:
    word: str  # the 4-byte instruction, hex (reloc-patched fields read as 0)
    mnemonic: str
    reloc: tuple[str, str] | None  # (type, normalized symbol), or None


def _norm_sym(sym: str) -> str:
    """Collapse intra-`.text` self-references to one token.

    A local `j .Llabel` becomes an R_MIPS_26 reloc against the enclosing `.text`
    section — named `.text.<func>` in the assembled target but plain `.text` in
    the compiled candidate. The jump's byte target already lives in the word, so
    both are the same local jump; normalize the differing section names to match.
    """
    return "<LOCAL>" if sym.startswith(".text") else sym.split("+", 1)[0]


def _disasm(obj: str) -> list[_Insn]:
    """`objdump -drz` an object into a per-instruction sequence with its reloc."""
    out = subprocess.run(
        ["mips-linux-gnu-objdump", "-drz", "-m", "mips:3000", obj],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    insns: list[_Insn] = []
    for line in out.splitlines():
        m = _INSN.match(line)
        if m:
            insns.append(_Insn(word=m.group(1), mnemonic=m.group(2), reloc=None))
            continue
        r = _RELOC.match(line)
        if r and insns:
            insns[-1].reloc = (r.group(1), _norm_sym(r.group(2)))
    return insns


def _matched_instructions(target_o: str, cand_o: str) -> int:
    """Count instructions that link to identical bytes at their own address.

    Byte-matching is POSITIONAL: the candidate is scored into the function's
    fixed-size slot, so the instruction at offset N must equal the original's at
    offset N — a match found at a shifted position is different bytes at a
    different address and does not count. We therefore compare index-aligned (like
    `funcdiff`, which zips the two linked byte streams), so `partial` and
    `open-spyro diff` report the same match count for the same function.

    Two aligned instructions match when their raw words are equal AND they carry
    the same relocation (type + symbol): word equality covers registers,
    immediates, and PC-relative/local-jump targets (all encoded in the word, with
    reloc-patched fields zero pre-link); the reloc check stops a `lui`/`jal`
    against symbol A from matching one against symbol B despite an identical word.

    A self-contained objdump parse (not decomp-permuter's Scorer) is used because
    the Scorer's reloc rewriter aborts on the intra-section R_MIPS_26 relocs that
    local `j` jumps produce here.
    """
    tgt = _disasm(target_o)
    cand = _disasm(cand_o)
    matched = 0
    for i, t in enumerate(tgt):
        if i < len(cand) and cand[i].word == t.word and cand[i].reloc == t.reloc:
            matched += 1
    return matched


def score_from_manifest(manifest_path: Path, out_path: Path) -> None:
    """In-container: objdump-diff each pair listed in the manifest.

    Manifest entries carry {name, size, target_o, cand_o}. A missing/failed
    candidate object (compile failure) scores 0. matched_bytes is clamped below
    the full size so a fully-reproducing wip never reads as a real (built,
    verified) match.
    """
    entries = json.loads(manifest_path.read_text())["functions"]
    scores: dict[str, int] = {}
    for e in entries:
        cand_o = Path(e["cand_o"])
        target_o = Path(e["target_o"])
        if not cand_o.is_file() or not target_o.is_file():
            scores[e["name"]] = 0
            continue
        try:
            n = _matched_instructions(str(target_o), str(cand_o))
        except Exception as exc:  # objdump/arch hiccup -> no credit, keep going
            print(f"partial-score: {e['name']}: {exc}", file=sys.stderr)
            scores[e["name"]] = 0
            continue
        scores[e["name"]] = min(n * 4, max(0, e["size"] - 4))
    out_path.write_text(json.dumps(scores, indent=2) + "\n")


# --- host side ---------------------------------------------------------------


def _container_script(entries: list[dict]) -> str:
    """Assemble each target .s + compile each wip .c, then run the scorer.

    Everything runs in one docker invocation. Assemble/compile failures are
    tolerated per-function (the scorer treats a missing object as 0), so one bad
    wip never aborts the batch.
    """
    lines = [
        "set -uo pipefail",
        "source config/compile.sh",  # imports AS / AS_FLAGS / compile.sh vars
        "export PYTHONPATH=tools/open-spyro/src${PYTHONPATH:+:$PYTHONPATH}",
    ]
    for e in entries:
        tgt_s = shlex.quote(e["target_s"])
        tgt_o = shlex.quote(e["target_o"])
        cand_o = shlex.quote(e["cand_o"])
        src_c = shlex.quote(e["src_c"])
        mode = e["mode"]
        # rm first so a failed assemble/compile leaves NO object (scored 0) rather
        # than a stale one from a prior run (which would score bogus bytes).
        lines.append(f"rm -f {tgt_o} {cand_o}")
        lines.append(f'"$AS" $AS_FLAGS -o {tgt_o} {tgt_s} 2>/dev/null || true')
        lines.append(f"config/compile.sh {mode} {src_c} {cand_o} 2>/dev/null || true")
    lines.append(
        "open-spyro partial-score --manifest build/partial/manifest.json "
        "--out build/partial/scores.json"
    )
    return "\n".join(lines)


def run() -> None:
    repo = repo_root()
    part = repo / "build/partial"
    targets_dir = part / "targets"
    targets_dir.mkdir(parents=True, exist_ok=True)

    cache_path = repo / "build/partial.json"
    cache: dict[str, dict] = {}
    if cache_path.is_file():
        try:
            cache = json.loads(cache_path.read_text())
        except json.JSONDecodeError:
            cache = {}

    asm_cache: dict[str, str] = {}
    entries: list[dict] = []  # to (re)score this run
    result: dict[str, dict] = {}  # final, incl. reused cache entries

    for name, src in _wip_sources(repo):
        t = _locate(repo, name)
        if t is None:
            continue  # not in any layout (shouldn't happen for a real wip)
        asm_text = asm_cache.get(t.segment)
        if asm_text is None:
            ap = _asm_path(repo, t.segment)
            if not ap.is_file():
                continue
            asm_text = asm_cache[t.segment] = ap.read_text()
        block = _extract_block(asm_text, name)
        if block is None:
            continue

        digest = _digest(src, block)
        prev = cache.get(name)
        if prev and prev.get("digest") == digest:
            result[name] = prev  # unchanged since last run — reuse the score
            continue

        target_s = targets_dir / f"{name}.s"
        target_s.write_text(_TARGET_PREAMBLE + block)
        rel_src = src.relative_to(repo)
        entries.append(
            {
                "name": name,
                "segment": t.segment,
                "mode": "main" if t.segment == "main" else "ovl",
                "size": t.size,
                "digest": digest,
                "src_c": str(rel_src),
                "target_s": f"build/partial/targets/{name}.s",
                "target_o": f"build/partial/targets/{name}.o",
                "cand_o": f"build/partial/targets/{name}.cand.o",
            }
        )

    if entries:
        (part / "manifest.json").write_text(json.dumps({"functions": entries}, indent=2) + "\n")
        # Same assemble+compile+score script either way; only the wrapper differs —
        # run it directly when already inside the matching image (CI), else shell in.
        script = _container_script(entries)
        cmd = (
            ["bash", "-c", script]
            if _in_container()
            else ["bash", "tools/docker_env.sh", "bash", "-c", script]
        )
        proc = subprocess.run(cmd, cwd=repo)
        scores_path = part / "scores.json"
        if proc.returncode != 0 or not scores_path.is_file():
            print("partial: scoring step failed; keeping prior scores", file=sys.stderr)
            scores = {}
        else:
            scores = json.loads(scores_path.read_text())
        for e in entries:
            result[e["name"]] = {
                "segment": e["segment"],
                "matched_bytes": int(scores.get(e["name"], 0)),
                "total_bytes": e["size"],
                "digest": e["digest"],
            }

    cache_path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    total = sum(r["matched_bytes"] for r in result.values())
    print(
        f"partial: scored {len(result)} wip function(s) "
        f"({len(entries)} rebuilt, {len(result) - len(entries)} cached); "
        f"{total:,} partial bytes -> build/partial.json"
    )
