# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import os
import sys
import logging
import platform
import subprocess

logger = logging.getLogger(__name__)

if platform.system() == "Windows":
    # Python wheel module is dynamically linked to the OIIO DLL present in the bin folder.
    _here = os.path.abspath(os.path.dirname(__file__))
    _bin_dir = os.path.join(_here, "bin")
    if os.path.exists(_bin_dir):
        if sys.version_info >= (3, 8):
            os.add_dll_directory(_bin_dir)  
        else:
            os.environ['PATH'] = '{0};{1}'.format(_bin_dir, os.getenv('PATH', ''))
    
    #TODO: Remove. Python 3.8 stopped loading DLLs from PATH on Windows for security reasons.
    if sys.version_info >= (3, 8):
        # This works around the python 3.8 change to stop loading DLLs from PATH on Windows.
        # We reproduce the old behavior by manually tokenizing PATH, checking that the 
        # directories exist and are not ".", then add them to the DLL load path.
        # Set the environment variable "OIIO_LOAD_DLLS_FROM_PATH" to "0" to disable this behavior.
        if os.getenv("OIIO_LOAD_DLLS_FROM_PATH", "1") == "1":
            logger.warning(
                "Adding DLL directories to PATH. This behavior will be removed in "
                "a future version. Set the environment variable OIIO_LOAD_DLLS_FROM_PATH "
                "to 0 to disable this behavior."
            )
            for path in os.getenv("PATH", "").split(os.pathsep):
                if os.path.exists(path) and path != ".":
                    os.add_dll_directory(path)

from .OpenImageIO import * # type: ignore # noqa: F401, F403, E402

__version__ = VERSION_STRING # type: ignore # noqa: F405

__status__ = "production"

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
    bin_dir = os.path.join(os.path.dirname(__file__), 'bin')
    return subprocess.call([os.path.join(bin_dir, name)] + args)

def _command_line():
    name = os.path.basename(sys.argv[0])
    raise SystemExit(_call_program(name, sys.argv[1:]))
