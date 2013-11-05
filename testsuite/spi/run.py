#!/usr/bin/python 

imagedir = parent + "/spi-oiio-tests/"
refdir = imagedir + "ref/"


def oiiotool_and_test (inputfile, ops, outputfile) :
    cmd = oiiotool (imagedir + inputfile + " " + ops + " -o " + outputfile)
    cmd += diff_command (outputfile, refdir+outputfile)
    return cmd


# Test fit
command += oiiotool_and_test ("testFullFrame_2kfa_lg10.0006.dpx",
                              "--fit:pad=1 512x512", "fit_lg10.dpx")

# Regression test on dealing with DPX with overscan
command += oiiotool_and_test ("dpxoverscan_hg0700_fg1_v2_2kdciufa_lg16.1014.dpx",
                              "--iscolorspace lg16 --crop -2,0,2401,911 --fullpixels",
                              "dpxoverscan_lg16.dpx")


outputs = [ "out.txt"]
