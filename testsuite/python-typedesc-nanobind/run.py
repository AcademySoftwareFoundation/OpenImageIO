#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

refdirlist = [make_relpath(os.path.join(OIIO_TESTSUITE_ROOT, "python-typedesc", "ref"))]
command += pythonbin + " src/test_typedesc_nanobind.py " + OIIO_BUILD_ROOT + " > out.txt"
