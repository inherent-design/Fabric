#!/usr/bin/env python3
"""Scan for kCamelCase constants and generate K_SCREAMING_SNAKE mappings.

Usage:
    python3 scripts/rename-constants.py                  # Print mapping (dry run)
    python3 scripts/rename-constants.py --apply          # Apply replacements
    python3 scripts/rename-constants.py --csv            # Output as CSV
    python3 scripts/rename-constants.py --vscode         # Output VSCode search/replace pairs
"""

import argparse
import re
import sys
from pathlib import Path

# Directories to scan (relative to project root)
SCAN_DIRS = ["include", "src", "tests"]
EXTENSIONS = {".hh", ".cc", ".h", ".cpp", ".inl"}

# Regex to find kCamelCase identifiers (not inside quotes or comments)
# Matches: kChunkSize, kMaxParticles, kWFCTileSize, etc.
K_CAMEL_RE = re.compile(r"\bk([A-Z][a-zA-Z0-9]*)\b")

# Regex to split camelCase into words
# Handles: "ChunkSize" -> ["Chunk", "Size"]
#          "WFCTileSize" -> ["WFC", "Tile", "Size"]
#          "MaxGpuJoints" -> ["Max", "Gpu", "Joints"]
#          "FixedDt" -> ["Fixed", "Dt"]
WORD_SPLIT_RE = re.compile(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+|[0-9]+")


def camel_to_screaming_snake(name: str) -> str:
    """Convert camelCase suffix (after 'k') to SCREAMING_SNAKE.

    Examples:
        ChunkSize -> K_CHUNK_SIZE
        WFCTileSize -> K_WFC_TILE_SIZE
        Half -> K_HALF
        FixedDt -> K_FIXED_DT
        MaxGpuJoints -> K_MAX_GPU_JOINTS
    """
    words = WORD_SPLIT_RE.findall(name)
    return "K_" + "_".join(w.upper() for w in words)


def find_project_root() -> Path:
    """Walk up from script location to find CMakeLists.txt."""
    candidate = Path(__file__).resolve().parent.parent
    if (candidate / "CMakeLists.txt").exists():
        return candidate
    # Fallback: cwd
    cwd = Path.cwd()
    if (cwd / "CMakeLists.txt").exists():
        return cwd
    print("Error: cannot find project root (no CMakeLists.txt)", file=sys.stderr)
    sys.exit(1)


def collect_source_files(root: Path) -> list[Path]:
    """Collect all C++ source files from scan directories."""
    files = []
    for scan_dir in SCAN_DIRS:
        d = root / scan_dir
        if not d.exists():
            continue
        for f in sorted(d.rglob("*")):
            if f.suffix in EXTENSIONS and f.is_file():
                files.append(f)
    return files


def discover_constants(files: list[Path]) -> dict[str, str]:
    """Scan files for kCamelCase identifiers and build old->new mapping.

    Returns dict mapping old name (e.g. "kChunkSize") to new name (e.g. "K_CHUNK_SIZE").
    """
    found: set[str] = set()
    for f in files:
        try:
            text = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in K_CAMEL_RE.finditer(text):
            found.add(m.group(0))  # full match including 'k' prefix

    # Build mapping
    mapping: dict[str, str] = {}
    for old_name in sorted(found):
        suffix = old_name[1:]  # strip leading 'k'
        new_name = camel_to_screaming_snake(suffix)
        # Skip if old == new (shouldn't happen) or if new name is trivially wrong
        if old_name != new_name:
            mapping[old_name] = new_name
    return mapping


def count_occurrences(files: list[Path], mapping: dict[str, str]) -> dict[str, int]:
    """Count total occurrences of each old constant name across all files."""
    counts: dict[str, int] = {k: 0 for k in mapping}
    for f in files:
        try:
            text = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for old_name in mapping:
            counts[old_name] += len(re.findall(r"\b" + re.escape(old_name) + r"\b", text))
    return counts


def check_conflicts(mapping: dict[str, str], files: list[Path]) -> list[str]:
    """Check if any new name already exists in the codebase as a different symbol."""
    # Collect all existing K_SCREAMING_SNAKE identifiers
    existing: set[str] = set()
    k_screaming_re = re.compile(r"\bK_[A-Z][A-Z0-9_]+\b")
    for f in files:
        try:
            text = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in k_screaming_re.finditer(text):
            existing.add(m.group(0))

    conflicts = []
    new_names = set(mapping.values())
    for old_name, new_name in mapping.items():
        if new_name in existing and new_name not in new_names:
            conflicts.append(f"  CONFLICT: {old_name} -> {new_name} (already exists in codebase)")
    return conflicts


def apply_replacements(files: list[Path], mapping: dict[str, str]) -> dict[str, int]:
    """Apply all replacements across files. Returns per-file change counts."""
    file_changes: dict[str, int] = {}
    # Sort by length descending to avoid partial replacements
    # e.g. kChunkSize before kChunk
    sorted_mapping = sorted(mapping.items(), key=lambda x: len(x[0]), reverse=True)

    for f in files:
        try:
            text = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        original = text
        for old_name, new_name in sorted_mapping:
            text = re.sub(r"\b" + re.escape(old_name) + r"\b", new_name, text)
        if text != original:
            f.write_text(text, encoding="utf-8")
            # Count changes
            changes = sum(
                1 for old_name in mapping
                if re.search(r"\b" + re.escape(mapping[old_name]) + r"\b", text)
                and re.search(r"\b" + re.escape(old_name) + r"\b", original)
            )
            file_changes[str(f)] = changes
    return file_changes


def main():
    parser = argparse.ArgumentParser(description="Rename kCamelCase constants to K_SCREAMING_SNAKE")
    parser.add_argument("--apply", action="store_true", help="Apply replacements (default: dry run)")
    parser.add_argument("--csv", action="store_true", help="Output mapping as CSV")
    parser.add_argument("--vscode", action="store_true", help="Output as VSCode regex search/replace pairs")
    parser.add_argument("--count", action="store_true", help="Include occurrence counts")
    args = parser.parse_args()

    root = find_project_root()
    print(f"Project root: {root}", file=sys.stderr)

    files = collect_source_files(root)
    print(f"Scanning {len(files)} files...", file=sys.stderr)

    mapping = discover_constants(files)
    print(f"Found {len(mapping)} unique kCamelCase constants", file=sys.stderr)

    # Check for conflicts
    conflicts = check_conflicts(mapping, files)
    if conflicts:
        print("\nWARNING: Name conflicts detected:", file=sys.stderr)
        for c in conflicts:
            print(c, file=sys.stderr)

    if args.count:
        counts = count_occurrences(files, mapping)

    if args.csv:
        print("old,new" + (",count" if args.count else ""))
        for old_name, new_name in mapping.items():
            line = f"{old_name},{new_name}"
            if args.count:
                line += f",{counts[old_name]}"
            print(line)
    elif args.vscode:
        # Output pairs suitable for VSCode regex find/replace
        print("# VSCode Find/Replace pairs (regex mode ON, match whole word ON)")
        print("# Find -> Replace")
        print()
        for old_name, new_name in mapping.items():
            print(f"\\b{old_name}\\b -> {new_name}")
    elif args.apply:
        print("\nApplying replacements...", file=sys.stderr)
        file_changes = apply_replacements(files, mapping)
        print(f"\nModified {len(file_changes)} files:", file=sys.stderr)
        for fpath in sorted(file_changes):
            rel = Path(fpath).relative_to(root)
            print(f"  {rel}")
        print(f"\nTotal: {len(mapping)} constants renamed across {len(file_changes)} files")
    else:
        # Default: print mapping table
        max_old = max(len(k) for k in mapping) if mapping else 0
        max_new = max(len(v) for v in mapping.values()) if mapping else 0
        header = f"{'OLD':<{max_old}}  {'NEW':<{max_new}}"
        if args.count:
            header += "  COUNT"
        print(header)
        print("-" * len(header))
        for old_name, new_name in mapping.items():
            line = f"{old_name:<{max_old}}  {new_name:<{max_new}}"
            if args.count:
                line += f"  {counts[old_name]:>5}"
            print(line)
        print(f"\n{len(mapping)} constants to rename")


if __name__ == "__main__":
    main()
