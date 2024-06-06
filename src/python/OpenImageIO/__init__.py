import os
import sys
import platform


if sys.version_info >= (3, 8) and platform.system() == 'Windows':
    os.add_dll_directory(os.path.dirname(__file__), 'libs')

from .OpenImageIO import *
