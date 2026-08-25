#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# save the error output
redirect = " >> out.txt 2>&1 "
failureok = 1

command += rw_command (OIIO_TESTSUITE_IMAGEDIR + "/broken",
                       "invalid_gray_alpha_sbit.png",
                       printinfo=False)

# These carry malformed Exif payloads in a PNG eXIf chunk, reaching the same
# shared decoder as the .jpg fixtures in testsuite/jpeg-corrupt/src. The eXIf
# length field is 31 bits, so this is the route where a deeply nested IFD
# chain can be made large enough to exhaust the stack.
command += info_command ("src/exif-utf8-type.png", safematch=True)
command += info_command ("src/exif-deep-ifds.png", safematch=True)

# These carry hostile XMP payloads in a compressed zTXt chunk, reaching the
# shared XMP decoder. PNG is the useful container: a zTXt payload is deflated
# and its length is 31 bits, so packets far larger than a 64 KB JPEG APP1
# marker cost only tens of KB on disk.
#
# xmp-deep-nesting nests 100000 elements; decode_xmp_node() recursed once per
# level and exhausted the stack (SIGSEGV on a plain release build).
command += info_command ("src/xmp-deep-nesting.png", safematch=True)
# The next three are volume attacks, so their metadata is deliberately not
# dumped -- what regresses is time and memory, not the attribute values.
# xmp-list-blowup appends 20000 dc:subject items; each append re-split,
# re-joined and re-interned the whole accumulated list, which took 11 s and
# 3.2 GB before the budget cap. xmp-many-attribs writes 8000 attributes into
# a spec whose attribute lookup is a linear scan. xmp-multi-packet spreads
# that same attack over 8 zTXt chunks, each within its own cap: the scan is
# over the whole spec, so a budget that reset per packet left it costing 3.8 s
# here and 211 s for a 1 MB file.
command += info_command ("src/xmp-list-blowup.png", verbose=False)
command += info_command ("src/xmp-many-attribs.png", verbose=False)
command += info_command ("src/xmp-multi-packet.png", verbose=False)
