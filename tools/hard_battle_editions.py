"""Identify a supported Hard Battle edition from the cartridge contents."""

import argparse
from hashlib import file_digest
from pathlib import Path
import tomllib


ROOT = Path(__file__).resolve().parents[1]


def identify_edition(rom):
    with rom.open("rb") as file:
        digest = file_digest(file, "sha256").hexdigest()
    for profile in sorted((ROOT / "config").glob("hard_battle_*/rom.toml")):
        with profile.open("rb") as file:
            config = tomllib.load(file)
        if config["sha256"] == digest:
            return profile.parent.name
    raise ValueError("ROM SHA-256 does not match a supported Hard Battle edition.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    args = parser.parse_args()
    try:
        print(identify_edition(args.rom))
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
