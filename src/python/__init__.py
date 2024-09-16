# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os, sys, platform, subprocess

# This works around the python 3.8 change to stop loading DLLs from PATH on Windows.
# We reproduce the old behaviour by manually tokenizing PATH, checking that the directories exist and are not ".",
# then add them to the DLL load path.
# This behviour can be disabled by setting the environment variable "OIIO_LOAD_DLLS_FROM_PATH" to "0"
if sys.version_info >= (3, 8) and platform.system() == "Windows" and os.getenv("OIIO_LOAD_DLLS_FROM_PATH", "1") == "1":
    for path in os.getenv("PATH", "").split(os.pathsep):
        if os.path.exists(path) and path != ".":
            os.add_dll_directory(path)


from .OpenImageIO import *

__version__ = VERSION_STRING

__status__ = "dev"

__author__ = "OpenImageIO Contributors"

__email__ = "oiio-dev@lists.aswf.io"

__license__ = "SPDX-License-Identifier: Apache-2.0"

__copyright__ = "Copyright Contributors to the OpenImageIO Project"

__doc__ = """OpenImageIO is a toolset for reading, writing, and manipulating image files of 
any image file format relevant to VFX / animation via a format-agnostic API 
with a feature set, scalability, and robustness needed for feature film 
production.
"""


def _call_program(name, args):
    BIN_DIR = os.path.join(os.path.dirname(__file__), 'bin')
    return subprocess.call([os.path.join(BIN_DIR, name)] + args)

def _command_line():
    name = os.path.basename(sys.argv[0])
    raise SystemExit(_call_program(name, sys.argv[1:]))
