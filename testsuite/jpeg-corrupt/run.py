#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


failureok = 1
redirect = ' >> out.txt 2>&1 '

command += oiiotool("--create 1x1 3 -d uint8 -o base-short-marker.jpg")
command += run_app(pythonbin + " src/make-short-marker-jpegs.py", silent=True)


# This file has a corrupted Exif block in the metadata. It used to
# crash on some platforms, on others would be caught by address sanitizer.
# Fixed by #1635. This test serves to guard against regressions.
command += info_command ("src/corrupt-exif.jpg", safematch=True)

# This file has a corrupted Exif block that makes it look like one item has a
# nonsensical length, that before being fixed, caused a buffer overrun.
command += info_command ("src/corrupt-exif-1626.jpg", safematch=True)

# This file's Exif block has an ExifIFD pointer whose offset lands one byte
# before the end of the Exif buffer. The shared decoder's recursive IFD branch
# read a 2-byte directory count there; before being fixed it ran one byte past
# the buffer (heap OOB read). Must now be dropped cleanly.
command += info_command ("src/corrupt-exif-recursive-ifd.jpg", safematch=True)

# This file's Exif block declares a tag with TIFF data type 129, the Exif 3.0
# UTF-8 type. tiff_data_size() didn't know that type and reported size_t(-1),
# which wrapped the shared decoder's bounds arithmetic into a span of length
# size_t(-1): a heap OOB read under a sanitizer, and an uncaught
# std::length_error that aborted the process on a release build.
command += info_command ("src/corrupt-exif-utf8-type.jpg", safematch=True)

# This file's Exif block chains 100 ExifIFD pointers, each aimed at the next.
# Every level costs a stack frame, so a large enough chain exhausts the stack;
# the decoder now stops descending after a fixed depth.
command += info_command ("src/corrupt-exif-deep-ifds.jpg", safematch=True)

# This file has a corrupted ICC profile block that has tags that say they
# extend beyond the boundaries of the ICC block itself.
command += run_app (oiiotool("--echo corrupt-icc-4551.jpg"))
command += run_app (oiio_app("iconvert") + " src/corrupt-icc-4551.jpg out-4551.jpg")

# This file has a corrupted ICC profile block
command += info_command ("src/corrupt-icc-4552.jpg", safematch=True)

# These files have short APP1/APP2 metadata marker payloads that used to be
# read past their saved-marker buffers before being ignored. Use iconvert to
# a null output to force a full input read.
command += iconvert("short-exif-app1-len4.jpg out.null",
                    successmessage="short-exif-app1-len4-ok")
command += iconvert("short-exif-app1-len5.jpg out.null",
                    successmessage="short-exif-app1-len5-ok")
command += iconvert("short-icc-app2-len11.jpg out.null",
                    successmessage="short-icc-app2-len11-ok")
command += iconvert("short-icc-app2-len12.jpg out.null",
                    successmessage="short-icc-app2-len12-ok")
command += iconvert("short-icc-app2-len13.jpg out.null",
                    successmessage="short-icc-app2-len13-ok")

# This file had corrupted IPTC data
command += oiiotool("-oiioattrib imageinput:strict 1 -info -v src/corrupt-iptc-8011.jpg")

# This file's SOF0 header declares a ~12 GB image (65500x65500x3) but holds
# only a few bytes of entropy-coded data: a classic decompression bomb. The
# reader's compression-ratio guard must reject it cleanly with a bounded
# allocation rather than attempting the enormous pixel allocation.
command += info_command("src/bomb-65500x65500.jpg", failureok=True, safematch=True)

# Same bomb, but progressive (SOF2). Progressive decoding allocates a
# whole-image coefficient array inside jpeg_start_decompress, so the size
# checks must happen right after jpeg_read_header, not after decompression
# has been started.
command += info_command("src/bomb-progressive-65500x65500.jpg", failureok=True,
                        safematch=True)
