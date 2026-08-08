#!/usr/bin/env python3
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Generate small malformed Ptex fixtures for the security-audit regression
# suite. Each fixture targets one branch of PtexInput's 64-byte-header
# validator (ptex_validate_header) so that a corrupt or hostile header is
# rejected cheaply before the Ptex library is asked to open it. See
# https://ptex.us/PtexFile.html for the on-disk header layout.

import struct

HEADER_FMT = "<4sIIIiHHIIIIIIQII"  # matches PtexHeader on-disk layout
assert struct.calcsize(HEADER_FMT) == 64


def header(magic=b"Ptex", version=1, meshtype=1, datatype=3, alphachan=-1,
           nchannels=3, nlevels=1, nfaces=1, extheadersize=0, faceinfosize=20,
           constdatasize=12, levelinfosize=16, minorversion=0,
           leveldatasize=0, metadatazipsize=0, metadatamemsize=0):
    return struct.pack(HEADER_FMT, magic, version, meshtype, datatype,
                       alphachan, nchannels, nlevels, nfaces, extheadersize,
                       faceinfosize, constdatasize, levelinfosize,
                       minorversion, leveldatasize, metadatazipsize,
                       metadatamemsize)


def write(name, data):
    open(name, "wb").write(data)
    print("wrote %s (%d bytes)" % (name, len(data)))


# H1 truncation: fewer than 64 header bytes -> "file too small".
write("truncated-header.ptx", b"Ptex\x01\x00\x00\x00")

# H8 bad magic: full 64 bytes but wrong signature.
write("bad-magic.ptx", header(magic=b"Nope"))

# H8 inconsistent level-info size: levelinfosize must equal nlevels*16.
write("bad-levelinfo.ptx", header(nlevels=2, levelinfosize=16))

# H2 decompression bomb: header claims a huge inflated metadata size backed by
# only a few compressed bytes. Here the claimed metadata block also pushes the
# summed block sizes past the (tiny) file, so it is rejected before any
# allocation.
write("bomb-metadata.ptx",
      header(metadatazipsize=8, metadatamemsize=0xFFFFFFF0))

# H2 bomb via level data larger than the whole file.
write("bomb-leveldata.ptx", header(leveldatasize=0xFFFFFFFFFFFF))

# H8 bomb via an absurd face count that the (small) faceinfo block could not
# possibly hold; the summed block sizes exceed the file and are rejected.
write("bad-nfaces.ptx", header(nfaces=0x40000000, faceinfosize=20))
