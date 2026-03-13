#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import importlib
import pathlib
import sys

sys.path.insert(
    0, str(pathlib.Path(__file__).resolve().parents[2] / "python-roi" / "src")
)

from test_roi import run  # noqa: E402


def load_package(build_root: pathlib.Path):
    package_root = build_root / "lib/python/nanobind-experimental"
    if not (package_root / "OpenImageIO" / "__init__.py").exists():
        raise RuntimeError(f"Could not find OpenImageIO package in {package_root}")

    sys.path.insert(0, str(package_root))
    return importlib.import_module("OpenImageIO")


def main() -> int:
    build_root = pathlib.Path(sys.argv[1]).resolve()
    oiio = load_package(build_root)

    print("module:", oiio.__name__)
    print("version:", oiio.__version__)
    run(oiio)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
