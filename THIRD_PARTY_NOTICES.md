# Third-party notices

## COBF — C/C++ Sourcecode Obfuscator/Shrouder

The bundled backend identifies itself at runtime as COBF 1.06 (2006-01-07).
It was written by Bernhard Baier and is not original work of the cppobf
project.

The upstream copyright notice says that COBF may be freely distributed when
the distribution includes the complete original source files and the copyright
notice. The unmodified source package supplied with this installation is kept
under vendor/cobf/original-source/, including copyright.txt, readme.txt,
documentation, sources, and the upstream GPL-covered lexer material described
by that notice.

The original executable and token resources under vendor/cobf/bin/ and
vendor/cobf/etc/ are retained. Starting with cppobf 0.1.3, the runtime is
`vendor/cppobf-backend/bin/cppobf-backend`, a separately labeled derivative.
Its modified lexer is in `vendor/cppobf-backend/lexer.l`; the reproducible build
recipe is `scripts/build_backend.py`. That recipe changes the runtime label
in a build copy of the original main.cpp and regenerates the scanner using
Flex. It also changes one token-spacing rule in a build copy of scanproc.h.
These AI-assisted compatibility changes add hexadecimal floating-token
and binary-integer recognition and preserve literal/word boundaries.
They do not replace the upstream authorship
or copyright notices, and the complete supplied original distribution remains
included with the derivative.

Please consult the complete upstream text at:

- vendor/cobf/original-source/copyright.txt
- vendor/cobf/original-source/readme.txt
- vendor/cobf/original-source/doc/cobf.pdf

Note: the supplied archive's version.txt says 1.07 while the bundled binary
self-reports 1.06. This notice records both facts instead of silently resolving
the discrepancy.
