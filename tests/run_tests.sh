#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="${TMPDIR:-/tmp}/cppobf-tests-$$"
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
mkdir -p "$build_dir"

"${CXX:-c++}" \
  -std=c++20 \
  -O2 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -I"$root/include" \
  "$root/tests/compatibility_test.cpp" \
  "$root/src/compatibility.cpp" \
  -o "$build_dir/compatibility_test"

"$build_dir/compatibility_test"
