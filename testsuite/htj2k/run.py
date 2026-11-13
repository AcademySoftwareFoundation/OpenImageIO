#!/usr/bin/env python

# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


# These tests are checking the openjph library that can optionally be compiled into the Jpeg2000
# plugin of OIIO. If the library is not enabled, these will fail.

# Capture stderr too, so the rejection messages below are checked.
redirect = ' >> out.txt 2>&1 '

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " -o test.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "test.j2c")

command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/dpx/dpx_nuke_10bits_rgb.dpx"
                    " -o testdpx.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/dpx/dpx_nuke_10bits_rgb.dpx", "testdpx.j2c")

# Tiled output is emulated by buffering, and must still be written on close.
command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " --tile 64 64 -o testtiled.j2c")
command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "testtiled.j2c")


command += oiiotool(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg"
                    " --attrib qstep 0.03 -o testcompress.j2c")

command += diff_command(OIIO_TESTSUITE_IMAGEDIR+"/tahoe-gps.jpg", "testcompress.j2c", extraargs="-fail 0.11")

# Malformed HTJ2K headers, built by src/make_malformed_htj2k.py. The HTJ2K
# reader allocates the whole image up front from the SIZ dimensions, so an
# oversized claim has to be rejected at open time rather than at allocation.
command += oiiotool("--info -v src/bomb-resolution.j2c", failureok = True)
command += oiiotool("--info -v src/bomb-ratio.j2c", failureok = True)
# Small images, but thousands of tiny tiles, tiny codeblocks or precincts, or
# resolutions a DFS marker keeps full size, that bloat OpenJPH's allocations.
# The reader only checks for this when built with OpenJPH >= 0.31.
def get_openjph_ver():
    deps = (subprocess.check_output([oiio_app('oiiotool').strip(),
                                    '--echo', '{getattribute(build:dependencies)}'])
            .strip().decode('utf-8'))
    for elem in deps.split(";"):
        if elem.startswith("openjph "):
            try:
                major, minor, patch = (int(x) for x in elem.split()[-1].split("."))
                return (major * 10000) + (minor * 100) + patch
            except Exception:
                pass
    return -1

if get_openjph_ver() >= 3100:
    redirect_push("out-tiles.txt")
    redirect += "2>&1 "
    command += oiiotool("--info -v src/bomb-tiles.j2c", failureok = True)
    command += oiiotool("--info -v src/bomb-tiny-tiles.j2c", failureok = True)
    command += oiiotool("--info -v src/bomb-narrow-blocks.j2c", failureok = True)
    command += oiiotool("--info -v src/bomb-tiny-precincts.j2c", failureok = True)
    command += oiiotool("--info -v src/bomb-wide-row.j2c", failureok = True)
    command += oiiotool("--info -v src/bomb-dfs.j2c", failureok = True)
    command += oiiotool("--info -v src/bomb-tall-blocks.j2c", failureok = True)
    redirect_pop()

# ... while a small valid codestream still reads.
command += oiiotool("--info -v --hash src/valid-16x16.j2c", failureok = True)

# Subsampled components, built by src/make_subsampled_htj2k.cpp. The reader
# used to assume every component was full resolution and hung on these.
# 4:2:0 at even and odd sizes.
command += oiiotool("--info -v --hash src/valid-420.j2c", failureok = True)
command += oiiotool("--info -v --hash src/valid-420-odd.j2c", failureok = True)
# With a color transform (decoded in interleaved order), alone and with
# tiles and offsets.
command += oiiotool("--info -v --hash src/valid-420-mct.j2c", failureok = True)
command += oiiotool("--info -v --hash src/valid-420-mct-tiled.j2c",
                    failureok = True)
# A subsampled first component with an image offset not a multiple of it.
command += oiiotool("--info -v --hash src/valid-subsampled-offset.j2c",
                    failureok = True)
# 10-bit, tiles, both offsets and non-power-of-2 factors at once.
command += oiiotool("--info -v --hash src/valid-tiled-offset-10bit.j2c",
                    failureok = True)
# Four components with a differently subsampled alpha. Keep the alpha
# unassociated so the hash covers the decoded samples, not a premultiply.
command += oiiotool("--iconfig oiio:UnassociatedAlpha 1 --info -v --hash"
                    " src/valid-rgba-subsampled.j2c", failureok = True)
# Five components with a color transform: only four are kept, but the
# interleaved decode must still pull a row of every component.
command += oiiotool("--iconfig oiio:UnassociatedAlpha 1 --info -v --hash"
                    " src/valid-5comp-mct.j2c", failureok = True)
# An empty component ahead of a nonempty one used to hang.
command += oiiotool("--info -v src/empty-component.j2c", failureok = True)
# Tiles smaller than the subsampling that each still hold a sample, and tile
# rows holding no sample of a component, decode with every OpenJPH.
command += oiiotool("--info -v --hash src/valid-tiles-under-subsampling.j2c",
                    failureok = True)
command += oiiotool("--info -v --hash src/valid-empty-tile-rows.j2c",
                    failureok = True)
# Tiles narrower than a component's subsampling leave it with no columns in
# some tiles, which crashes OpenJPH before 0.27, so the reader rejects such
# files there; later versions decode them. The corrupt one is the fuzzer
# find that used to hang. Both outcomes are checked against their own ref
# (out-emptytilecols*.txt).
redirect_push("out-emptytilecols.txt")
redirect += "2>&1 "
command += oiiotool("--info -v --hash src/valid-empty-tile-cols.j2c",
                    failureok = True)
if get_openjph_ver() >= 2700:
    # Decoding the corrupt file must succeed (no failureok, so a crash fails
    # the test). It prints only an OpenJPH "info" line about the truncated
    # data, whose source line number varies by version, so capture the log
    # and filter that line out separately, expecting nothing left.
    command += oiiotool("src/subsampled-hang.j2c -o hang.tif > hang.txt 2>&1",
                        silent = True)
    command += run_app("grep -v \"ojph info\" hang.txt", failureok = True)
else:
    command += oiiotool("src/subsampled-hang.j2c -o hang.tif", failureok = True)
redirect_pop()

# Reading a valid HTJ2K file and then a malformed one in the same process
# used to crash: OpenJPH's error handler is global, and the old code left it
# pointing at a stack object from the first open(). The second open's error
# report then dereferenced freed stack. Both files must open in one process.
# Filter to the stable lines -- the rejection message itself comes from
# OpenJPEG's fallback attempt and its wording varies by library version.
# Use repeated -e rather than an alternation regex: cmd.exe on Windows CI
# treats the '|' inside 'Reading|jpeg2000|...' as a shell pipe. Quote the
# pattern with double quotes for the same reason -- cmd.exe does not treat
# single quotes as quoting, so grep would see two arguments.
command += oiiotool("--info -v src/valid-16x16.j2c src/stale-error-handler.j2c"
                    " 2>&1 | grep -e Reading -e jpeg2000 -e \"Full command\"",
                    failureok = True)
