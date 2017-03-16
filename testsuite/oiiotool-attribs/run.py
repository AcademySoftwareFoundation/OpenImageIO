#!/usr/bin/env python

# Test oiiotool's ability to add and delete attributes


# Test --eraseattrib ability to erase one specific attribute
# The result should not have "Make"
command += oiiotool (parent+"/oiio-images/tahoe-gps.jpg --eraseattrib Make -o nomake.jpg")
command += info_command ("nomake.jpg", safematch=True)

# Test --eraseattrib ability to match patterns
# The result should have no GPS tags
command += oiiotool (parent+"/oiio-images/tahoe-gps.jpg --eraseattrib \"GPS:.*\" -o nogps.jpg")
command += info_command ("nogps.jpg", safematch=True)

# Test --eraseattrib ability to strip all attribs
# The result should be very minimal
command += oiiotool (parent+"/oiio-images/tahoe-gps.jpg --eraseattrib \".*\" -o noattribs.jpg")
command += info_command ("noattribs.jpg", safematch=True)


