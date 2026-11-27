"""Inspect SNES HiROM dumps and compare them with local reference dumps."""

import argparse
from hashlib import sha1, sha256
from pathlib import Path


HIROM_HEADER = 0xFFC0


def read_dump(path):
    data = path.read_bytes()
    header_size = 512 if len(data) % 1024 == 512 else 0
    return data[header_size:], header_size


def compare(data, reference):
    changed = sum(left != right for left, right in zip(data, reference))
    return changed, len(data) - len(reference)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--reference", action="append", type=Path, default=[])
    args = parser.parse_args()
    references = [(path.name, read_dump(path)[0]) for path in args.reference]

    for path in sorted(args.directory.iterdir()):
        if not path.is_file() or path.suffix.lower() not in {".sfc", ".smc"}:
            continue
        data, copier_header = read_dump(path)
        header = data[HIROM_HEADER:HIROM_HEADER + 32]
        print(path.name)
        print(f"  Bytes: {len(data)} | Copier header: {copier_header} bytes | SHA-256: {sha256(data).hexdigest()}")
        print(f"  SHA-1: {sha1(data).hexdigest()}")
        if len(header) == 32 and header[21] in {0x21, 0x31}:
            title = header[:21].rstrip(b" \0").decode("ascii", errors="replace")
            checksum = int.from_bytes(header[30:32], "little")
            print(f"  HiROM title: {title} | Region: {header[25]} | Revision: {header[27]} | Header checksum: {checksum:04X}")
        if references:
            differences = [(compare(data, reference), name, reference) for name, reference in references]
            (changed, size_delta), name, reference = min(differences, key=lambda item: item[0][0] + abs(item[0][1]))
            print(f"  Closest reference: {name} | Changed bytes: {changed} | Size delta: {size_delta:+d}")
            same_before_title = data[:HIROM_HEADER] == reference[:HIROM_HEADER]
            same_after_title = data[HIROM_HEADER + 21:] == reference[HIROM_HEADER + 21:]
            if changed and same_before_title and same_after_title:
                print("  All changes are in the internal title field.")


if __name__ == "__main__":
    main()
