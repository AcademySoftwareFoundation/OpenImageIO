#!/usr/bin/env python3
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Generate the small movie fixtures used by the ffmpeg regression tests.
# Requires the `ffmpeg` command line tool; the generated files are committed,
# so this only needs to be re-run if a fixture changes.
#
#   gray8.avi            valid 8-bit grayscale movie
#   gray16.avi           valid 16-bit grayscale movie
#   bomb-16384x9000.avi  tiny file whose header declares a 1.1 GB frame
#   truncated.avi        valid header followed by a chopped-off stream
#   resolution-change.mkv  frames smaller than the size the header declares
#   bframes.mp4          h264 with B-frames, so the decoder holds frames back
#   audio-track.mkv      video plus a longer audio track
#   start-offset.mkv     stream whose first frame is at t = 5 seconds
#
# The two grayscale movies are the regression case for a heap overread: the
# reader asked FFmpeg for GRAY8/GRAY16 frames but described them in the
# ImageSpec as 3-channel, so each scanline copy ran three times past the end
# of the decoded row.
#
# AVI keeps the frame dimensions in two places, the main header (avih) and
# the stream format header (strf, a BITMAPINFOHEADER). Patching both is
# enough to make a small file claim an arbitrary frame size, since ffv1 takes
# its dimensions from the container.

import os
import struct
import subprocess
import sys


def ffmpeg(pix_fmt, size, out):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i",
                    "testsrc=size=%s:rate=1:duration=1" % size,
                    "-pix_fmt", pix_fmt, "-c:v", "ffv1", "-frames:v", "1",
                    out], check=True)


def patch_dimensions(src, dst, width, height):
    d = bytearray(open(src, "rb").read())
    avih = d.find(b"avih")
    strf = d.find(b"strf")
    if avih < 0 or strf < 0:
        sys.exit("could not locate AVI headers in " + src)
    # avih: dwWidth/dwHeight are the 9th and 10th uint32 of the structure.
    d[avih + 40:avih + 44] = struct.pack("<i", width)
    d[avih + 44:avih + 48] = struct.pack("<i", height)
    # strf: biWidth/biHeight follow biSize.
    d[strf + 12:strf + 16] = struct.pack("<i", width)
    d[strf + 16:strf + 20] = struct.pack("<i", height)
    open(dst, "wb").write(bytes(d))


ffmpeg("gray", "128x64", "gray8.avi")
ffmpeg("gray16le", "128x64", "gray16.avi")

# 16384x9000 x 4 chan x 16 bits = 1125 MB claimed by a ~6 KB file. The
# dimensions are deliberately under FFmpeg's own ~2.7e8 pixel ceiling, so
# nothing upstream of OIIO rejects them.
ffmpeg("gbrap16le", "16x16", "bomb-16384x9000.avi")
patch_dimensions("bomb-16384x9000.avi", "bomb-16384x9000.avi", 16384, 9000)

ffmpeg("gray", "128x64", "truncated.avi")
d = open("truncated.avi", "rb").read()
open("truncated.avi", "wb").write(d[:len(d) // 3])

# Two Annex B h264 sequences at different resolutions, concatenated. The
# Matroska header describes the larger one, so the first frames decode
# smaller than the header claims and swscale would read past the end of them.
for size, out in [("32x32", "small.h264"), ("320x240", "large.h264")]:
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i",
                    "color=c=gray:size=%s:rate=25:duration=0.08" % size,
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-g", "1",
                    "-crf", "51", "-frames:v", "2", "-bsf:v",
                    "h264_mp4toannexb", "-f", "h264", out], check=True)
with open("both.h264", "wb") as f:
    f.write(open("small.h264", "rb").read() + open("large.h264", "rb").read())
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-fflags", "+genpts",
                "-r", "25", "-f", "h264", "-i", "both.h264", "-c", "copy",
                "resolution-change.mkv"], check=True)
for f in ["small.h264", "large.h264", "both.h264"]:
    os.remove(f)


# Well-formed movies that exercise the frame indexing, rather than the
# handling of hostile input. Each is 10 frames of 32x32.

# libx264 with B-frames gives the decoder a reorder delay, so the last frames
# only come out after the decoder is flushed at the end of the stream.
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i",
                "testsrc=size=32x32:rate=10:duration=1", "-c:v", "libx264",
                "-pix_fmt", "yuv420p", "-bf", "3", "-g", "30", "-crf", "40",
                "bframes.mp4"], check=True)

# 1 second of video and 3 seconds of audio. Matroska stores no frame count,
# so the reader has to derive one from the packet timestamps, and the audio
# packets must not be allowed to contribute.
subprocess.run(["ffmpeg", "-y", "-loglevel", "error",
                "-f", "lavfi", "-i", "testsrc=size=32x32:rate=10:duration=1",
                "-f", "lavfi", "-i",
                "sine=frequency=440:duration=3:sample_rate=8000",
                "-c:v", "ffv1", "-c:a", "aac", "-b:a", "8k", "-ac", "1",
                "audio-track.mkv"], check=True)

# A nonzero stream start time, as an mp4 edit list or a live capture would
# produce. Frame 0 is at t = 5 s, not t = 0.
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-f", "lavfi", "-i",
                "testsrc=size=32x32:rate=10:duration=1", "-c:v", "ffv1",
                "-output_ts_offset", "5", "-muxdelay", "0",
                "start-offset.mkv"], check=True)
