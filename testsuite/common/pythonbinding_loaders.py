#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import importlib
import importlib.util
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


def load_python_test_run(test_name: str, script_path: pathlib.Path):
    """Load ``run(oiio)`` from a canonical Python testsuite module by path.

    The nanobind migration runners are executed as standalone scripts by
    ``runtest.py``, so they cannot rely on package-relative imports. This
    helper locates the original pybind11 test module under ``testsuite`` and
    returns its exported ``run`` function.
    """
    testsuite_root = None
    for parent in [script_path.resolve().parent] + list(script_path.resolve().parents):
        if parent.name == "testsuite":
            testsuite_root = parent
            break
    if testsuite_root is None:
        raise RuntimeError(f"Could not determine testsuite root from {script_path}")

    test_src_dir = testsuite_root / test_name / "src"
    test_modules = sorted(test_src_dir.glob("*.py"))
    if len(test_modules) != 1:
        raise RuntimeError(
            f"Expected exactly one Python test module in {test_src_dir}, "
            f"found {len(test_modules)}"
        )
    test_module = test_modules[0]
    spec = importlib.util.spec_from_file_location(f"oiio_{test_name}_module",
                                                  test_module)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load Python test module from {test_module}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.run
