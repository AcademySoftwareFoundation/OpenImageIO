# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import sys
import platform
import subprocess

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
OpenImageIO experimental Python package exposing nanobind migration bindings.
The production bindings are not installed in this configuration.
"""[1:-1]

__version__ = getattr(_ext, "__version__", "")


def _call_program(name, args):
    bin_dir = os.path.join(os.path.dirname(__file__), "bin")
    return subprocess.call([os.path.join(bin_dir, name)] + args)


def _command_line():
    name = os.path.basename(sys.argv[0])
    raise SystemExit(_call_program(name, sys.argv[1:]))
