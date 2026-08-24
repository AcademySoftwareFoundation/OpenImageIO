#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


redirect = ' >> out.txt 2>&1 '

# Test handling of colorInteropID in multi-part files.

# Parts: "lin_ap1_scene", missing, "data", missing
command += oiiotool("--pattern constant:color=1,0,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename beauty "
                    "--pattern constant:color=0,1,0 4x4 3 -d half "
                    "--eraseattrib oiio:ColorSpace --attrib oiio:subimagename diffuse "
                    "--pattern constant:color=0.25,0.25,0.25 4x4 3 -d half "
                    "--attrib oiio:ColorSpace data --attrib oiio:subimagename depth "
                    "--pattern constant:color=0,0,1 4x4 3 -d half "
                    "--eraseattrib oiio:ColorSpace --attrib oiio:subimagename specular "
                    "--siappendall -o typical.exr")
command += info_command("typical.exr",
                        extraargs="-oiioattrib openexr:core 0", safematch=True)
command += info_command("typical.exr",
                        extraargs="-oiioattrib openexr:core 1", safematch=True)

# Parts: missing, "data", missing
command += oiiotool("--pattern constant:color=1,0,0 4x4 3 -d half "
                    "--eraseattrib oiio:ColorSpace --attrib oiio:subimagename beauty "
                    "--pattern constant:color=0.25,0.25,0.25 4x4 3 -d half "
                    "--attrib oiio:ColorSpace data --attrib oiio:subimagename depth "
                    "--pattern constant:color=0,0,1 4x4 3 -d half "
                    "--eraseattrib oiio:ColorSpace --attrib oiio:subimagename specular "
                    "--siappendall -o typical_nocolorspace.exr")
command += info_command("typical_nocolorspace.exr",
                        extraargs="-oiioattrib openexr:core 0", safematch=True)
command += info_command("typical_nocolorspace.exr",
                        extraargs="-oiioattrib openexr:core 1", safematch=True)

# Parts: "data", missing, "lin_ap1_scene", missing
# Not valid according to the CIF recommendation, but can be read anyway.
command += info_command("src/multipart_colorspace_data_first.exr",
                        extraargs="-oiioattrib openexr:core 0", safematch=True)
command += info_command("src/multipart_colorspace_data_first.exr",
                        extraargs="-oiioattrib openexr:core 1", safematch=True)

# Parts: "lin_ap1_scene", "lin_ap1_scene"
command += oiiotool("--pattern constant:color=1,0,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename beauty "
                    "--pattern constant:color=0,1,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename specular "
                    "--siappendall -o datafirst.exr", failureok=True)

# Parts: "lin_ap1_scene", missing
command += oiiotool("--pattern constant:color=1,0,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename beauty "
                    "--pattern constant:color=0,1,0 4x4 3 -d half "
                    "--attrib oiio:subimagename specular "
                    "--siappendall -o datafirst.exr", failureok=True)

# Parts: "lin_ap1_scene", "lin_ap1_scene"
# Not valid according to the CIF recommendation, error on write.
command += oiiotool("--pattern constant:color=1,0,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename beauty "
                    "--pattern constant:color=0,1,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_rec709_scene --attrib oiio:subimagename specular "
                    "--siappendall -o datafirst.exr", failureok=True)

# Parts: "data", "lin_ap1_scene", "lin_ap1_scene"
# Not valid according to the CIF recommendation, error on write.
command += oiiotool("--pattern constant:color=0.25,0.25,0.25 4x4 3 -d half "
                    "--attrib oiio:ColorSpace data --attrib oiio:subimagename depth "
                    "--pattern constant:color=1,0,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename beauty "
                    "--pattern constant:color=0,1,0 4x4 3 -d half "
                    "--attrib oiio:ColorSpace lin_ap1_scene --attrib oiio:subimagename specular "
                    "--siappendall -o datafirst.exr", failureok=True)
