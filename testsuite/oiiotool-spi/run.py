#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = OIIO_TESTSUITE_IMAGEDIR
refdir = imagedir + "/ref/"
refdirlist = [ "ref/", refdir ]
outputs = [ ]


# Define a handy function that runs an oiiotool command, and
# also diffs the result against a reference image.
def oiiotool_and_test (inputfile, ops, outputfile, precommand="") :
    cmd = oiiotool (precommand + " " + imagedir + "/" + inputfile +
                    " " + ops + " -o " + outputfile)
    outputs.append (outputfile)
    return cmd



# Test fit with pad on DPX
command += oiiotool_and_test ("testFullFrame_2kfa_lg10.0006.dpx",
                              "--fit:pad=1 512x512", "fit_lg10.dpx")

# Conversion of linear half exr to vd16 uint16 TIFF
# at very high resolution used for marketing stills.
command += oiiotool_and_test ("mkt019_comp_wayn_fullres_s3d_lf_v51_misc_lnh.1001.exr",
                              "--croptofull --colorconvert:unpremult=1 lnh vd16 --ch R,G,B,A -d uint16",
                              "mkt019_comp_wayn_fullres_s3d_lf_v51_alpha_misc_vd16.1001.tif",
                              precommand = "--colorconfig " + imagedir + "/ht2.ocio/config.ocio")

# Test fit/cut on JPEG
command += oiiotool_and_test ("ffr0830_avid_ref_v3_hd_ref8.1024.jpg",
                              "--fit 2154x0 --cut 2154x1137+0+38 --cut 2154x1136",
                              "ffr0830_avid_ref_match_v3_2kdcip_ref8.1024.jpg")

# Test fit + color conversion + DPX->JPEG
# N.B. 
command += oiiotool_and_test ("ep0400_bg1_v101_3kalxog_alogc16.1001.dpx",
                              "--fit 1028x662 --colorconvert alogc16 vd8",
                              "ep0400-v2_bg1_v101_1kalxog_vd8.1001.jpg",
                              precommand = "--colorconfig " + imagedir + "/pxl.ocio/config.ocio")

# Test ociofiletransform
command += oiiotool_and_test ("os0225_110_lightingfix_v002.0101.dpx",
                              "--colorconvert lm10 lnf --ociofiletransform srgb_look.csp --colorconvert lnf vd8 -d uint8",
                              "os0225_110_lightingfix_v002.0101.png",
                              precommand = "--colorconfig " + imagedir + "/os4.ocio/config.ocio")

# Test read of iff
command += oiiotool_and_test ("iff/iff_vd8.1001.iff",
                              "",
                              "./iff_vd8.1001.iff")
command += info_command ("iff_vd8.1001.iff")


# Regression test on dealing with DPX with overscan
# REMOVED -- DPX spec doesn't support overscan!
#command += oiiotool_and_test ("dpxoverscan_hg0700_fg1_v2_2kdciufa_lg16.1014.dpx",
#                              "--iscolorspace lg16 --crop -2,0,2401,911 --fullpixels",
#                              "dpxoverscan_lg16.dpx")

outputs += [ "out.txt" ]
