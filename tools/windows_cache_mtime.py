"""Restore checkout timestamps for unchanged inputs in a cached Windows build."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def tracked_inputs(root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--", "CMakeLists.txt", "cmake", "src"],
        cwd=root,
        check=True,
        stdout=subprocess.PIPE,
    )
    return [Path(os.fsdecode(path)) for path in result.stdout.split(b"\0") if path]


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(root: Path, manifest: Path, inputs: list[Path]) -> None:
    entries = {}
    for relative in inputs:
        path = root / relative
        if path.is_file():
            entries[relative.as_posix()] = {"sha256": digest(path), "mtime_ns": path.stat().st_mtime_ns}
    manifest.write_text(json.dumps(entries), encoding="utf-8")
    print(f"Recorded timestamps for {len(entries)} build inputs.")


def restore(root: Path, manifest: Path) -> None:
    if not manifest.is_file():
        print("No timestamp snapshot in the restored build cache.")
        return
    entries = json.loads(manifest.read_text(encoding="utf-8"))
    restored = 0
    for name, saved in entries.items():
        path = root / name
        if path.is_file() and digest(path) == saved["sha256"]:
            stat = path.stat()
            os.utime(path, ns=(stat.st_atime_ns, saved["mtime_ns"]))
            restored += 1
    print(f"Restored timestamps for {restored} unchanged build inputs.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=("save", "restore"))
    parser.add_argument("build_directory", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    manifest = args.build_directory / "checkout-mtimes.json"
    if args.action == "save":
        save(root, manifest, tracked_inputs(root))
    else:
        restore(root, manifest)


if __name__ == "__main__":
    main()
