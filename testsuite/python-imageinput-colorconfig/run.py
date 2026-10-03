#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Write one file per format whose reader sets "oiio:ColorSpace", tagged so
# that the reader's color code runs. Writing may load the color config, so it
# happens here in oiiotool, and the reading in a fresh Python process.
files = []
for ext, nchans, attribs in [
        ("exr", 3, "-attrib oiio:ColorSpace lin_rec709_scene"),
        ("png", 4, "-attrib oiio:ColorSpace g22_rec709_scene"),
        ("dpx", 3, "-attrib oiio:ColorSpace KodakLog"),
        ("tga", 3, "-attrib oiio:ColorSpace g22_rec709_scene"),
        ("rla", 3, "-attrib oiio:ColorSpace g22_rec709_scene"),
        ("hdr", 3, ""),
        ("jpg", 3, "-attrib Exif:ColorSpace 1"),
        ("tif", 3, "-attrib Exif:ColorSpace 1"),
        ("bmp", 3, ""),
        ("ppm", 3, ""),
        ("gif", 3, ""),
        ("webp", 3, ""),
        ("jp2", 3, ""),
        ("jxl", 3, "-attrib oiio:ColorSpace srgb_rec709_scene"),
        ("avif", 3, "") ]:
    # Formats with optional dependencies may lack a writer; the test script
    # skips a file whose format has no reader.
    files += [ "colorconfig." + ext ]
    command += oiiotool (f"-pattern constant:color=0.5,0.5,0.5,0.5 32x32 {nchans} {attribs} -o colorconfig.{ext}",
                         failureok=True)
# A PNG with a cICP chunk.
files += [ "colorconfig-cicp.png" ]
command += oiiotool ("-pattern constant:color=0.5,0.5,0.5 32x32 3 -attrib oiio:ColorSpace pq_rec2020_display -o colorconfig-cicp.png")
# An ACES container OpenEXR.
files += [ "colorconfig-aces.exr" ]
command += oiiotool ("--create 32x32 3 -d half --compression none -sattrib openexr:ACESContainerPolicy strict -o colorconfig-aces.exr")
files += [ "colorconfig.ico",  # made from colorconfig.png by the script
           # Parts: "data", missing, "lin_ap1_scene", missing
           OIIO_TESTSUITE_ROOT + "/openexr-multipart-colorspace/src/multipart_colorspace_data_first.exr",
           OIIO_TESTSUITE_IMAGEDIR + "/cineon/checker.cin",
           OIIO_TESTSUITE_IMAGEDIR + "/dds/dds_dxgi_bc1_srgb.dds",
           OIIO_TESTSUITE_IMAGEDIR + "/dds/dds_bc6hu_hdr.dds",
           OIIO_TESTSUITE_IMAGEDIR + "/heif/sewing-threads.heic",
           OIIO_TESTSUITE_IMAGEDIR + "/raw/RAW_CANON_EOS_7D.CR2",
           OIIO_TESTSUITE_ROOT + "/psd/src/Layers_8bit_RGB.psd",
           OIIO_TESTSUITE_ROOT + "/ffmpeg/src/bframes.mp4" ]

command += pythonbin + " src/test_imageinput_colorconfig.py " + " ".join(files) + " > out.txt"
