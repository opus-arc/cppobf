# Changelog

## 0.1.3 — 2026-11-04

- Fix hexadecimal floating-point constants being split into numbers and
  identifiers by the backend lexer, which previously produced invalid or
  renamed exponents/significands. Keep literal spelling and type intact.
- Recognize binary integers and hexadecimal digit separators.
- Keep a space between a string literal and a following word, including
  `extern "C" const ...`, which otherwise became an invalid literal suffix.
- Preserve Clang attributes and the NEON/Accelerate system names exercised by
  the Horowitz build, including stream-buffer base types.
- Add repeatable `--preserve` token files for explicit external ABI contracts.
- Collect all Clang errors per verification pass so the diagnostic repair
  loop does not stop behind the first 20 errors in a large translation unit.
- Build the modified backend under a distinct name and retain the original
  COBF distribution and notices.
- Add bit-pattern, macro spelling, numeric-type, system API and cross-file
  link regressions, alongside the existing compatibility tests.
