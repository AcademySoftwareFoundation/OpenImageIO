#!/usr/bin/env python3
"""Compatibility wrapper for the shared sampling regression."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parents[3]
    script = repo_root / "src" / "imiv" / "tools" / "imiv_sampling_regression.py"
    cmd = [sys.executable, str(script), *sys.argv[1:]]
    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
