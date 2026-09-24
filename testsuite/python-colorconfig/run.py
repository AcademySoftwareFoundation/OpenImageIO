#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


import os

command += run_app(pythonbin + " src/test_colorconfig.py")
command += run_app(
    pythonbin + ' src/test_normalization.py "' + OIIO_PROJECT_ROOT
    + '/src/libOpenImageIO/interop-identities-config.ocio"')
command += run_app(
    pythonbin + ' src/test_properties_api.py "' + OIIO_PROJECT_ROOT
    + '/src/libOpenImageIO/interop-identities-config.ocio"')
