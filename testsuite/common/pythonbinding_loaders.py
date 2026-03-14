#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import importlib
import pathlib
import sys


def load_nanobind_oiio_package(build_root: pathlib.Path):
    """Import the staged nanobind OpenImageIO package from a build tree.

    The nanobind migration tests do not import an installed Python package.
    Instead, they point at the package directory that CMake stages under
    ``<build_root>/lib/python/nanobind`` and temporarily prepend that location
    to ``sys.path`` before importing ``OpenImageIO``.
    """
    package_root = build_root / "lib/python/nanobind"
    if not (package_root / "OpenImageIO" / "__init__.py").exists():
        raise RuntimeError(f"Could not find OpenImageIO package in {package_root}")

    sys.path.insert(0, str(package_root))
    return importlib.import_module("OpenImageIO")
