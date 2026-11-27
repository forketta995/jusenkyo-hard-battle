"""Build and install the Windows preview from a local Hard Battle ROM."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
from hard_battle_editions import identify_edition


ROOT = Path(__file__).resolve().parents[1]
INSTALL_FILES = (
    "ramma_spc.dll",
    "hard_battle_rom_path.txt",
    "snes_spc-license.txt",
    "hard_battle_preview.exe",
)
TITLE_GRAPHIC_SELECTION = "hard_battle_title_graphic_rom_path.txt"


def find_tool(name, toolchain_bin):
    if toolchain_bin is not None:
        executable = toolchain_bin / f"{name}.exe"
        if executable.is_file():
            return str(executable)
        raise ValueError(f"Missing {name}.exe in {toolchain_bin}.")
    executable = shutil.which(name)
    if executable is None:
        raise ValueError(f"Could not find {name} on PATH. Use --toolchain-bin to select a tool directory.")
    return executable


def install(rom, destination, toolchain_bin, title_graphic_rom=None):
    if sys.platform != "win32":
        raise ValueError("This builder currently targets Windows.")
    if destination.exists():
        raise ValueError(f"The install destination already exists: {destination}")

    print("Verifying the Hard Battle ROM...", flush=True)
    edition = identify_edition(rom)
    print(f"ROM profile: {edition}", flush=True)
    if title_graphic_rom and (edition != "hard_battle_usa" or identify_edition(title_graphic_rom) != "hard_battle_jpn"):
        raise ValueError("The alternate title graphic requires a USA base and a Japan revision 0 donor ROM.")

    cmake = find_tool("cmake", toolchain_bin)
    ninja = find_tool("ninja", toolchain_bin)
    cc = find_tool("gcc", toolchain_bin)
    cxx = find_tool("g++", toolchain_bin)

    destination.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="jb-", dir=destination.parent) as work_dir:
        work = Path(work_dir)
        build_dir = work / "b"
        staged = work / "install"
        subprocess.run([
            cmake, "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
            "-DBUILD_TESTING=OFF",
            f"-DCMAKE_C_COMPILER:FILEPATH={cc}",
            f"-DCMAKE_CXX_COMPILER:FILEPATH={cxx}",
            f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja}",
            f"-DPython3_EXECUTABLE:FILEPATH={sys.executable}",
            f"-DRAMMA_HARD_BATTLE_ROM:FILEPATH={rom}",
        ], check=True)
        subprocess.run([
            cmake, "--build", str(build_dir), "--target", "hard_battle_preview", "ramma_recomp",
        ], check=True)
        subprocess.run([str(build_dir / "ramma_recomp.exe")], check=True)
        subprocess.run([
            cmake, "--install", str(build_dir), "--prefix", str(staged),
            "--component", "HardBattlePreview",
        ], check=True)
        files = INSTALL_FILES
        if title_graphic_rom:
            (staged / TITLE_GRAPHIC_SELECTION).write_text(str(title_graphic_rom) + "\n", encoding="utf-8")
            files = (*INSTALL_FILES[:-1], TITLE_GRAPHIC_SELECTION, INSTALL_FILES[-1])
        # Claim the new directory after verification and publish the executable last.
        destination.mkdir()
        try:
            for name in files:
                shutil.copyfile(staged / name, destination / name)
        except OSError:
            for name in files:
                (destination / name).unlink(missing_ok=True)
            destination.rmdir()
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path, help="Path to a supported Hard Battle ROM.")
    parser.add_argument("--out", required=True, type=Path, help="New directory for the playable build.")
    parser.add_argument("--toolchain-bin", type=Path, help="Directory with CMake, Ninja and MinGW-w64 executables.")
    parser.add_argument("--title-graphic-rom", type=Path, help="Optional Japan revision 0 ROM supplying the USA title graphic.")
    args = parser.parse_args()
    destination = args.out.absolute()
    try:
        toolchain_bin = args.toolchain_bin.absolute() if args.toolchain_bin else None
        title_graphic_rom = args.title_graphic_rom.absolute() if args.title_graphic_rom else None
        install(args.rom.absolute(), destination, toolchain_bin, title_graphic_rom)
    except subprocess.CalledProcessError as error:
        parser.exit(1, f"The local build failed with exit code {error.returncode}. See the build output above.\n")
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Installed: {destination / 'hard_battle_preview.exe'}")


if __name__ == "__main__":
    main()
