#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


failureok = True
redirect = ' >> out.txt 2>&1 '

# Each file under src/ (built by src/make_malformed_vdb.cpp) carries an empty
# tree plus hostile file_bbox_min/file_bbox_max metadata, so the reader is
# asked for image extents that overflow, invert, or describe far more voxels
# than the file could hold. All must be rejected with a bounded error rather
# than crash or attempt an enormous allocation.
for f in [ "bad-bbox-huge.vdb",     # extent past the per-dimension ceiling
           "bad-bbox-bomb.vdb",     # ~32 PB of dense voxels from 436 bytes
           "bad-bbox-overflow.vdb", # extent overflows 32-bit arithmetic
           "bad-bbox-roundup.vdb",  # overflows when rounded to the leaf grid
           "bad-bbox-inverted.vdb", # min > max, i.e. negative resolution
           "bad-bbox-empty.vdb" ]:  # no active voxels at all
    command += oiiotool("--info -v src/" + f, failureok=True)

# Also rejected, but the message is OpenVDB's own and its wording varies by
# version, so keep it out of the compared output and just check that the read
# fails.
command += oiiotool("--info -v src/truncated.vdb > trunc.txt 2>&1",
                    failureok=True, silent=True)
command += oiiotool("-echo \"truncated.vdb rejected\"")
