"""Byte-identical WAD.WAD repack tool.

WAD.WAD is a trivially regular container:
  * 0x800-byte table of contents: 256 little-endian (offset, size) pairs. Null
    pairs (0,0) are unused slots, NOT a terminator.
  * Every populated slot's data follows, sector-aligned (0x800), laid out in slot
    order with zero gaps/overlaps and exact coverage (the last blob ends exactly at
    end-of-file). Verified against disc/orig/WAD.WAD.

The semantic slot layout is transcribed:
    0  universal_logo
    1  wad1_titlescreen_gfx
    2  <titlescreen overlay>
    3  cutscene_titlescreen   4 cutscene_intro
    5  cutscene_outro1        6 cutscene_outro2
    7  gameover_skybox        8 pete
    9 + 2*i   <level i overlay>     (i = 0..35)
   10 + 2*i   level i data
   81  <credits overlay>
   82..91  credits1_data_0..9
   92..101 credits2_data_0..9
The 36th level entry (slots 79,80) is empty; slots 102..255 are unused.

The 37 overlay slots are identified by matching their TOC offset against the frozen
overlay table (config/overlays.json). Overlay parts therefore reference the
already-extracted disc/orig/overlays/<name>.ovl rather than being re-extracted;
everything else is a data blob written to disc/orig/wad/.

Subcommands:
  unpack  WAD.WAD -> config/wad.json (committed manifest) + data parts
  pack    manifest + parts -> WAD.WAD  (--check diffs against the original)
  verify  round-trip: pack from current parts, assert SHA-1 == original
"""

from __future__ import annotations

import hashlib
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, NoReturn

from open_spyro.paths import repo_root

SECTOR = 0x800
TOC_SLOTS = 256
TOC_SIZE = TOC_SLOTS * 8  # 0x800
WAD_SHA1 = "ab68001377ea93fb309a5c2a35e772c9d1f57e0b"


@dataclass(frozen=True)
class _Layout:
    repo: Path
    orig: Path  # part paths in the manifest are relative to here
    wad: Path
    overlays_json: Path
    manifest: Path
    parts_dir: Path
    overlay_dir: Path


def _layout() -> _Layout:
    repo = repo_root()
    orig = repo / "disc/orig"
    return _Layout(
        repo=repo,
        orig=orig,
        wad=orig / "WAD.WAD",
        overlays_json=repo / "config/overlays.json",
        manifest=repo / "config/wad.json",
        parts_dir=orig / "wad",
        overlay_dir=orig / "overlays",
    )


def _die(msg: str) -> NoReturn:
    print(f"wadtool: ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def _sha1(b: bytes) -> str:
    return hashlib.sha1(b).hexdigest()


def _parse_toc(wad: bytes) -> list[tuple[int, int, int]]:
    """0x800 TOC -> list of (slot, offset, size) for populated slots, in slot order."""
    out = []
    for slot in range(TOC_SLOTS):
        off, size = struct.unpack_from("<II", wad, slot * 8)
        if off == 0 and size == 0:
            continue
        out.append((slot, off, size))
    return out


def _slot_name_layout() -> dict[int, str]:
    """Hard-coded wad.h semantic names for the (non-overlay) data slots."""
    names = {
        0: "universal_logo",
        1: "wad1_titlescreen_gfx",
        3: "cutscene_titlescreen",
        4: "cutscene_intro",
        5: "cutscene_outro1",
        6: "cutscene_outro2",
        7: "gameover_skybox",
        8: "pete",
    }
    for n in range(10):
        names[82 + n] = f"credits1_data_{n}"
        names[92 + n] = f"credits2_data_{n}"
    return names


def _overlay_slot_kind(slot: int) -> bool:
    """True if this TOC slot is expected (per wad.h) to hold a code overlay."""
    if slot in (2, 81):  # titlescreen, credits
        return True
    return 9 <= slot <= 78 and (slot - 9) % 2 == 0  # level i overlay = 9 + 2*i


def _build_manifest(lay: _Layout, wad: bytes) -> dict[str, Any]:
    if _sha1(wad) != WAD_SHA1:
        _die(f"WAD.WAD SHA-1 mismatch (got {_sha1(wad)}, want {WAD_SHA1})")

    ovl = json.loads(lay.overlays_json.read_text())
    ovl_by_off = {o["vrom"]: o for o in ovl["overlays"]}
    data_names = _slot_name_layout()

    toc = _parse_toc(wad)
    # Coverage check: contiguous from TOC end to EOF, no gaps/overlaps.
    cursor = TOC_SIZE
    for slot, off, size in sorted(toc, key=lambda e: e[1]):
        if off != cursor:
            _die(
                f"slot {slot}: offset 0x{off:x} != expected 0x{cursor:x} "
                f"(gap/overlap — layout assumption broken)"
            )
        if off % SECTOR or size % SECTOR:
            _die(f"slot {slot}: offset/size not sector-aligned")
        cursor = off + size
    if cursor != len(wad):
        _die(f"coverage 0x{cursor:x} != file size 0x{len(wad):x}")

    slots = []
    for slot, off, size in toc:
        blob = wad[off : off + size]
        is_ovl = _overlay_slot_kind(slot)
        if is_ovl:
            entry = ovl_by_off.get(off)
            if entry is None:
                _die(f"slot {slot} expected an overlay at 0x{off:x} but none in config")
            if entry["size"] != size:
                _die(f"slot {slot} overlay {entry['name']}: size mismatch")
            name = entry["name"]
            part = f"overlays/{entry['ovl_file']}"
        else:
            if off in ovl_by_off:
                _die(f"slot {slot} not flagged overlay but matches overlay offset")
            name = data_names.get(slot, f"slot_{slot}")
            part = f"wad/{name}.bin"
        slots.append(
            {
                "slot": slot,
                "name": name,
                "overlay": is_ovl,
                "offset": off,
                "size": size,
                "sha1": _sha1(blob),
                "part": part,
            }
        )
    return {
        "_comment": "Authoritative WAD.WAD container manifest. Generated "
        "by `open-spyro wad unpack`. Slots in TOC order; unlisted slots "
        "(of 256) are null. Overlay parts reference disc/orig/overlays/; "
        "data parts live in disc/orig/wad/.",
        "wad_sha1": WAD_SHA1,
        "wad_size": len(wad),
        "sector": SECTOR,
        "toc_slots": TOC_SLOTS,
        "slots": slots,
    }


def _pack_bytes(lay: _Layout, manifest: dict[str, Any], overlay_dir: Path | None = None) -> bytes:
    """Rebuild the WAD bytes from manifest + on-disk parts.

    overlay_dir, if given, supplies rebuilt overlays (e.g. build/overlays); any
    overlay missing there falls back to the original disc/orig/overlays copy. So a
    partial dir of just the overlays built from our sources is enough.
    """
    buf = bytearray(manifest["wad_size"])
    for s in manifest["slots"]:
        slot, off, size = s["slot"], s["offset"], s["size"]
        part = lay.orig / s["part"]
        if s["overlay"] and overlay_dir is not None:
            rebuilt = Path(overlay_dir) / Path(s["part"]).name
            if rebuilt.exists():
                part = rebuilt
        if not part.exists():
            _die(f"part missing: {part}")
        data = part.read_bytes()
        if len(data) != size:
            _die(f"slot {slot} ({s['name']}): part size {len(data)} != {size}")
        buf[off : off + size] = data
        struct.pack_into("<II", buf, slot * 8, off, size)
    return bytes(buf)


def unpack() -> None:
    lay = _layout()
    wad = lay.wad.read_bytes()
    manifest = _build_manifest(lay, wad)
    lay.parts_dir.mkdir(parents=True, exist_ok=True)
    n_data = 0
    for s in manifest["slots"]:
        if s["overlay"]:
            # Overlay bytes are already extracted; just sanity-check.
            p = lay.orig / s["part"]
            if not p.exists():
                _die(
                    f"overlay part missing: {s['part']} — "
                    f"run 'make extract' or check config/overlays.json"
                )
            if _sha1(p.read_bytes()) != s["sha1"]:
                _die(f"overlay part {s['part']} SHA-1 disagrees with WAD blob")
            continue
        (lay.orig / s["part"]).write_bytes(wad[s["offset"] : s["offset"] + s["size"]])
        n_data += 1
    lay.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        f"wadtool: unpacked {len(manifest['slots'])} slots "
        f"({len(manifest['slots']) - n_data} overlays + {n_data} data parts)"
    )
    print(f"wadtool: manifest -> {lay.manifest.relative_to(lay.repo)}")
    print(f"wadtool: data parts -> {lay.parts_dir.relative_to(lay.repo)}/")


def pack(output: Path | None = None, check: bool = False, overlay_dir: Path | None = None) -> None:
    lay = _layout()
    if not lay.manifest.exists():
        _die(f"{lay.manifest.relative_to(lay.repo)} missing — run 'wad unpack' first")
    manifest = json.loads(lay.manifest.read_text())
    out = _pack_bytes(lay, manifest, overlay_dir)
    got = _sha1(out)
    if check:
        orig = lay.wad.read_bytes()
        if out == orig:
            print(f"wadtool: pack OK — byte-identical (SHA-1 {got})")
        else:
            n = min(len(out), len(orig))
            first = next((i for i in range(n) if out[i] != orig[i]), n)
            _die(
                f"pack MISMATCH: first diff at 0x{first:x}; "
                f"sizes out={len(out)} orig={len(orig)}; SHA-1 {got} != {WAD_SHA1}"
            )
    if output:
        Path(output).write_bytes(out)
        print(f"wadtool: wrote {output} (SHA-1 {got})")
    elif not check:
        print(f"wadtool: packed in-memory (SHA-1 {got})")


def verify() -> None:
    """Round-trip proof: rebuild from current parts, assert SHA-1 == original."""
    lay = _layout()
    if not lay.manifest.exists():
        _die(f"{lay.manifest.relative_to(lay.repo)} missing — run 'wad unpack' first")
    manifest = json.loads(lay.manifest.read_text())
    if manifest["wad_sha1"] != WAD_SHA1:
        _die("manifest wad_sha1 != recorded baseline")
    out = _pack_bytes(lay, manifest)
    got = _sha1(out)
    if got != WAD_SHA1:
        _die(f"round-trip FAILED: SHA-1 {got} != {WAD_SHA1}")
    print(f"wadtool: round-trip OK — repacked WAD.WAD is byte-identical (SHA-1 {got})")
