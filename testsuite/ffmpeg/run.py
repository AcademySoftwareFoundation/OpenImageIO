#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

imagedir = "ref/"
files = [ "vp9_display_p3.mkv", "vp9_rec2100_pq.mkv" ]
for f in files:
    command = command + info_command (os.path.join(imagedir, f))

# Capture stderr too, so the errors from the malformed-input reads below land
# in out.txt and are checked against the ref.
redirect = " >> out.txt 2>&1 "

# Regression test for narrow 10-bit VP9 input that used to make swscale write
# past OIIO's destination buffer during pixel reads.
command = command + oiiotool ("src/ffmpeg-width1-gbrp16.mkv --hash")

# Grayscale movies. These used to be described as 3-channel even though the
# frames were requested from swscale as GRAY8/GRAY16, so every scanline copy
# ran past the end of the decoded row. Check the channel count, then read the
# pixels to make sure the copy stays in bounds. (No hash: swscale output is
# not bit-identical across FFmpeg versions.) See src/make_test_movies.py.
for f in [ "gray8.avi", "gray16.avi" ]:
    command = command + info_command ("src/" + f, verbose=False, hash=False)
    command = command + oiiotool ("src/" + f + " --hash")

# Hostile/truncated input must be rejected cleanly, with no crash and no
# over-allocation ahead of the rejection.
for f in [ "bomb-16384x9000.avi", "truncated.avi" ]:
    command = command + info_command ("src/" + f, verbose=False,
                                      failureok=True)

# A stream whose frames decode smaller than the header claims. swscale used
# to read past the end of those frames; the read must fail instead. (iinfo,
# because oiiotool's --hash exits 0 on a failed read.)
command = command + info_command ("src/resolution-change.mkv", verbose=False,
                                  failureok=True, info_program="iinfo")

# Frame indexing. Each of these is 10 frames of well-formed video: check that
# the reader says so, and that every frame decodes. (No hashes: swscale
# output is not bit-identical across FFmpeg versions, so just read the pixels
# and let a failed decode show up as an error.)
#   bframes.mp4       the decoder holds frames back, so the last ones only
#                     appear once it is flushed at the end of the stream.
#   audio-track.mkv   the audio track is longer than the video, and used to
#                     be counted as extra frames.
#   start-offset.mkv  the first frame is at t = 5 s, and the whole movie used
#                     to be undecodable.
for f in [ "bframes.mp4", "audio-track.mkv", "start-offset.mkv" ]:
    command = command + info_command ("src/" + f, verbose=False, hash=False)
    # -o so oiiotool actually reads the pixels; the .exr is not compared.
    command = command + oiiotool ("-a src/" + f + " -o " + f + ".exr")
