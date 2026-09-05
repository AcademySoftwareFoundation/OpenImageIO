#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = "ref/"
files = [ "IMG_7702_small.heic", "Chimera-AV1-8bit-162.avif", "test-10bit.avif" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))

command += oiiotool (os.path.join(imagedir, "test-10bit.avif") +
                     " -d uint10 --cicp \"9,16,9,1\" -o cicp_pq.avif" )
command += info_command ("cicp_pq.avif", safematch=True)


command += oiiotool (os.path.join(imagedir, "test-10bit.avif") +
                     " -d uint10 --attrib oiio:ColorSpace hlg_rec2020_display -o colorspace_hlg.avif" )
command += info_command ("colorspace_hlg.avif", safematch=True)

files = [ "greyhounds-looking-for-a-table.heic", "sewing-threads.heic" ]
for f in files:
    command = command + info_command (os.path.join(OIIO_TESTSUITE_IMAGEDIR, f))

command += oiiotool("--pattern checker:color1=1:color2=0 64x64 1 -o mono-8bit.avif")
command += info_command("mono-8bit.avif", safematch=True)

command += oiiotool("--pattern checker:color1=1:color2=0 64x64 1 -d uint10 -o mono-10bit.avif")
command += info_command("mono-10bit.avif", safematch=True)

# Test non-multiple-of-64 dimensions
command += oiiotool("--pattern fill:color=0.5,0.5,0.5 47x31 3 -o odd-size.avif")
command += info_command("odd-size.avif", safematch=True)

# Test reading an irot-transformed image with oiio:reorient=0, which asks
# for the raw stored pixels without applying the transformation. The spec
# must describe the stored (unrotated) plane, the orientation must be
# reported via the "Orientation" attribute, and rotating the raw pixels
# back must reproduce the default auto-reoriented read exactly.
rotated = make_relpath(os.path.join(imagedir, "rotated-90cw.heic"))
reorient_redirect = " >> out-reorient.txt "
command += oiio_app("oiiotool") + " --info -v --iconfig oiio:reorient 0 " + rotated + reorient_redirect + ";\n"
command += oiio_app("oiiotool") + " --iconfig oiio:reorient 0 " + rotated + " -d uint8 -o reorient0.tif ;\n"
command += oiio_app("oiiotool") + " " + rotated + " -d uint8 -o oriented.tif ;\n"
command += oiio_app("oiiotool") + " reorient0.tif --rotate90 -o reorient0-rotated.tif ;\n"
command += oiio_app("oiiotool") + " --info -v --no-metamatch \"DateTime|Software|ImageHistory\" reorient0.tif" + reorient_redirect + ";\n"
command += oiio_app("oiiotool") + " reorient0-rotated.tif oriented.tif --diff" + reorient_redirect + ";\n"
outputs += [ "out-reorient.txt" ]

# avif conversion is expected to fail if libheif is built without AV1 support
failureok = 1
