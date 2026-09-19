#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Allow some LSB slop -- necessary because of the alpha disassociation
# combined with implied sRGB conversion.
failthresh = 0.02
failpercent = 0.02
redirect = ' >> out.txt 2>&1 '
failureok = True

imagedir = OIIO_TESTSUITE_IMAGEDIR + "/jpeg2000/broken"
files = [ "issue_3427.jp2" ]
for f in files:
    command += rw_command (imagedir, f, printinfo=False)

# Try to parse library_list to find the OpenJPEG version. If found, return it as
# a single number, e.g. 2.5.3 -> 20503. Return -1 if not found.
def get_openjp_ver():
    liblist = (subprocess.check_output([oiio_app('oiiotool').strip(),
                                       '--echo', '{getattribute(library_list)}'])
               .strip().decode('utf-8'))
    for elem in liblist.split(";"):
        if elem.startswith("jpeg2000"):
            try:
                major, minor, build = (int(x) for x in elem.split()[-1].split("."))
                return (major * 10000) + (minor * 100) + build
            except Exception:
                pass
    return -1

# Test for writing jpeg2000 with icc profile -- this test case is only added for
# OpenJPEG >= 2.5.4 because ICC profile writing only became reliable from that version.
if get_openjp_ver() >= 20504:
    command += oiiotool("../common/tahoe-tiny.tif --iccread ref/test-jp2.icc -o tahoe-icc.jp2")
    command += oiiotool("--echo \"ICCProfile of tahoe-icc.jp2\" --info -v tahoe-icc.jp2 | grep \"ICCProfile\"")
    command += oiiotool("tahoe-icc.jp2 --iccwrite test-jp2.icc")

    outputs = [
        "test-jp2.icc",
        "out.txt"
    ]
