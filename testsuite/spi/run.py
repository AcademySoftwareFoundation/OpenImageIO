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


outputs = [ "out.txt"]
