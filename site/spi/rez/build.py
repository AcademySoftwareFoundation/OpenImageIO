#!/usr/bin/env python
import shutil
import os

folders = [
    'bin',
    'include',
    'lib',
    'python',
    'share',
]

src = os.environ['REZ_BUILD_SOURCE_PATH']
dest = os.environ['REZ_BUILD_INSTALL_PATH']

for folder in folders:
    shutil.copytree(os.path.join(src, folder), os.path.join(dest, folder))
