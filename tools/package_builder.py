"""Package the Hard Battle builder and its source inputs as a ZIP."""

import argparse
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parents[1]
INPUTS = (
    "CMakeLists.txt",
    "cmake/SnesRecompilation.cmake",
    "cmake/HardBattleTargets.cmake",
    "docs/local-builder.md",
    "tools/build_hard_battle.py",
    "tools/hard_battle_editions.py",
    "tools/recompile_65816.py",
)
SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp"}


def source_files():
    yield from (ROOT / name for name in INPUTS)
    for profile in sorted((ROOT / "config").glob("hard_battle_*/rom.toml")):
        yield profile
        yield profile.with_name("preview.cmake")
    for directory in (ROOT / "src/runtime", ROOT / "third_party/snes_spc"):
        for path in sorted(directory.rglob("*")):
            if not path.is_file():
                continue
            if path.suffix in SOURCE_SUFFIXES or path.name == "CMakeLists.txt":
                yield path
    yield ROOT / "third_party/snes_spc/license.txt"
    yield ROOT / "third_party/snes_spc/README.md"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    destination = parser.parse_args().out.resolve()
    if destination.exists():
        parser.exit(1, f"The archive already exists: {destination}\n")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="jb-package-", dir=destination.parent) as work_dir:
        staged = Path(work_dir) / "builder.zip"
        with ZipFile(staged, "w") as archive:
            for path in source_files():
                entry = ZipInfo(path.relative_to(ROOT).as_posix())
                archive.writestr(entry, path.read_bytes(), compress_type=ZIP_DEFLATED)
        staged.rename(destination)
    print(f"Packaged: {destination}")


if __name__ == "__main__":
    main()
