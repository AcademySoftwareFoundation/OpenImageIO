# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import sys
import platform

_here = os.path.abspath(os.path.dirname(__file__))

# Set $OpenImageIO_ROOT if not already set before importing helper modules.
if not os.getenv("OpenImageIO_ROOT"):
    if all([os.path.exists(os.path.join(_here, i)) for i in ["share", "bin", "lib"]]):
        os.environ["OpenImageIO_ROOT"] = _here

if platform.system() == "Windows":
    _bin_dir = os.path.join(_here, "bin")
    if os.path.exists(_bin_dir):
        os.add_dll_directory(_bin_dir)
    elif sys.version_info >= (3, 8):
        if os.getenv("OPENIMAGEIO_PYTHON_LOAD_DLLS_FROM_PATH", "0") == "1":
            for path in os.getenv("PATH", "").split(os.pathsep):
                if os.path.exists(path) and path != ".":
                    os.add_dll_directory(path)

from . import _OpenImageIO as _ext  # noqa: E402
from ._OpenImageIO import *  # type: ignore # noqa: E402, F401, F403

__doc__ = """
OpenImageIO Python package exposing the nanobind migration bindings.
The production pybind11 bindings are not installed in this configuration.
"""[1:-1]

__version__ = getattr(_ext, "__version__", "")

# TODO: Restore the Python CLI entry-point trampolines when the nanobind
# package ships the full wheel-style bin/lib/share layout.
