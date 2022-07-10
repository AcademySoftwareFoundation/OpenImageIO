import os, sys, platform

if sys.version_info >= (3, 8) and platform.system() == "Windows" and os.getenv("OIIO_LOAD_DLLS_FROM_PATH", "0") == "1":
    for path in os.getenv("PATH", "").split(os.pathsep):
        if os.path.exists(path) and path != ".":
            os.add_dll_directory(path)

from .OpenImageIO import *

