#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import pathlib
import sys

import OpenImageIO as oiio

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "common"))

from roi_shared import run  # noqa: E402


try:
    run(oiio)
except Exception as detail:
    print("Unknown exception:", detail)
