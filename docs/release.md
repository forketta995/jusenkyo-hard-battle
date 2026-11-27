# First alpha release

Proposed tag `v0.1.0-alpha.1`.

## Release notes

- Build the Windows preview locally from a USA or Japan revision 0 ROM.
- Play from the title through Ranma's opening cutscene and first match against
  Akane, with original stereo audio, pause, and Continue.
- Optionally load the Japanese title graphic in the USA preview using a second
  local ROM.
- Requires Python 3.11 or later, CMake 3.20 or later, Ninja, and 64-bit MinGW-w64.
  The builder is currently distributed as a ZIP of source and configuration.

## Package and verify

Run these commands from the repository root.

```powershell
python tools/package_builder.py --out dist/jusenkyo-hard-battle-builder-v0.1.0-alpha.1-windows.zip
Get-FileHash -Algorithm SHA256 -LiteralPath dist/jusenkyo-hard-battle-builder-v0.1.0-alpha.1-windows.zip
```

Extract the ZIP to a fresh directory and follow [the builder instructions](local-builder.md)
with each supported ROM. Run the repository tests in separate USA and Japan
build directories. Add `RAMMA_HARD_BATTLE_TITLE_GRAPHIC_ROM` to the USA build
to include the mixed-title test.

Attach the builder ZIP and its SHA-256 to the GitHub release. Keep local ROMs,
generated C, built executables, and local ROM paths outside the release assets.
