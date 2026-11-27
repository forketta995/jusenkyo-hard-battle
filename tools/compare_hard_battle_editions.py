"""Compare ROM banks and US control-flow anchors against another Hard Battle edition."""

import argparse
from pathlib import Path
import tomllib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args()
    base = args.base.read_bytes()
    candidate = args.candidate.read_bytes()
    rules = Path(__file__).resolve().parents[1] / "config/hard_battle_usa/rom.toml"
    config = tomllib.loads(rules.read_text(encoding="utf-8"))
    for address in range(0, min(len(base), len(candidate)), 0x10000):
        end = address + 0x10000
        equal = sum(left == right for left, right in zip(base[address:end], candidate[address:end]))
        print(f"Bank {0xC0 + (address >> 16):02X}: {equal / 65536:.1%} at equal offsets")

    anchors = sorted({entry["address"] & 0x3FFFFF
                      for key in ("stack_dispatches", "indexed_jumps", "conditional_checkpoints")
                      for entry in config[key]})
    for anchor in anchors:
        before, after = max(0, anchor - 8), anchor + 9
        signature = base[before:after]
        match = candidate.find(signature)
        unique = match != -1 and candidate.find(signature, match + 1) == -1
        resolved = match + anchor - before
        print(f"{anchor + 0xC00000:06X}: {resolved + 0xC00000:06X}" if unique else f"{anchor + 0xC00000:06X}: unresolved")
    print(f"Reset vectors: {int.from_bytes(base[0xFFFC:0xFFFE], 'little'):04X} / "
          f"{int.from_bytes(candidate[0xFFFC:0xFFFE], 'little'):04X}")


if __name__ == "__main__":
    main()
