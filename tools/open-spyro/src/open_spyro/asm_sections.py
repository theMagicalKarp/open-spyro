"""Give every function in a compiled C file its own sections (``config/compile.sh``).

gcc 2.7.2 predates ``-ffunction-sections``: a translation unit's code all lands in
one ``.text`` and its read-only data in one ``.rodata``. The fixed-VMA link places
each function at its own address, so it needs a section per function. This filter
runs on maspsx's output and renames, per function:

    .text                ->  .section .text.<fn>,"ax"
    .section .rodata     ->  .section .rodata.<fn>,"a"

Which function a directive belongs to:
  * inside a function (``.ent`` .. ``.end``, e.g. a jump table): that function;
  * a ``.text`` between functions: the next function (gcc switches to ``.text``
    just before each ``.globl`` / ``.ent``);
  * a ``.rodata`` chunk between functions (string literals gcc emits ahead of a
    function, or a file-scope ``const`` defined anywhere in the file): the one
    function whose body references a label the chunk defines. If no function or
    several do, the next function, else the previous one.

Directives that select no function (a file with no functions) are left as they
are. Only section names change, never an instruction, so a one-function file
assembles to the same bytes as before, just in ``.text.<fn>`` / ``.rodata.<fn>``.

Stdlib only; reads stdin, writes stdout:

    python3 -m open_spyro.asm_sections < maspsx.s > sectioned.s
"""

from __future__ import annotations

import re
import sys

_TEXT = re.compile(r"^\s*(?:\.text|\.section\s+\.text)\s*(?:,.*)?$")
_RODATA = re.compile(r"^\s*(?:\.rdata|\.section\s+\.(?:rodata|rdata))\s*(?:,.*)?$")
_SECTION = re.compile(r"^\s*(?:\.text|\.data|\.bss|\.rdata|\.sdata|\.sbss|\.section)\b")
_ENT = re.compile(r"^\s*\.ent\s+([A-Za-z_.$][\w.$]*)")
_END = re.compile(r"^\s*\.end\s+([A-Za-z_.$][\w.$]*)")
_LABEL = re.compile(r"^([A-Za-z_.$][\w.$]*):")
_WORD = re.compile(r"[A-Za-z_.$][\w.$]*")


def split_sections(text: str) -> str:
    lines = text.splitlines()

    # Function bodies: (name, first line, last line), and the symbols each mentions.
    bodies: list[tuple[str, int, int]] = []
    start: tuple[str, int] | None = None
    for i, line in enumerate(lines):
        if m := _ENT.match(line):
            start = (m.group(1), i)
        elif _END.match(line) and start:
            bodies.append((start[0], start[1], i))
            start = None
    refs = [
        {
            w
            for line in lines[a : b + 1]
            if not line.lstrip().startswith("#")  # maspsx "# DEBUG" notes
            for w in _WORD.findall(line)
        }
        for _, a, b in bodies
    ]

    def owner_at(i: int) -> int | None:
        """Index of the body containing line i, or None between functions."""
        for k, (_, a, b) in enumerate(bodies):
            if a <= i <= b:
                return k
        return None

    def next_body(i: int) -> int | None:
        return next((k for k, (_, a, _b) in enumerate(bodies) if a > i), None)

    def prev_body(i: int) -> int | None:
        return next((k for k in range(len(bodies) - 1, -1, -1) if bodies[k][2] < i), None)

    out = list(lines)
    for i, line in enumerate(lines):
        if not bodies:
            break
        inside = owner_at(i)
        if _TEXT.match(line):
            k = inside if inside is not None else next_body(i)
            if k is not None:
                out[i] = f'.section .text.{bodies[k][0]},"ax"'
        elif _RODATA.match(line):
            k = inside
            if k is None:
                # The chunk runs to the next section switch; who references its labels?
                labels: set[str] = set()
                for line2 in lines[i + 1 :]:
                    if _SECTION.match(line2) or _ENT.match(line2):
                        break
                    if m := _LABEL.match(line2):
                        labels.add(m.group(1))
                users = [k2 for k2, r in enumerate(refs) if labels & r]
                if len(users) == 1:
                    k = users[0]
                else:
                    k = next_body(i)
                    if k is None:
                        k = prev_body(i)
            if k is not None:
                out[i] = f'.section .rodata.{bodies[k][0]},"a"'

    # gcc emits a .text only when it is not already in .text: a function that
    # switched to .rodata and back (a jump table) leaves the next one with no
    # directive of its own, so it would land in the previous function's section.
    # Open its section just before the .align/.globl lines that lead its .ent.
    inserts: dict[int, str] = {}
    gap_start = 0
    for name, a, b in bodies:
        if not any(_TEXT.match(line) for line in lines[gap_start:a]):
            j = a
            while j > gap_start and re.match(r"^\s*\.(?:align|globl)\b", lines[j - 1]):
                j -= 1
            inserts[j] = f'.section .text.{name},"ax"'
        gap_start = b + 1
    for i in sorted(inserts, reverse=True):
        out.insert(i, inserts[i])
    return "\n".join(out) + "\n"


def main() -> None:
    sys.stdout.write(split_sections(sys.stdin.read()))


if __name__ == "__main__":
    main()
