#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

import subprocess
import sys
import tempfile
from pathlib import Path


if len(sys.argv) != 3:
    print("usage: check-rowsperstrip-zero.py <iinfo> <oiiotool>")
    sys.exit(2)

iinfo = sys.argv[1]
oiiotool = sys.argv[2]
src = Path(__file__).with_name("rowsperstrip-zero.exr")

with tempfile.TemporaryDirectory() as tmpdir:
    out = str(Path(tmpdir) / "rowsperstrip-zero.exr")
    checks = [
        [iinfo, "--hash", str(src)],
        [iinfo, "--stats", str(src)],
        [oiiotool, str(src), "-o", out],
    ]
    for cmd in checks:
        result = subprocess.run(cmd, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        if result.returncode != 0:
            print("command failed with rc={}: {}".format(
                  result.returncode, " ".join(cmd)))
            sys.exit(1)
