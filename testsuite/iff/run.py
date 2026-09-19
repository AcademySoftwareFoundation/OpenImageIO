#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = ' >> out.txt 2>&1 '

command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --scanline -o gridscanline.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridscanline.iff")
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif --tile 64 64 -o gridtile.iff")
command += diff_command (OIIO_TESTSUITE_IMAGEDIR+"/grid.tif", "gridtile.iff")

# Regression test: verify reading of 16 bit rgba + float z (used to have a
# buffer overrun)
command += info_command("src/tiny_rgba16z.iff", hash=True)

# Regression test: rgba chunk size 0 caused subtraction underflow
command += info_command("src/bad_rgba_chunk_size.iff", hash=True, failureok=True)
# Regression test: zbuf chunk size 0 caused subtraction underflow
command += info_command("src/bad_zbuf_chunk_size.iff", hash=True, failureok=True)

# Regression test: a ZBUFFER-only (no RGBA) header advertised a 16-bit
# ImageSpec while the internal pixel size stayed 32-bit, causing
# read_native_tile to write past the caller's tile buffer. Must be rejected.
command += info_command("src/zbuffer_only.iff", hash=True, failureok=True)
# Regression test: a 336-byte IFF whose TBHD declares a ~8 GB image
# (46341x46341x4) -- a decompression bomb the compression-ratio guard must
# reject before any large allocation.
command += info_command("src/bomb-46341.iff", hash=True, failureok=True)

# Regression test: the decoded tile rectangles are not required to cover the
# whole image -- the loop stops once the declared tile count is consumed.
# These two 2x1 images satisfy that count while leaving pixel (1,0) with no
# tile: the first declares one tile too few, the second spends its second
# tile on a duplicate of the first. The staging buffer must be cleared, or
# the uncovered pixel returns whatever was in the heap. A stable hash here
# is the point of the test.
command += info_command("src/missing-tile.iff", hash=True)
command += info_command("src/duplicate-tile.iff", hash=True)
# Valid multi-tile layouts, including out-of-order, partial edge, and fully
# covered overlapping tiles, must still read correctly.
command += info_command("src/valid-multitile.iff", hash=True)
command += info_command("src/valid-out-of-order.iff", hash=True)
command += info_command("src/valid-edge-tile.iff", hash=True)
command += info_command("src/overlap-full-coverage.iff", hash=True)
