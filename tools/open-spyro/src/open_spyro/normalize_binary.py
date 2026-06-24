"""Truncate / zero-pad a binary file to an exact length.

The link + ``objcopy -O binary`` of the main EXE (and each overlay) can emit
trailing ``.bss`` zero-fill past the real image end, or fall short. The original
files have a fixed length (the EXE ends at 0x66000; each overlay's length is frozen
in ``config/overlays.json``), so the build normalizes every produced image to that
exact size: drop any trailing bytes if longer, zero-pad if shorter.

Shared by ``tools/build_main.sh`` and ``tools/build_overlays.sh`` (previously a
copy-pasted inline Python heredoc in each).
"""

from __future__ import annotations

from pathlib import Path


def run(path: Path, size: int) -> None:
    data = path.read_bytes()
    if len(data) > size:
        data = data[:size]
    elif len(data) < size:
        data = data + b"\x00" * (size - len(data))
    path.write_bytes(data)
    print(f"normalize_binary: {path} normalized to {len(data)} bytes")
