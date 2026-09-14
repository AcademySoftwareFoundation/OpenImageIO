#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# A multi-frame DICOM presents each frame as a subimage. Every frame here is
# a different constant value, so the per-subimage hashes catch both losing
# the frames entirely and reading the same frame more than once. The
# dicom: metadata is filtered out because it varies with the DCMTK version.

command += pythonbin + " src/make_multiframe.py multiframe.dcm ;\n"
command += info_command("multiframe.dcm",
                        extraargs="--no-metamatch \"dicom:\"")

outputs = [ "out.txt" ]
