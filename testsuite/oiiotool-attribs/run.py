#!/usr/bin/env python

# Test oiiotool's ability to add and delete attributes


# Test --eraseattrib ability to erase one specific attribute
# The result should not have "Make"
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg --eraseattrib Make -o nomake.jpg")
command += info_command ("nomake.jpg", safematch=True)

# Test --eraseattrib ability to match patterns
# The result should have no GPS tags
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg --eraseattrib \"GPS:.*\" -o nogps.jpg")
command += info_command ("nogps.jpg", safematch=True)

# Test --eraseattrib ability to strip all attribs
# The result should be very minimal
command += oiiotool (OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg --eraseattrib \".*\" -o noattribs.jpg")
command += info_command ("noattribs.jpg", safematch=True)

# Test adding and erasing attribs to multiple subimages
command += oiiotool ("--create 64x64 3 -dup --siappend " +
                     "--attrib:allsubimages=1 foo bar -d half -o attrib2.exr")
command += info_command ("attrib2.exr", safematch=True)
command += oiiotool ("attrib2.exr " +
                     "--eraseattrib:allsubimages=1 foo -d half -o attrib0.exr")
command += info_command ("attrib0.exr", safematch=True)
