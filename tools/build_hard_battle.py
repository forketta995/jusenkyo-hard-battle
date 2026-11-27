"""Build and install the preview from a local Hard Battle ROM on Windows, macOS or Linux."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
from hard_battle_editions import identify_edition


ROOT = Path(__file__).resolve().parents[1]
WINDOWS = sys.platform == "win32"
EXECUTABLE_SUFFIX = ".exe" if WINDOWS else ""
SPC_LIBRARY = {"win32": "ramma_spc.dll", "darwin": "ramma_spc.dylib"}.get(sys.platform, "ramma_spc.so")
PREVIEW = f"hard_battle_preview{EXECUTABLE_SUFFIX}"
INSTALL_FILES = (
    SPC_LIBRARY,
    "hard_battle_rom_path.txt",
    "snes_spc-license.txt",
    PREVIEW,
)
TITLE_GRAPHIC_SELECTION = "hard_battle_title_graphic_rom_path.txt"


def find_tool(name, toolchain_bin):
    if toolchain_bin is not None:
        executable = toolchain_bin / f"{name}{EXECUTABLE_SUFFIX}"
        if executable.is_file():
            return str(executable)
        raise ValueError(f"Missing {executable.name} in {toolchain_bin}.")
    executable = shutil.which(name)
    if executable is None:
        raise ValueError(f"Could not find {name} on PATH. Use --toolchain-bin to select a tool directory.")
    return executable


def compiler_options(toolchain_bin):
    # windows uses MinGW-w64; elsewhere CMake picks the system compiler unless a tool directory is given
    if not WINDOWS and toolchain_bin is None:
        return []
    names = ("gcc", "g++") if WINDOWS else ("cc", "c++")
    cc, cxx = (find_tool(name, toolchain_bin) for name in names)
    return [f"-DCMAKE_C_COMPILER:FILEPATH={cc}", f"-DCMAKE_CXX_COMPILER:FILEPATH={cxx}"]


def require_sdl3(build_dir):
    cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    found = any(line.startswith("SDL3_DIR:") and not line.endswith("-NOTFOUND") for line in cache.splitlines())
    if not found:
        raise ValueError("CMake could not find SDL3. Install it (for example `brew install sdl3`) "
                         "or point CMAKE_PREFIX_PATH at an SDL3 installation.")


def install(rom, destination, toolchain_bin, title_graphic_rom=None):
    if destination.exists():
        raise ValueError(f"The install destination already exists: {destination}")

    print("Verifying the Hard Battle ROM...", flush=True)
    edition = identify_edition(rom)
    print(f"ROM profile: {edition}", flush=True)
    if title_graphic_rom and (edition != "hard_battle_usa" or identify_edition(title_graphic_rom) != "hard_battle_jpn"):
        raise ValueError("The alternate title graphic requires a USA base and a Japan revision 0 donor ROM.")

    cmake = find_tool("cmake", toolchain_bin)
    ninja = find_tool("ninja", toolchain_bin)
    compilers = compiler_options(toolchain_bin)
    build_type = [] if WINDOWS else ["-DCMAKE_BUILD_TYPE=Release"]

    destination.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="jb-", dir=destination.parent) as work_dir:
        work = Path(work_dir)
        build_dir = work / "b"
        staged = work / "install"
        subprocess.run([
            cmake, "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
            "-DBUILD_TESTING=OFF",
            *build_type,
            *compilers,
            f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja}",
            f"-DPython3_EXECUTABLE:FILEPATH={sys.executable}",
            f"-DRAMMA_HARD_BATTLE_ROM:FILEPATH={rom}",
        ], check=True)
        if not WINDOWS:
            require_sdl3(build_dir)
        subprocess.run([
            cmake, "--build", str(build_dir), "--target", "hard_battle_preview", "ramma_recomp",
        ], check=True)
        subprocess.run([str(build_dir / f"ramma_recomp{EXECUTABLE_SUFFIX}")], check=True)
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
                # copy2 keeps the executable bit of the preview on POSIX hosts
                shutil.copy2(staged / name, destination / name)
        except OSError:
            for name in files:
                (destination / name).unlink(missing_ok=True)
            destination.rmdir()
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path, help="Path to a supported Hard Battle ROM.")
    parser.add_argument("--out", required=True, type=Path, help="New directory for the playable build.")
    parser.add_argument("--toolchain-bin", type=Path,
                        help="Directory with CMake, Ninja and the C/C++ compilers (MinGW-w64 on Windows).")
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
    print(f"Installed: {destination / PREVIEW}")


if __name__ == "__main__":
    main()
