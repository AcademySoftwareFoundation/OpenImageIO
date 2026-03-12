#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import importlib
import pathlib
import sys


def load_package(build_root: pathlib.Path):
    package_root = build_root / "lib/python/nanobind-experimental"
    if not (package_root / "OpenImageIO" / "__init__.py").exists():
        raise RuntimeError(f"Could not find OpenImageIO package in {package_root}")

    sys.path.insert(0, str(package_root))
    return importlib.import_module("OpenImageIO")


def main() -> int:
    build_root = pathlib.Path(sys.argv[1]).resolve()
    nbexp = load_package(build_root)

    print("module:", nbexp.__name__)
    print("version:", nbexp.__version__)

    roi = nbexp.ROI(1, 4, 2, 6)
    other = nbexp.ROI(0, 2, 1, 3)

    print("roi:", roi)
    print("contains (1,2):", roi.contains(1, 2))
    print("contains (5,2):", roi.contains(5, 2))
    print("contains other:", roi.contains(other))
    print("union:", nbexp.union(roi, other))
    print("intersection:", nbexp.intersection(roi, other))
    print("ROI.All defined:", nbexp.ROI.All.defined)
    print("copy equals:", roi.copy() == roi)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
