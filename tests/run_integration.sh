#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/cppobf-integration.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
mkdir -p "$test_dir/with spaces"

"$root/build.sh"
"$root/cppobf" --version | grep -F 'cppobf 0.1.2' >/dev/null
xcrun clang++ -std=c++20 "$root/tests/integration_fixture.cpp" \
  -o "$test_dir/original"
"$root/cppobf" "$root/tests/integration_fixture.cpp" \
  -o "$test_dir/with spaces/obfuscated.cpp" --verify \
  >"$test_dir/report.txt"
grep -F 'removed 2 static_assert messages' "$test_dir/report.txt" >/dev/null
xcrun clang++ -std=c++20 -fsyntax-only -Werror=unknown-attributes \
  "$test_dir/with spaces/obfuscated.cpp"
xcrun clang++ -std=c++20 "$test_dir/with spaces/obfuscated.cpp" \
  -o "$test_dir/obfuscated"
nm -gU "$test_dir/obfuscated" | grep -E ' _ForeignValue$' >/dev/null
nm -gU "$test_dir/obfuscated" | grep -E ' _ApiEntry$' >/dev/null
"$test_dir/original" >"$test_dir/original.out"
"$test_dir/obfuscated" >"$test_dir/obfuscated.out"
cmp "$test_dir/original.out" "$test_dir/obfuscated.out"

printf 'sentinel\n' >"$test_dir/with spaces/rejected.cpp"
printf 'const char* text = LR"(unsupported)";\n' >"$test_dir/wide.cpp"
if "$root/cppobf" "$test_dir/wide.cpp" \
    -o "$test_dir/with spaces/rejected.cpp" --verify \
    >"$test_dir/wide.out" 2>"$test_dir/wide.err"; then
  echo 'wide raw string should have been rejected' >&2
  exit 1
fi
grep -F 'Wide raw string literals' "$test_dir/wide.err" >/dev/null
printf 'sentinel\n' | cmp - "$test_dir/with spaces/rejected.cpp"

if "$root/cppobf" "$root/tests/integration_fixture.cpp" \
    -o "$test_dir/with spaces/rejected.cpp" --std 'c++20;invalid' \
    >"$test_dir/std.out" 2>"$test_dir/std.err"; then
  echo 'invalid C++ standard should have been rejected' >&2
  exit 1
fi
grep -F 'Unsupported --std value' "$test_dir/std.err" >/dev/null
printf 'sentinel\n' | cmp - "$test_dir/with spaces/rejected.cpp"

echo 'end-to-end integration tests passed'
