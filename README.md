# cppobf

cppobf is a small macOS frontend using a patched COBF-derived backend. It is
intended for a single C++ translation unit produced by a tool such as
cpp-amalgamate.

## Build

    ./build.sh

Apple Clang, Flex, Python 3 and the macOS command-line developer tools are required.
The build regenerates the lexer and builds `cppobf-backend`; the retained
original COBF distribution is not modified.

## Install with Homebrew

    brew install opus-arc/tap/cppobf

Binary releases target Apple Silicon macOS.

Before publishing a release, run the compatibility and end-to-end checks:

    ./tests/run_tests.sh
    ./tests/run_integration.sh

## Usage

    cppobf <input.cpp> -o <output.cpp> [options]

Typical use:

    cpp-amalgamate amalgamation.cpp > single.cpp
    ./cppobf single.cpp -o obfuscated.cpp --verify

Options are --verify, --std <standard>, --preserve <file>, and --keep-temp. Run
./cppobf --help for the complete concise reference.

The lexer recognizes hexadecimal floating literals and binary integers as
complete numeric tokens. It preserves hexadecimal floating spelling and type,
including `f`/`L` suffixes, without converting values through host floating point.
The compatibility stage removes decimal digit separators and hexadecimal
separators inside `0x` numeric tokens. Ordinary and u8 raw strings are converted to
escaped literals. For C++17 and newer, the diagnostic string in a two-argument
`static_assert` is removed before COBF sees it; the assertion condition is
preserved verbatim. Wide raw strings (uR, UR, and LR) are rejected because
converting them without a declared source encoding could change semantics.

With --verify, cppobf runs Apple Clang syntax checking. On a failure, cppobf
consults COBF's identifier map and preserves only identifiers that diagnostics
clearly place in the standard library or a known system API context. It stops
with the diagnostics if a repair would require guessing that an identifier is
external.

Verification also treats unknown C++ attributes as errors, so a renamed
standard attribute cannot be silently ignored. Simple `extern "C"` function
declarations and definitions in the amalgamated input retain their C ABI names;
check exported symbols against any public header before distributing a library.
For cross-translation-unit C++ names or externally defined data, pass
`--preserve external.tok` (repeatable). Each non-comment line must contain one
identifier. This is an explicit ABI contract; application-specific names are
not added to the global system preset.

Temporary files are created under the system temporary directory and removed
by default. This no-space location is required because COBF 1.06 does not quote
the preprocessor command it constructs internally. --keep-temp retains the
directory for inspection. The final output is a single .cpp; any generated
cobf.h or uncobf.h include is inlined.

## Scope and limitations

- One already-amalgamated .cpp input only.
- Syntax verification only; framework and application linking are outside the
  first version's scope.
- COBF 1.06 is token based rather than C++ semantic aware. A globally preserved
  system name shared by user code cannot be renamed selectively.
- Compilation cannot prove behavioral equivalence. Test the resulting program
  with the application's own test suite before distribution.

COBF is third-party software by Bernhard Baier. The modified runtime is labeled
`cppobf-backend`, and the original binary and complete source distribution are
retained. See THIRD_PARTY_NOTICES.md and
vendor/cobf/original-source/copyright.txt.
