#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import pathlib
import sys

# runtest.py executes this file as a script, not as part of a Python package,
# so relative imports are not available here. Add the ROI test directory and
# shared helper directory to sys.path explicitly, then import the canonical ROI
# test body plus the helper that loads the staged nanobind package from build/.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "common"))
sys.path.insert(
    0, str(pathlib.Path(__file__).resolve().parents[2] / "python-roi" / "src")
)

from pythonbinding_loaders import load_nanobind_oiio_package  # noqa: E402
from test_roi import run  # noqa: E402


def main() -> int:
    build_root = pathlib.Path(sys.argv[1]).resolve()
    oiio = load_nanobind_oiio_package(build_root)

    run(oiio)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
