"""Reads a session's log and says whether the console layer went through the expected states.

Why a verifier and not a trace to read: a trace gets read the day it is written. The next day one skims
it, and a line missing in the middle does not leap out. Here the expected sequence is written once, in the
call, and the machine compares.

It reads the lines `console_ui::set` writes, and those alone:

    the console: PLAY -> MENU (Start on the title screen)

Usage:
    py -3 tools/fv2/verify_console.py <log.txt> PLAY MENU PLAY STAGE PLAY PAUSE MENU_IN MENU

Output: one line per expected state, and exit code 0 if the sequence is exactly that one.
"""

import re
import sys

LINE = re.compile(r"the console: (\w+) -> (\w+) \(")


def sequence(log):
    """The sequence of states passed through, the first one included."""
    seen = []
    for line in log.splitlines():
        m = LINE.search(line)
        if not m:
            continue
        frm, to = m.group(1), m.group(2)
        if not seen:
            seen.append(frm)
        elif seen[-1] != frm:
            # a state change that does not start where the previous one arrived: the trace has a hole
            seen.append("?" + frm)
        seen.append(to)
    return seen


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    expected = argv[2:]
    with open(argv[1], "r", encoding="utf-8", errors="replace") as f:
        got = sequence(f.read())
    width = max(len(e) for e in expected + got) if got else 10
    right = True
    for i in range(max(len(expected), len(got))):
        a = expected[i] if i < len(expected) else "-"
        o = got[i] if i < len(got) else "-"
        mark = "  " if a == o else "<-- here"
        right = right and a == o
        print(f"  {a:<{width}}  {o:<{width}}  {mark}")
    print()
    print("the sequence of states is the one expected" if right
          else "THE SEQUENCE OF STATES IS NOT THE ONE EXPECTED")
    return 0 if right else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
