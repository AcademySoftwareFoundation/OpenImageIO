#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

from __future__ import annotations

import OpenImageIO as oiio


# frames.gif is written by run.py: 4 frames, each a different solid color.
filename = "frames.gif"
nframes = 4


# The color of the frame we're currently on, which is how we tell whether a
# seek landed where it said it did.
def frame_color (input) :
    pixels = input.read_image (oiio.UINT8)
    if pixels is None :
        return None
    return tuple (int (v) for v in pixels[0][0][0:3])


# Read the frames in order, to have something to compare the out-of-order
# reads against.
def reference_colors () :
    input = oiio.ImageInput.open (filename)
    colors = []
    for s in range (nframes) :
        input.seek_subimage (s, 0)
        colors.append (frame_color (input))
    input.close()
    return colors


# Reaching an earlier frame means reopening the file and replaying the ones
# before it, so seeking around is more than an index assignment.
def test_out_of_order_seeks (colors) :
    print ("Testing out of order seeks:")
    input = oiio.ImageInput.open (filename)
    for s in [2, 0, 3, 1, 3, 0] :
        ok = input.seek_subimage (s, 0)
        print ("  seek to subimage", s, "ok:", ok,
               " on subimage:", input.current_subimage(),
               " pixels match:", frame_color (input) == colors[s])
    input.close()
    print ("")


# A seek past the last frame is how the subimage count gets discovered, so it
# is an ordinary thing to do, and it must not leave the reader on the frame it
# just refused: repeating it would then take the "we're already there" early
# out and report success.
def test_seek_past_end () :
    print ("Testing seek past the last subimage:")
    input = oiio.ImageInput.open (filename)
    print ("  seek to subimage", nframes, "rejected:",
           not input.seek_subimage (nframes, 0))
    input.geterror()
    print ("  not left on that subimage:",
           input.current_subimage() != nframes)
    print ("  still rejected when repeated:",
           not input.seek_subimage (nframes, 0))
    input.geterror()
    input.close()
    print ("")


# The failed seek above read its way to the end of the file. The frames that
# are still good have to remain reachable, which means the reader can't assume
# the stream is where its subimage index says it is.
def test_seek_back_after_failure (colors) :
    print ("Testing seeks after a failed seek:")
    input = oiio.ImageInput.open (filename)
    input.seek_subimage (nframes, 0)
    input.geterror()
    for s in [1, 3, 0] :
        ok = input.seek_subimage (s, 0)
        print ("  seek to subimage", s, "ok:", ok,
               " on subimage:", input.current_subimage(),
               " pixels match:", frame_color (input) == colors[s])
    input.close()
    print ("")


try:
    colors = reference_colors()
    print ("Frames read in order all differ:", len (set (colors)) == nframes)
    print ("")
    test_out_of_order_seeks (colors)
    test_seek_past_end()
    test_seek_back_after_failure (colors)
    print ("Done.")
except Exception as detail:
    print ("Unknown exception:", detail)
