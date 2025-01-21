# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import sys
import platform
import subprocess

_here = os.path.abspath(os.path.dirname(__file__))

# Set $OpenImageIO_ROOT if not already set *before* importing the OpenImageIO module.
if not os.getenv("OpenImageIO_ROOT"):
    if all([os.path.exists(os.path.join(_here, i)) for i in ["share", "bin", "lib"]]): 
        os.environ["OpenImageIO_ROOT"] = _here

if platform.system() == "Windows":
    # Python wheel module is dynamically linked to the OIIO DLL present in the bin folder.
    _bin_dir = os.path.join(_here, "bin")
    if os.path.exists(_bin_dir):
        os.add_dll_directory(_bin_dir) 
    elif sys.version_info >= (3, 8):
        # This works around the python 3.8 change to stop loading DLLs from PATH on Windows.
        # We reproduce the old behavior by manually tokenizing PATH, checking that the 
        # directories exist and are not ".", then add them to the DLL load path.
        # This behavior is disabled by default, but can be enabled by setting the environment
        # variable "OPENIMAGEIO_PYTHON_LOAD_DLLS_FROM_PATH" to "1"
        if os.getenv("OPENIMAGEIO_PYTHON_LOAD_DLLS_FROM_PATH", "0") == "1":
            for path in os.getenv("PATH", "").split(os.pathsep):
                if os.path.exists(path) and path != ".":
                    os.add_dll_directory(path)

from .OpenImageIO import * # type: ignore # noqa: F401, F403, E402

__doc__ = """
OpenImageIO is a toolset for reading, writing, and manipulating image files of 
any image file format relevant to VFX / animation via a format-agnostic API 
with a feature set, scalability, and robustness needed for feature film 
production.
"""[1:-1]


def _call_program(name, args):
    bin_dir = os.path.join(os.path.dirname(__file__), 'bin')
    return subprocess.call([os.path.join(bin_dir, name)] + args)

def _command_line():
    name = os.path.basename(sys.argv[0])
    raise SystemExit(_call_program(name, sys.argv[1:]))
