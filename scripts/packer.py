#!/usr/bin/env python3
"""Zenith Project Source Code Packer Tool

Packs source files into a single text file located in scripts/output/.
Root directory is always the project root.
"""

import os
import sys
import argparse
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
OUTPUT_DIR = SCRIPT_DIR / "output"
DEFAULT_OUTPUT_FILE = "output.txt"

DEFAULT_EXTENSIONS = {'.h', '.cpp', '.hpp', '.lua', '.zen', '.test'}
IGNORE_DIRS = {'build', '.xmake', '.git', 'bin', 'scripts', 'doc', 'docs', '.vscode', '__pycache__', '.build_cache'}


def collect_files(search_path: Path, extensions: set[str], output_path: Path) -> list[Path]:
    """Recursively collects and sorts matching files under search_path."""
    collected = []
    script_path = Path(__file__).resolve()

    for root, dirs, files in os.walk(search_path):
        dirs[:] = sorted([d for d in dirs if d not in IGNORE_DIRS])
        for file in sorted(files):
            file_path = Path(root) / file
            if file_path.suffix.lower() in extensions:
                try:
                    resolved = file_path.resolve()
                    if resolved == output_path.resolve() or resolved == script_path:
                        continue
                except OSError:
                    pass
                collected.append(file_path)

    collected.sort()
    return collected


def pack_files(subdirs: list[str], output_filename: str, mode: str, extensions: set[str]):
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    clean_filename = Path(output_filename).name or DEFAULT_OUTPUT_FILE
    output_path = (OUTPUT_DIR / clean_filename).resolve()

    target_paths = []
    if subdirs:
        for sub in subdirs:
            t = (PROJECT_ROOT / sub).resolve()
            if not t.exists():
                print(f"[X] Error: Directory does not exist: {t}")
                sys.exit(1)
            target_paths.append(t)
    else:
        target_paths = [PROJECT_ROOT]

    all_files: list[Path] = []
    seen = set()
    for target in target_paths:
        for f in collect_files(target, extensions, output_path):
            if f.resolve() not in seen:
                seen.add(f.resolve())
                all_files.append(f)

    file_mode = 'a' if mode == 'append' else 'w'
    file_count = 0

    with open(output_path, file_mode, encoding="utf-8") as outfile:
        for path in all_files:
            try:
                rel_path = path.resolve().relative_to(PROJECT_ROOT)
            except ValueError:
                rel_path = path

            header = f"{'='*60}\nFILE: {rel_path}\n{'='*60}\n"
            if file_count > 0 or (file_mode == 'a' and outfile.tell() > 0):
                outfile.write(f"\n\n{header}")
            else:
                outfile.write(f"{header}")

            try:
                with open(path, "r", encoding="utf-8", errors="replace") as infile:
                    outfile.write(infile.read())
                file_count += 1
            except Exception as e:
                outfile.write(f"[Error reading file: {e}]\n")

    rel_out = output_path.relative_to(PROJECT_ROOT) if output_path.is_relative_to(PROJECT_ROOT) else output_path
    print(f"[✓] Successfully packed {file_count} files into '{rel_out}' (Mode: {mode.upper()}).")


def parse_extensions(raw_exts: list[str]) -> set[str]:
    exts = set()
    for item in raw_exts:
        for part in item.replace(',', ' ').split():
            clean = part.strip().lower()
            if not clean:
                continue
            if not clean.startswith('.'):
                clean = '.' + clean
            exts.add(clean)
    return exts or DEFAULT_EXTENSIONS


def main():
    parser = argparse.ArgumentParser(
        description="Zenith Source Code Packer Tool",
        epilog="Examples:\n"
               "  python3 scripts/packer.py\n"
               "  python3 scripts/packer.py -d src\n"
               "  python3 scripts/packer.py -d src/Zenith/VM -o vm_code.txt\n"
               "  python3 scripts/packer.py -d tests -m append -o tests.txt",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        "-d", "--dir",
        dest="dirs",
        nargs="+",
        default=[],
        help="Specific subfolder(s) relative to project root to pack (e.g. -d src or -d src/Zenith/VM). Defaults to project root."
    )

    parser.add_argument(
        "-m", "--mode",
        choices=["clear", "append"],
        default="clear",
        help="Write mode: 'clear' (overwrite) or 'append' (append to existing file). Default is 'clear'."
    )

    parser.add_argument(
        "-o", "--output",
        default=DEFAULT_OUTPUT_FILE,
        help=f"Output filename (always placed in scripts/output/). Default is '{DEFAULT_OUTPUT_FILE}'."
    )

    parser.add_argument(
        "-e", "--ext",
        nargs="+",
        default=[],
        help="Optional file extension filter (e.g. -e .cpp .h or -e cpp,h). Defaults to standard Zenith extensions."
    )

    args = parser.parse_args()
    exts = parse_extensions(args.ext) if args.ext else DEFAULT_EXTENSIONS
    pack_files(args.dirs, args.output, args.mode, exts)


if __name__ == "__main__":
    main()
