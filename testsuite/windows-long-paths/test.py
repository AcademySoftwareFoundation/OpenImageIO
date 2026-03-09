"""Test script for long path support in compiled Windows executables.

This script verifies that `oiiotool` can read paths longer than MAX_PATH (260)
characters on Windows.

Reference: https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
"""

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from uuid import uuid4

_WINDOWS_MAX_PATH = 260


def main(src_root: str, oiiotool_exe: str) -> int:
    """
    Manufactures a long directory path within a temp. directory, copies a test
    image to it, and ensures `oiiotool` can read it without erroring.
    """
    if not os.path.isfile(oiiotool_exe):
        print(f"ERROR: oiiotool path {oiiotool_exe!r} does not exist")
        return 1

    test_image = "grid.tif"
    src_image_path = os.path.join(src_root, "testsuite", "common", test_image)

    with tempfile.TemporaryDirectory() as test_dir:
        while len(test_dir) < _WINDOWS_MAX_PATH:
            test_dir = os.path.join(test_dir, str(uuid4()))
        os.makedirs(test_dir, exist_ok=True)

        shutil.copy(src_image_path, test_dir)
        test_image_path = os.path.join(test_dir, test_image)

        print("Testing long image path:", test_image_path, flush=True)
        result = subprocess.call([oiiotool_exe, test_image_path, "--printinfo"])

    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Windows long path test")
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--oiiotool-path", required=True)
    args = parser.parse_args()

    sys.exit(main(args.source_root, args.oiiotool_path))
