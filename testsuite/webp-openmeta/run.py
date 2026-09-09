#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

redirect = " >> out.txt 2>&1 "

command += oiiotool("--create 1x1 3 -d uint8 -o base.webp")
command += run_app(pythonbin + " src/make-openmeta-webp.py", silent=True)
command += oiiotool(
    'openmeta-metadata.webp '
    '--echo "TIFF Make={TOP.Make}" '
    '--echo "TIFF Orientation={TOP.Orientation}" '
    '--echo "TIFF aperture={TOP.\'Exif:ApertureValue\'}" '
    '--echo "TIFF XMP rating={TOP.\'XMP:Rating\'}" '
    '--echo "TIFF XMP aperture={TOP.\'xmp:http://ns.adobe.com/exif/1.0/:ApertureValue\'}" '
    '--echo "TIFF document={TOP.\'xmp:http://ns.adobe.com/xap/1.0/mm/:DocumentID\'}" '
    '--echo "TIFF history={TOP.\'xmp:http://ns.adobe.com/xap/1.0/mm/:History[1]/stEvt:action\'}" '
    '--echo "TIFF ColorSpace={TOP.\'oiio:ColorSpace\'}"'
)
command += oiiotool(
    'openmeta-prefixed-metadata.webp '
    '--echo "Prefixed Make={TOP.Make}" '
    '--echo "Prefixed Orientation={TOP.Orientation}" '
    '--echo "Prefixed aperture={TOP.\'Exif:ApertureValue\'}" '
    '--echo "Prefixed XMP rating={TOP.\'XMP:Rating\'}" '
    '--echo "Prefixed XMP aperture={TOP.\'xmp:http://ns.adobe.com/exif/1.0/:ApertureValue\'}" '
    '--echo "Prefixed document={TOP.\'xmp:http://ns.adobe.com/xap/1.0/mm/:DocumentID\'}" '
    '--echo "Prefixed history={TOP.\'xmp:http://ns.adobe.com/xap/1.0/mm/:History[1]/stEvt:action\'}" '
    '--echo "Prefixed ColorSpace={TOP.\'oiio:ColorSpace\'}"'
)
