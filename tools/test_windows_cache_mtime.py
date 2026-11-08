"""A restored build must never hide changed inputs from Ninja."""

import os
from pathlib import Path
import tempfile
import unittest

from windows_cache_mtime import restore, save


class TimestampCacheTests(unittest.TestCase):
    def test_restore_only_unchanged_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            unchanged = root / "unchanged.h"
            changed = root / "changed.h"
            missing = root / "missing.h"
            manifest = root / "manifest.json"
            for path in (unchanged, changed, missing):
                path.write_text("original", encoding="utf-8")
                os.utime(path, ns=(1_600_000_000_000_000_000, 1_600_000_000_000_000_000))
            save(root, manifest, [Path(path.name) for path in (unchanged, changed, missing)])

            unchanged.touch()
            changed.write_text("modified", encoding="utf-8")
            missing.unlink()
            changed_time = changed.stat().st_mtime_ns
            restore(root, manifest)

            self.assertEqual(1_600_000_000_000_000_000, unchanged.stat().st_mtime_ns)
            self.assertEqual(changed_time, changed.stat().st_mtime_ns)


if __name__ == "__main__":
    unittest.main()
