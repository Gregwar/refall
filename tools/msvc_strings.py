#!/usr/bin/env python3
"""MSVC refuses a string literal over 16380 bytes (error C2026), and the universes' shaders are raw string literals,
often longer. This splits each long raw literal into pieces at line ends (`)"` then `R"(` on the next line: the
compiler joins adjacent literals, so the string is unchanged). MSVC's limit for the joined string is 65535 bytes: a
shader longer than that (`inline const char* NAME = R"(...)" R"(...)";`) is joined at run time instead, as a
std::string that NAME points into.

usage: tools/msvc_strings.py          split them in place (src/)
       tools/msvc_strings.py --check  only list them, exit 1 if any (e.g. before a release)
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PIECE = 12000  # well under 16380
RAW = re.compile(r'R"([^()\\\s]{0,16})\((.*?)\)\1"', re.S)


def split(body, delim):
    pieces, cur = [], ""
    for line in body.splitlines(keepends=True):
        if cur and len(cur.encode()) + len(line.encode()) > PIECE:
            pieces.append(cur)
            cur = ""
        cur += line
    pieces.append(cur)
    return (')' + delim + '"\nR"' + delim + '(').join(pieces)


DECL = re.compile(r'inline const char\* (\w+) = ((?:\s*R"([^()\\\s]{0,16})\(.*?\)\3")+)\s*;', re.S)


def join_at_run_time(text):
    """The declarations whose literals add up to more than MSVC's 65535 bytes, as a std::string joined at start."""
    n = 0

    def fix(m):
        nonlocal n
        name, lits = m.group(1), m.group(2)
        pieces = [x.group(0) for x in RAW.finditer(lits)]
        if sum(len(x.encode()) for x in pieces) <= 64000:
            return m.group(0)
        n += 1
        return (f"inline const std::string {name}_SRC = std::string(" + pieces[0] + ")" +
                "".join(" +\n" + x for x in pieces[1:]) + ";\n"
                f"inline const char* {name} = {name}_SRC.c_str();")

    text = DECL.sub(fix, text)
    if n and "#include <string>" not in text:
        text = text.replace("#pragma once\n", "#pragma once\n#include <string>\n", 1)
    return text, n


def main():
    check = "--check" in sys.argv
    found = 0
    for base, _, files in os.walk(os.path.join(ROOT, "src")):
        for name in files:
            if not name.endswith((".hpp", ".cpp", ".h")):
                continue
            path = os.path.join(base, name)
            text = open(path, encoding="utf-8").read()
            changed = False

            def fix(m):
                nonlocal found, changed
                delim, body = m.group(1), m.group(2)
                if len(body.encode()) <= PIECE + 2000:
                    return m.group(0)
                found += 1
                rel = os.path.relpath(path, ROOT)
                line = text.count("\n", 0, m.start()) + 1
                print(f"{rel}:{line}: {len(body.encode())} bytes")
                if check:
                    return m.group(0)
                changed = True
                return 'R"' + delim + '(' + split(body, delim) + ')' + delim + '"'

            new = RAW.sub(fix, text)
            new, joined = join_at_run_time(new)
            if joined:
                found += joined
                print(f"{os.path.relpath(path, ROOT)}: {joined} shader(s) over 65535 bytes, to join at run time")
                changed = not check
            if changed:
                open(path, "w", encoding="utf-8").write(new)
    if check and found:
        print(f"{found} string literals too long for MSVC: run tools/msvc_strings.py")
        sys.exit(1)


if __name__ == "__main__":
    main()
