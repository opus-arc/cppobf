#!/usr/bin/env python3
"""Build cppobf-backend from the retained COBF sources (AI-assisted changes)."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
original = root / "vendor/cobf/original-source/src"
build = root / ".build/backend"
build.mkdir(parents=True, exist_ok=True)
main = (original / "cpp/main.cpp").read_text()
# The upstream license requires a distinct label for modified versions.
for old, new in [
    ('"1.06 2006-01-07"', '"0.1.3 (derived from COBF 1.06)"'),
    ('"This is COBF ("', '"This is cppobf-backend ("'),
    ('" COBF "', '" cppobf-backend "'),
    ('syntax: cobf options', 'syntax: cppobf-backend options'),
]:
    assert old in main, old
    main = main.replace(old, new)
(build / "main.cpp").write_text(main)
scanner = (original / "include/scanproc.h").read_text()
old = "if (/* 1st Rule: */ (last_char_type == WORD && char_type == WORD) ||"
new = """// cppobf: prevent a string literal and following word becoming a UDL.
    if ((last_char == '"' && char_type == WORD) ||
        /* 1st Rule: */ (last_char_type == WORD && char_type == WORD) ||"""
assert scanner.count(old) == 1
(build / "scanproc.h").write_text(scanner.replace(old, new))
with (build / "lexer.cpp").open("w") as out:
    subprocess.run([os.environ.get("FLEX", "flex"), "-t",
                    str(root / "vendor/cppobf-backend/lexer.l")],
                   stdout=out, check=True)
sources = [build / "main.cpp", build / "lexer.cpp"]
sources += [original / "cpp" / f"{name}.cpp" for name in
            ["lib", "b_alloc", "b_array", "b_cont", "b_str", "b_obj", "b_set"]]
output = root / "vendor/cppobf-backend/bin/cppobf-backend"
output.parent.mkdir(exist_ok=True)
subprocess.run(["xcrun", "clang++", "-std=c++11", "-O2", "-Dunix",
                "-Wno-deprecated-declarations", "-I" + str(build),
                "-I" + str(original / "include"),
                *map(str, sources), "-o", str(output)], check=True)
print(f"Built {output}")
