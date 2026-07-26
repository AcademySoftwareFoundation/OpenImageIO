#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# Capture stderr too, so the error messages from the malformed-header
# regression reads below land in out.txt and are checked against the ref.
redirect = " >> out.txt 2>&1 "

imagedir = "src"
files = [ "triangle.ptx" ]
for f in files:
    command += info_command (imagedir + "/" + f, extraargs="--stats")

# Regression tests for malformed/hostile headers. PtexInput validates the
# fixed 64-byte header before handing the file to the Ptex library, so each of
# these must be rejected cleanly (no crash, no over-allocation). See
# src/make_malformed_ptex.py for how the fixtures are generated.
for f in [ "truncated-header.ptx", "bad-magic.ptx", "bad-levelinfo.ptx",
           "bomb-metadata.ptx", "bomb-leveldata.ptx", "bad-nfaces.ptx" ]:
    command += info_command ("src/" + f, verbose=False, failureok=True)
