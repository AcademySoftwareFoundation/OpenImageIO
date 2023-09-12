# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os, sys, platform

# This works around the python 3.8 change to stop loading DLLs from PATH on Windows.
# We reproduce the old behaviour by manually tokenizing PATH, checking that the directories exist and are not ".",
# then add them to the DLL load path.
# This behviour can be disabled by setting the environment variable "OIIO_LOAD_DLLS_FROM_PATH" to "0"
if sys.version_info >= (3, 8) and platform.system() == "Windows" and os.getenv("OIIO_LOAD_DLLS_FROM_PATH", "1") == "1":
    for path in os.getenv("PATH", "").split(os.pathsep):
        if os.path.exists(path) and path != ".":
            os.add_dll_directory(path)

from .OpenImageIO import *

