"""Build and install the preview from a local Hard Battle ROM on Windows, macOS or Linux."""

import argparse
from dataclasses import dataclass
import json
import os
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
VISUAL_STUDIO_GENERATORS = {
    16: "Visual Studio 16 2019",
    17: "Visual Studio 17 2022",
    18: "Visual Studio 18 2026",
}
VISUAL_STUDIO_CMAKE = Path("Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")


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


@dataclass
class Toolchain:
    name: str
    cmake: str
    configure: list
    config: str | None = None

    def build_options(self):
        return ["--config", self.config] if self.config else []

    def output_dir(self, build_dir):
        return build_dir / self.config if self.config else build_dir


def find_visual_studio():
    root = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    vswhere = root / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    result = subprocess.run([
        str(vswhere), "-latest", "-products", "*", "-format", "json", "-utf8",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
    ], capture_output=True, encoding="utf-8")
    try:
        installs = json.loads(result.stdout) if result.returncode == 0 else []
    except json.JSONDecodeError:
        return None
    if not installs:
        return None
    major = int(installs[0]["installationVersion"].split(".")[0])
    generator = VISUAL_STUDIO_GENERATORS.get(major)
    return (Path(installs[0]["installationPath"]), generator) if generator else None


def ninja_toolchain(name, toolchain_bin, compilers):
    cmake = find_tool("cmake", toolchain_bin)
    ninja = find_tool("ninja", toolchain_bin)
    options = []
    if compilers:
        cc, cxx = (find_tool(compiler, toolchain_bin) for compiler in compilers)
        options = [f"-DCMAKE_C_COMPILER:FILEPATH={cc}", f"-DCMAKE_CXX_COMPILER:FILEPATH={cxx}"]
    return Toolchain(name, cmake, [
        "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_MAKE_PROGRAM:FILEPATH={ninja}", *options,
    ])


def msvc_toolchain(visual_studio):
    path, generator = visual_studio
    # the bundled CMake always knows its own Visual Studio generator
    bundled = path / VISUAL_STUDIO_CMAKE
    cmake = str(bundled) if bundled.is_file() else shutil.which("cmake")
    if cmake is None:
        raise ValueError("Could not find CMake on PATH or in the Visual Studio installation.")
    return Toolchain("msvc", cmake, [
        "-G", generator, "-A", "x64", f"-DCMAKE_GENERATOR_INSTANCE={path.as_posix()}",
    ], config="Release")


def select_toolchain(requested, toolchain_bin, find_vs=find_visual_studio):
    if not WINDOWS:
        if requested in ("msvc", "mingw"):
            raise ValueError(f"The {requested} toolchain is only available on Windows.")
        # cmake picks the system compiler unless a tool directory is given
        return ninja_toolchain("system", toolchain_bin, ("cc", "c++") if toolchain_bin else ())
    if requested == "msvc" and toolchain_bin is not None:
        raise ValueError("--toolchain-bin selects MinGW-w64 tools and cannot be combined with --toolchain msvc.")
    mingw_available = toolchain_bin is not None or shutil.which("gcc") is not None
    if requested == "mingw" or (requested == "auto" and mingw_available):
        return ninja_toolchain("mingw", toolchain_bin, ("gcc", "g++"))
    visual_studio = find_vs()
    if visual_studio is None:
        raise ValueError("Could not find MinGW-w64 GCC on PATH or Visual Studio 2019 or later "
                         "with the C++ x64 build tools.")
    return msvc_toolchain(visual_studio)


def require_sdl3(build_dir):
    cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    found = any(line.startswith("SDL3_DIR:") and not line.endswith("-NOTFOUND") for line in cache.splitlines())
    if not found:
        raise ValueError("CMake could not find SDL3. Install it (for example `brew install sdl3`) "
                         "or point CMAKE_PREFIX_PATH at an SDL3 installation.")


def install(rom, destination, toolchain_bin, title_graphic_rom=None, requested_toolchain="auto"):
    if destination.exists():
        raise ValueError(f"The install destination already exists: {destination}")

    print("Verifying the Hard Battle ROM...", flush=True)
    edition = identify_edition(rom)
    print(f"ROM profile: {edition}", flush=True)
    if title_graphic_rom and (edition != "hard_battle_usa" or identify_edition(title_graphic_rom) != "hard_battle_jpn"):
        raise ValueError("The alternate title graphic requires a USA base and a Japan revision 0 donor ROM.")

    toolchain = select_toolchain(requested_toolchain, toolchain_bin)
    print(f"Toolchain: {toolchain.name}", flush=True)
    cmake = toolchain.cmake

    destination.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="jb-", dir=destination.parent) as work_dir:
        work = Path(work_dir)
        build_dir = work / "b"
        staged = work / "install"
        subprocess.run([
            cmake, "-S", str(ROOT), "-B", str(build_dir),
            *toolchain.configure,
            "-DBUILD_TESTING=OFF",
            f"-DPython3_EXECUTABLE:FILEPATH={sys.executable}",
            f"-DRAMMA_HARD_BATTLE_ROM:FILEPATH={rom}",
        ], check=True)
        if not WINDOWS:
            require_sdl3(build_dir)
        subprocess.run([
            cmake, "--build", str(build_dir), *toolchain.build_options(),
            "--target", "hard_battle_preview", "ramma_recomp",
        ], check=True)
        subprocess.run([str(toolchain.output_dir(build_dir) / f"ramma_recomp{EXECUTABLE_SUFFIX}"), str(rom)], check=True)
        subprocess.run([
            cmake, "--install", str(build_dir), *toolchain.build_options(), "--prefix", str(staged),
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
    parser.add_argument("--toolchain", choices=("auto", "mingw", "msvc"), default="auto",
                        help="Windows compiler: MinGW-w64 GCC, or Visual Studio with the C++ build tools. "
                             "auto uses MinGW-w64 when gcc is on PATH, otherwise Visual Studio.")
    parser.add_argument("--toolchain-bin", type=Path,
                        help="Directory with CMake, Ninja and the C/C++ compilers (MinGW-w64 on Windows).")
    parser.add_argument("--title-graphic-rom", type=Path, help="Optional Japan revision 0 ROM supplying the USA title graphic.")
    args = parser.parse_args()
    destination = args.out.absolute()
    try:
        toolchain_bin = args.toolchain_bin.absolute() if args.toolchain_bin else None
        title_graphic_rom = args.title_graphic_rom.absolute() if args.title_graphic_rom else None
        install(args.rom.absolute(), destination, toolchain_bin, title_graphic_rom, args.toolchain)
    except subprocess.CalledProcessError as error:
        parser.exit(1, f"The local build failed with exit code {error.returncode}. See the build output above.\n")
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Installed: {destination / PREVIEW}")


if __name__ == "__main__":
    main()
