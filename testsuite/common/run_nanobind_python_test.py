#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import pathlib
import sys

from pythonbinding_loaders import load_nanobind_oiio_package, load_python_test_run


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "Usage: run_nanobind_python_test.py <canonical-test-name> <build-root>"
        )

    test_name = sys.argv[1]
    build_root = pathlib.Path(sys.argv[2]).resolve()
    run = load_python_test_run(test_name, pathlib.Path(__file__))
    oiio = load_nanobind_oiio_package(build_root)

    try:
        run(oiio)
    except Exception as detail:
        print("Unknown exception:", detail)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
