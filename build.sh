#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
xcrun clang++ \
  -std=c++20 \
  -O2 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -I"$root/include" \
  "$root/src/main.cpp" \
  "$root/src/obfuscator.cpp" \
  "$root/src/compatibility.cpp" \
  "$root/src/process_runner.cpp" \
  -o "$root/cppobf"

echo "Built $root/cppobf"
