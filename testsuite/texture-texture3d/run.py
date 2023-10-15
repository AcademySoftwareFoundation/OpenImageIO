#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = ">> out.txt 2>&1"

# basic
command += testtex_command("-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "../openvdb/src/sphere.vdb -o out.exr")

# handles, derivatives
command += testtex_command("--handle --derivs --no-gettextureinfo "
                           "-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "-o out-handle.exr ../openvdb/src/sphere.vdb")

# more channels
command += testtex_command("-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "-nchannels 5 --no-gettextureinfo ../openvdb/src/sphere.vdb -o out-5chan.exr")

# subimages
command += testtex_command("-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "-subimage 0 --no-gettextureinfo ../openvdb/src/sphere.vdb -o out-subimage0.exr")
command += testtex_command("-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "-subimagename density --no-gettextureinfo ../openvdb/src/sphere.vdb -o out-subimagedensity.exr")
command += testtex_command("-res 2 2 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "-subimagename missing --no-gettextureinfo ../openvdb/src/sphere.vdb -o out-subimagemissing.exr")

# interp and mip modes
command += testtex_command("-res 128 128 --nowarp --offset 5 5 5 --scalest 3 3 "
                           "-interpmode 0 --no-gettextureinfo "
                           "../openvdb/src/sphere.vdb -o closest.exr")

outputs = [ "out.exr",
            "out-handle.exr",
            "out-5chan.exr",
            "out-subimage0.exr",
            "out-subimagedensity.exr",
            "out-subimagemissing.exr",
            "closest.exr",
            "out.txt" ]
