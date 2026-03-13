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

    spec = nbexp.ImageSpec()
    spec.x = 10
    spec.y = 20
    spec.z = 0
    spec.width = 30
    spec.height = 40
    spec.depth = 1
    spec.nchannels = 3
    spec.full_x = 8
    spec.full_y = 18
    spec.full_z = 0
    spec.full_width = 34
    spec.full_height = 44
    spec.full_depth = 1

    print("spec roi:", nbexp.get_roi(spec))
    print("spec roi_full:", nbexp.get_roi_full(spec))
    nbexp.set_roi(spec, nbexp.ROI(12, 16, 22, 26, 0, 1, 0, 2))
    nbexp.set_roi_full(spec, nbexp.ROI(7, 19, 17, 29, 0, 1, 0, 4))
    print("updated roi:", nbexp.get_roi(spec))
    print("updated roi_full:", nbexp.get_roi_full(spec))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
