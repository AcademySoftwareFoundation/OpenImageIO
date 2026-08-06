#!/usr/bin/env python3
"""Compatibility wrapper for the shared imiv backend verification runner.

Preferred direct entrypoint:

  python src\\imiv\\tools\\imiv_backend_verify.py --backend vulkan --build-dir build

This wrapper preserves the previous Windows-specific command path.
"""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    sys.path.insert(0, str(script_dir))
    from imiv_backend_verify import main as backend_main

    return backend_main()


if __name__ == "__main__":
    raise SystemExit(main())
