#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = ' >> out.txt 2>&1 '

# This file was rendered and saved in Blender, and therefore contains metadata
# in the form of comments.

# Check if the comments are correctly decoded as attributes, and that writing
# to a new JPEG does not include them by default.
command += rw_command ("src", "blender-render.jpg", use_oiiotool=1,
                       output_filename="no-attribs.jpg")
command += info_command ("no-attribs.jpg", safematch=True)

# Check that, when jpeg:com_attributes is set, the attributes are preserved.
command += rw_command ("src", "blender-render.jpg", use_oiiotool=1,
                       output_filename="with-attribs.jpg",
                       extraargs="--attrib:type=int jpeg:com_attributes 1")
command += info_command ("with-attribs.jpg", safematch=True)

# Check that JPEG comments that don't match an attribute will be read as ImageDescription.
command += rw_command ("src", "blender-render.jpg", use_oiiotool=1,
                       output_filename="with-attribs-and-desc.jpg",
                       extraargs="--attrib:type=int jpeg:com_attributes 1 "
                       "--attrib:type=string ImageDescription \"A photo\"")
command += info_command ("with-attribs-and-desc.jpg", safematch=True)

# Check that JPEG comments that would match an attribute will be read as ImageDescription
# if jpeg:com_attributes is 0.
command += rw_command ("src", "blender-render.jpg", use_oiiotool=1,
                       output_filename="with-colon-desc.jpg",
                       extraargs="--attrib:type=string ImageDescription \"Example:Text\"")
command += info_command ("with-colon-desc.jpg", safematch=True,
                         extraargs="--oiioattrib:type=int jpeg:com_attributes 0")
