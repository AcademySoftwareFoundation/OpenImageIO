#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import importlib.util
import pathlib
import sys


def load_module(build_root: pathlib.Path):
    search_dirs = [
        build_root / "lib/python/nanobind-experimental",
        build_root,
    ]
    matches = []
    for module_dir in search_dirs:
        matches = sorted(module_dir.glob("_nanobind_experimental*"))
        if matches:
            break
    if not matches:
        raise RuntimeError(f"Could not find nanobind module in {search_dirs}")

    module_path = matches[0]
    spec = importlib.util.spec_from_file_location("_nanobind_experimental",
                                                  module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load spec for {module_path}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    build_root = pathlib.Path(sys.argv[1]).resolve()
    nbexp = load_module(build_root)

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
