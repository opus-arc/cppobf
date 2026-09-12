# cppobf

cppobf is a small macOS frontend for the COBF 1.06 source obfuscator. It is
intended for a single C++ translation unit produced by a tool such as
cpp-amalgamate.

## Build

    ./build.sh

Apple Clang and the macOS command-line developer tools are required.

## Install with Homebrew

    brew install opus-arc/tap/cppobf

The first binary release targets Apple Silicon macOS.

## Usage

    cppobf <input.cpp> -o <output.cpp> [options]

Typical use:

    cpp-amalgamate amalgamation.cpp > single.cpp
    ./cppobf single.cpp -o obfuscated.cpp --verify

Options are --verify, --std <standard>, and --keep-temp. Run
./cppobf --help for the complete concise reference.

The compatibility stage removes digit separators only when an apostrophe is
between two decimal digits. Ordinary and u8 raw strings are converted to
escaped literals. Wide raw strings (uR, UR, and LR) are rejected because
converting them without a declared source encoding could change semantics.

With --verify, cppobf runs Apple Clang syntax checking. On a failure, cppobf
consults COBF's identifier map and preserves only identifiers that diagnostics
clearly place in the standard library or a known system API context. It stops
with the diagnostics if a repair would require guessing that an identifier is
external.

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

COBF is third-party software by Bernhard Baier. See THIRD_PARTY_NOTICES.md and
vendor/cobf/original-source/copyright.txt.
