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
